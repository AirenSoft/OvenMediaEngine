//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http2_stream.h"

#include "../http_connection.h"
#include "../../http_private.h"

namespace http
{
	namespace svr
	{
		namespace h2
		{
			HttpStream::HttpStream(const std::shared_ptr<HttpConnection> &connection, uint32_t stream_id)
				: HttpExchange(connection), _stream_id(stream_id)
			{
				_request = std::make_shared<Http2Request>(GetConnection()->GetSocket(), GetConnection()->GetHpackDecoder());
				_request->SetConnectionType(ConnectionType::Http20);
				_request->SetTlsData(GetConnection()->GetTlsData());

				_response = std::make_shared<Http2Response>(stream_id, GetConnection()->GetSocket(), GetConnection()->GetHpackEncoder());
				_response->SetTlsData(GetConnection()->GetTlsData());
				_response->SetHeader("server", "OvenMediaEngine");
				_response->SetHeader("content-type", "text/html");

				// https://www.rfc-editor.org/rfc/rfc7540.html#section-5.1.1
				// A stream identifier of zero (0x0) is used for connection control messages
				if (_stream_id == 0)
				{
					SendInitialControlMessage();
				}
			}

			std::shared_ptr<HttpRequest> HttpStream::GetRequest() const
			{
				return _request;
			}
			std::shared_ptr<HttpResponse> HttpStream::GetResponse() const
			{
				return _response;
			}

			// Get Stream ID
			uint32_t HttpStream::GetStreamId() const
			{
				return _stream_id;
			}

			bool HttpStream::OnEndHeaders()
			{
				// Header Completed
				if (_request->AppendHeaderData(_header_block) <= 0)
				{
					return false;
				}

				if (IsUpgradeRequest() == true)
				{
					if (AcceptUpgrade() == false)
					{
						SetStatus(Status::Error);
						return -1;
					}

					SetStatus(Status::Upgrade);
					return true;
				}

				// HTTP/2 Connection is awalys keep-alive
				SetKeepAlive(true);

				// Notify to interceptor
				if (OnRequestPrepared() == false)
				{
					return -1;
				}

				if (_headers_frame->IS_HTTP2_FRAME_FLAG_ON(Http2HeadersFrame::Flags::EndStream))
				{
					return OnEndStream();
				}
				else
				{
					// Continue to receive more data
					SetStatus(Status::Exchanging);
				}

				return true;
			}

			bool HttpStream::OnEndStream()
			{
				// End of Stream, that means no more data will be sent
				auto result = OnRequestCompleted();
				switch (result)
				{
					case InterceptorResult::Completed:
						SetStatus(Status::Completed);
						break;
					case InterceptorResult::Moved:
						SetStatus(Status::Moved);
						break;
					case InterceptorResult::Error:
					default:
						SetStatus(Status::Error);
						return false;
				}

				return true;
			}

