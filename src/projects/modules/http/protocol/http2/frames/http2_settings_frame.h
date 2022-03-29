//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/byte_io.h>
#include "../http2_frame.h"

namespace http
{
	namespace prot
	{
		namespace h2
		{
			class Http2SettingsFrame : public Http2Frame
			{
			public:
				enum class Flags : uint8_t
				{
					None = 0x00,
					Ack = 0x01,
				};

				enum class Parameters : uint16_t
				{
					HeaderTableSize = 0x1,
					EnablePush = 0x2,
					MaxConcurrentStreams = 0x3,
					InitialWindowSize = 0x4,
					MaxFrameSize = 0x5,
					MaxHeaderListSize = 0x6,
				};

				// Make from parsed general frame header and payload
				Http2SettingsFrame(const std::shared_ptr<const Http2Frame> &frame)
					: Http2Frame(frame)
				{
					ParsingPayload();
				}

				// Make by itself
				Http2SettingsFrame()
				{
					SetType(Http2Frame::Type::Settings);
					SetFlags(0);
					// Settings Frame's stream id is 0
					SetStreamId(0);
				}

				// To String
				ov::String ToString() const override
				{
					ov::String str;
					
					str = Http2Frame::ToString();

					str += "\n";
					str += "[Settings Frame]\n";
					// Print parameters
					for (const auto &param : _parameters)
					{
						str += ov::String::FormatString("%s: %u\n", GetStringFromSettingsParamId(param.first).CStr(), param.second);
					}

					return str;
				}

				void SetAck()
				{
					SetFlags(GetFlags() | static_cast<uint8_t>(Flags::Ack));
				}

				bool IsAck() const
				{
					return GetFlags() & static_cast<uint8_t>(Flags::Ack);
				}

				void SetParameter(Parameters parameter, uint32_t value)
				{
					_parameters[static_cast<uint16_t>(parameter)] = value;
				}

				// Get Parameter
				// Returns {exist, value}
				std::tuple<bool, uint32_t> GetParameter(Parameters parameter) const
				{
					auto itr = _parameters.find(static_cast<uint16_t>(parameter));
					if (itr == _parameters.end())
					{
						return {false, 0};
					}

					return {true, itr->second};
				}

				// To data
				const std::shared_ptr<const ov::Data> GetPayload() const override
				{
					if (Http2Frame::GetPayload() != nullptr)
					{
						return Http2Frame::GetPayload();
					}

					if (_parameters.size() == 0)
					{
						return nullptr;
					}

					auto payload = std::make_shared<ov::Data>();
					ov::ByteStream stream(payload.get()); 

					for (auto &parameter : _parameters)
					{
						stream.WriteBE16(parameter.first);
						stream.WriteBE32(parameter.second);
					}

					return payload;
				}

				// Get string from parameter
				static const ov::String GetStringFromSettingsParamId(uint16_t param)
				{
					switch (param)
					{
						case static_cast<uint16_t>(Parameters::HeaderTableSize):
							return "HeaderTableSize";
						case static_cast<uint16_t>(Parameters::EnablePush):
							return "EnablePush";
						case static_cast<uint16_t>(Parameters::MaxConcurrentStreams):
							return "MaxConcurrentStreams";
						case static_cast<uint16_t>(Parameters::InitialWindowSize):
							return "InitialWindowSize";
						case static_cast<uint16_t>(Parameters::MaxFrameSize):
							return "MaxFrameSize";
						case static_cast<uint16_t>(Parameters::MaxHeaderListSize):
							return "MaxHeaderListSize";
						default:
							return "Unknown";
					}
				}

			private:
				void ParsingPayload()
				{
					// The payload of a SETTINGS frame consists of zero or more parameters,
					auto payload = GetPayload();
					if (payload == nullptr)
					{
						SetParsingState(ParsingState::Completed);
						return;
					}
					
					// Parse each parameter in the payload
					auto payload_data = payload->GetDataAs<uint8_t>();
					auto payload_size = payload->GetLength();
					size_t payload_offset = 0;

					if (payload_size % 6 != 0)
					{
						SetParsingState(ParsingState::Error);
						return;
					}

					while (payload_offset < payload_size)
					{
						auto parameter = ByteReader<uint16_t>::ReadBigEndian(payload_data + payload_offset);
						payload_offset += sizeof(uint16_t);

						auto value = ByteReader<uint32_t>::ReadBigEndian(payload_data + payload_offset);
						payload_offset += sizeof(uint32_t);

						_parameters[parameter] = value;
					}

					SetParsingState(ParsingState::Completed);
				}

				std::shared_ptr<Http2Frame> _frame;
				// Parameters
				std::map<uint16_t, uint32_t> _parameters;
			};
		}
	}
}