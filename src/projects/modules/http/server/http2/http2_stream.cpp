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

			bool HttpStream::OnFrameReceived(const std::shared_ptr<const Http2Frame> &frame)
			{
				std::shared_ptr<const Http2Frame> parsed_frame = frame;
				bool result = false;
				switch (frame->GetType())
				{
					case Http2Frame::Type::Data:
						break;
					case Http2Frame::Type::Headers:
					{
						parsed_frame = std::make_shared<Http2HeadersFrame>(frame);
						if (parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse settings frame");
							return false;
						}

						result = OnHeadersFrameReceived(std::static_pointer_cast<const Http2HeadersFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Priority:
						break;
					case Http2Frame::Type::RstStream:
						break;
					case Http2Frame::Type::Settings:
					{
						parsed_frame = std::make_shared<const Http2SettingsFrame>(frame);
						if (parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse settings frame");
							return false;
						}

						result = OnSettingsFrameReceived(std::static_pointer_cast<const Http2SettingsFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::PushPromise:
						break;
					case Http2Frame::Type::Ping:
						break;
					case Http2Frame::Type::GoAway:
					{
						parsed_frame = std::make_shared<const Http2GoAwayFrame>(frame);
						if (parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse settings frame");
							return false;
						}

						result = OnGoAwayFrameReceived(std::static_pointer_cast<const Http2GoAwayFrame>(parsed_frame));
					
						break;
					}
					case Http2Frame::Type::WindowUpdate:
					{
						parsed_frame = std::make_shared<const Http2WindowUpdateFrame>(frame);
						if (parsed_frame->GetParsingState() != Http2Frame::ParsingState::Completed)
						{
							logte("Failed to parse window update frame");
							return false;
						}

						result = OnWindowUpdateFrameReceived(std::static_pointer_cast<const Http2WindowUpdateFrame>(parsed_frame));
						break;
					}
					case Http2Frame::Type::Continuation:
						break;

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

				if (frame->IS_HTTP2_FRAME_FLAG_ON(Http2HeadersFrame::Flags::EndHeaders))
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
					if (OnRequestPrepared(GetSharedPtr()) == false)
					{
						return -1;
					}

					if (frame->IS_HTTP2_FRAME_FLAG_ON(Http2HeadersFrame::Flags::EndStream))
					{
						SetStatus(Status::Completed);
					}
				}

				if (frame->IS_HTTP2_FRAME_FLAG_ON(Http2HeadersFrame::Flags::EndStream))
				{
					// End of Stream, that means no more data will be sent
					auto result = OnRequestCompleted(GetSharedPtr());
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
							return -1;
					}
				}
				else
				{
					// Continue to receive more data
					SetStatus(Status::Exchanging);
				}

				return true;
			}

			bool HttpStream::OnSettingsFrameReceived(const std::shared_ptr<const Http2SettingsFrame> &frame)
			{
				if (frame->IsAck() == false)
				{
					// Settings Frame
					auto settings_frame = std::make_shared<Http2SettingsFrame>();
					settings_frame->SetAck();
					return _response->Send(settings_frame);
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

			bool HttpStream::SendInitialControlMessage()
			{
				logtd("Send Initial Control Message");
				// Settings Frame
				auto settings_frame = std::make_shared<Http2SettingsFrame>();
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::HeaderTableSize, 65536);
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::MaxConcurrentStreams, 1000);
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::InitialWindowSize, 6291456);
				settings_frame->SetParameter(Http2SettingsFrame::Parameters::MaxHeaderListSize, 262144);

				auto result = _response->Send(settings_frame);

				// WindowUpdate Frame
				auto window_update_frame = std::make_shared<Http2WindowUpdateFrame>(0);
				window_update_frame->SetWindowSizeIncrement(6291456);

				result = result ? _response->Send(window_update_frame) : false;
				
				return result;
			}
		}  // namespace h2
	}	   // namespace svr
}  // namespace http