			bool HttpStream::SendInitialControlMessage()
			{
				logtd("Send Initial Control Message");
				// Settings Frame
				auto settings_frame = std::make_shared<Http2SettingsFrame>();
				// Max decoder table size, encoder(client) will use this value for encoder and notify by DecodeDynamicTableSizeUpdate in HPACK
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::HeaderTableSize, MAX_HEADER_TABLE_SIZE); 
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::MaxConcurrentStreams, 100);
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::InitialWindowSize, 6291456);
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::MaxHeaderListSize, 262144);

				auto result = _response->Send(settings_frame);

				// WindowUpdate Frame
				auto window_update_frame = std::make_shared<Http2WindowUpdateFrame>(0);
				window_update_frame->SetWindowSizeIncrement(6291456);

				result = result ? _response->Send(window_update_frame) : false;
				
				return result;
			}

			bool HttpStream::OnFrameReceived(const std::shared_ptr<Http2Frame> &frame)
			{
				std::shared_ptr<const Http2Frame> parsed_frame = frame;
				bool result = false;
				switch (frame->GetType())
				{
					case Http2Frame::Type::Data:
					{
						parsed_frame = frame->GetFrameAs<Http2DataFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse Data frame");
							return false;
						}

						result = OnDataFrameReceived(std::static_pointer_cast<const Http2DataFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Headers:
					{
						parsed_frame = frame->GetFrameAs<Http2HeadersFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse Headers frame");
							return false;
						}

						result = OnHeadersFrameReceived(std::static_pointer_cast<const Http2HeadersFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Priority:
					{
						parsed_frame = frame->GetFrameAs<Http2PriorityFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse Priority frame");
							return false;
						}

						result = OnPriorityFrameReceived(std::static_pointer_cast<const Http2PriorityFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::RstStream:
					{
						parsed_frame = frame->GetFrameAs<Http2RstStreamFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse RstStream frame");
							return false;
						}

						result = OnRstStreamFrameReceived(std::static_pointer_cast<const Http2RstStreamFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Settings:
					{
						parsed_frame = frame->GetFrameAs<Http2SettingsFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse Settings frame");
							return false;
						}

						result = OnSettingsFrameReceived(std::static_pointer_cast<const Http2SettingsFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::PushPromise:
					{
						// No need to parse server side
						break;
					}
					case Http2Frame::Type::Ping:
					{
						parsed_frame = frame->GetFrameAs<Http2PingFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse Ping frame");
							return false;
						}

						result = OnPingFrameReceived(std::static_pointer_cast<const Http2PingFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::GoAway:
					{
						parsed_frame = frame->GetFrameAs<Http2GoAwayFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse GoAway frame");
							return false;
						}

						result = OnGoAwayFrameReceived(std::static_pointer_cast<const Http2GoAwayFrame>(parsed_frame));
					
						break;
					}
					case Http2Frame::Type::WindowUpdate:
					{
						parsed_frame = frame->GetFrameAs<Http2WindowUpdateFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse WindowUpdate frame");
							return false;
						}

						result = OnWindowUpdateFrameReceived(std::static_pointer_cast<const Http2WindowUpdateFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Continuation:
					{
						parsed_frame = frame->GetFrameAs<Http2ContinuationFrame>();
						if (parsed_frame == nullptr || parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse Continuation frame");
							return false;
						}

						result = OnContinuationFrameReceived(std::static_pointer_cast<const Http2ContinuationFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Unknown:
					default:
						logte("Unknown frame type received");
						return false;
				}

				logti("Frame Processing %s : %s", result?"Completed":"Error", parsed_frame->ToString().CStr());

				return true;
			}

			bool HttpStream::OnHeadersFrameReceived(const std::shared_ptr<const Http2HeadersFrame> &frame)
			{
				if (_header_block == nullptr)
				{
					_header_block = std::make_shared<ov::Data>();
				}

				_header_block->Append(frame->GetHeaderBlockFragment());
				_headers_frame = frame;

				if (frame->IS_HTTP2_FRAME_FLAG_ON(Http2HeadersFrame::Flags::EndHeaders))
				{
					return OnEndHeaders();
				}

				return true;
			}

			// Data frame received
			bool HttpStream::OnDataFrameReceived(const std::shared_ptr<const Http2DataFrame> &frame)
			{
				if (OnDataReceived(frame->GetData()) == false)
				{
					return false;
				}

				if (frame->IS_HTTP2_FRAME_FLAG_ON(Http2DataFrame::Flags::EndStream))
				{
					return OnEndStream();
				}

				return true;
			}

			bool HttpStream::OnPriorityFrameReceived(const std::shared_ptr<const Http2PriorityFrame> &frame)
			{
				return true;
			}

			bool HttpStream::OnRstStreamFrameReceived(const std::shared_ptr<const Http2RstStreamFrame> &frame)
			{
				return true;
			}

			bool HttpStream::OnSettingsFrameReceived(const std::shared_ptr<const Http2SettingsFrame> &frame)
			{
				if (frame->IsAck() == false)
				{
					// Apply SETTINGS_HEADER_TABLE_SIZE to HPACK encoder
					auto [exist, size] = frame->GetParameter(Http2SettingsFrame::Parameters::HeaderTableSize);
					if (exist)
					{
						auto hpack_encoder = GetConnection()->GetHpackEncoder();
						hpack_encoder->UpdateDynamicTableSize(std::min(size, MAX_HEADER_TABLE_SIZE));
					}
					
					// Settings Frame
					auto settings_frame = std::make_shared<Http2SettingsFrame>();
					settings_frame->SetAck();
					return _response->Send(settings_frame);
				}

				return true;
			}

			bool HttpStream::OnPushPromiseFrameReceived(const std::shared_ptr<const Http2PushPromiseFrame> &frame)
			{
				return true;
			}

			bool HttpStream::OnPingFrameReceived(const std::shared_ptr<const Http2PingFrame> &frame)
			{
				if (frame->IsAck() == false)
				{
					// PING Frame
					auto ping_frame = std::make_shared<Http2PingFrame>(frame->GetStreamId());
					ping_frame->SetAck();
					ping_frame->SetOpaqueData(frame->GetOpaqueData());
					return _response->Send(ping_frame);
				}

				return true;
			}

			bool HttpStream::OnWindowUpdateFrameReceived(const std::shared_ptr<const Http2WindowUpdateFrame> &frame)
			{
				return true;
			}

			bool HttpStream::OnGoAwayFrameReceived(const std::shared_ptr<const Http2GoAwayFrame> &frame)
			{
				return true;
			}

			bool HttpStream::OnContinuationFrameReceived(const std::shared_ptr<const Http2ContinuationFrame> &frame)
			{
				if (_header_block == nullptr)
				{
					// It must be created by headers frame
					return false;
				}

				_header_block->Append(frame->GetHeaderBlockFragment());

				if (frame->IS_HTTP2_FRAME_FLAG_ON(Http2ContinuationFrame::Flags::EndHeaders))
				{
					return OnEndHeaders();
				}

				return true;
			}
		}  // namespace h2
	}	   // namespace svr
}  // namespace http
