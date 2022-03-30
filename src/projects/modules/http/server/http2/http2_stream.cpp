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
				_response = std::make_shared<Http2Response>(GetConnection()->GetSocket());
				_response->SetTlsData(GetConnection()->GetTlsData());
				_response->SetHeader("Server", "OvenMediaEngine");
				_response->SetHeader("Content-Type", "text/html");

				// https://www.rfc-editor.org/rfc/rfc7540.html#section-5.1.1
				// A stream identifier of zero (0x0) is used for connection control messages
				if (_stream_id == 0)
				{
					SendInitialControlMessage();
				}
			}

			std::shared_ptr<HttpRequest> HttpStream::GetRequest() const
			{
				return nullptr;
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
						break;
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
				return true;
			}

			bool HttpStream::OnSettingsFrameReceived(const std::shared_ptr<const Http2SettingsFrame> &frame)
			{
				if (frame->IsAck())
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
				auto window_update_frame = std::make_shared<Http2WindowUpdateFrame>();
				window_update_frame->SetWindowSizeIncrement(6291456);

				result = result ? _response->Send(window_update_frame) : false;
				
				return result;
			}
		}  // namespace h2
	}	   // namespace svr
}  // namespace http
