//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "./json.h"
#include "./string.h"

#include <inttypes.h>
#include <string>
#include <ctime>
#include <chrono>

namespace ov
{
	class Converter
	{
	public:
		Converter() = delete;
		~Converter() = delete;

		static ov::String ToString(int number)
		{
			return ov::String::FormatString("%d", number);
		}

		static ov::String ToString(const ov::String &str)
		{
			return str;
		}

		static ov::String ToString(unsigned int number)
		{
			return ov::String::FormatString("%u", number);
		}

		static ov::String ToString(int64_t number)
		{
			return ov::String::FormatString("%" PRId64, number);
		}

		static ov::String ToString(uint64_t number)
		{
			return ov::String::FormatString("%" PRIu64, number);
		}

		// Due to conflict between size_t and uint64_t in linux, make sure that the OS is only active when macOS
#if defined(__APPLE__)
		static ov::String ToString(size_t number)
		{
			return ov::String::FormatString("%zu", number);
		}
#endif  // defined(__APPLE__)

		static ov::String ToString(float number)
		{
			return ov::String::FormatString("%f", number);
		}

		static ov::String ToString(double number)
		{
			return ov::String::FormatString("%f", number);
		}

		static ov::String ToString(const ov::JsonObject &object)
		{
			return object.ToString();
		}

		static ov::String ToString(const ::Json::Value &value)
		{
			if (value.isString())
			{
				return value.asCString();
			}

			return ov::Json::Stringify(value);
		}
	
		static ov::String ToString(const std::chrono::system_clock::time_point &tp)
		{
			std::time_t t = std::chrono::system_clock::to_time_t(tp);
			ov::String ts = std::ctime(&t);
			ts.SetLength(ts.GetLength() - 1);
			return ts;
		}

		static ov::String ToString(const StreamSourceType &type)
		{
			switch(type)
			{
				case StreamSourceType::Ovt:
					return "Ovt";
				case StreamSourceType::Rtmp:
					return "Rtmp";
				case StreamSourceType::Rtsp:
					return "Rtsp";
				case StreamSourceType::RtspPull:
					return "RtspPull";
				case StreamSourceType::Transcoder:
					return "Transcoder";
				default:
					return "Unknown";
			}
		}


		static ov::String ToString(const PublisherType &type)
		{
			switch(type)
			{
				case PublisherType::Webrtc:
					return "WebRTC";
				case PublisherType::Rtmp:
					return "RTMP";
				case PublisherType::Hls:
					return "HLS";
				case PublisherType::Dash:
					return "DASH";
				case PublisherType::LlDash:
					return "LL-DASH";
				case PublisherType::Ovt:
					return "Ovt";
				case PublisherType::Unknown:
				default:
					return "Unknown";
			}
		}

		static int32_t ToInt32(const ov::String &str, int base = 10)
		{
			try
			{
				return std::stoi(str.CStr(), nullptr, base);
			}
			catch (std::invalid_argument &e)
			{
				return 0;
			}
		}

		static uint16_t ToUInt16(const ov::String &str, int base = 10)
		{
			try
			{
				return (uint16_t)std::stoi(str.CStr(), nullptr, base);
			}
			catch (std::invalid_argument &e)
			{
				return 0;
			}
		}

		static uint32_t ToUInt32(const char *str, int base = 10)
		{
			try
			{
				if (str != nullptr)
				{
					return (uint32_t)std::stoul(str, nullptr, base);
				}
			}
			catch (std::invalid_argument &e)
			{
			}

			return 0;
		}

		static uint32_t ToUInt32(const ::Json::Value &value, int base = 10)
		{
			try
			{
				if (value.isNumeric())
				{
					return (uint32_t)value.asUInt();
				}

				if (value.isString())
				{
					return (uint32_t)std::stoul(value.asCString(), nullptr, base);
				}

				return 0;
			}
			catch (std::invalid_argument &e)
			{
				return 0;
			}
		}

		static int64_t ToInt64(const ov::String &str, int base = 10)
		{
			try
			{
				return std::stoll(str.CStr(), nullptr, base);
			}
			catch (std::invalid_argument &e)
			{
				return 0L;
			}
		}

		static uint64_t ToUInt64(const ov::String &str, int base = 10)
		{
			try
			{
				return std::stoull(str.CStr(), nullptr, base);
			}
			catch (std::invalid_argument &e)
			{
				return 0UL;
			}
		}

		static bool ToBool(const ov::String &str)
		{
			ov::String value = str.LowerCaseString();

			if (str == "true")
			{
				return true;
			}
			else if (str == "false")
			{
				return false;
			}

			return (ToInt64(str) != 0);
		}

		static float ToFloat(const ov::String &str)
		{
			try
			{
				return std::stof(str.CStr(), nullptr);
			}
			catch (std::invalid_argument &e)
			{
				return 0.0f;
			}
		}

		static double ToDouble(const ov::String &str)
		{
			try
			{
				return std::stod(str.CStr(), nullptr);
			}
			catch (std::invalid_argument &e)
			{
				return 0.0;
			}
		}
	};
}  // namespace ov
