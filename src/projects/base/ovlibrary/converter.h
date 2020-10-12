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
#include "base/mediarouter/media_type.h"

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <string>
#include <ctime>
#include <chrono>
#include <cmath>

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
			char buffer[32] { 0 };
			::ctime_r(&t, buffer);
			// Ensure null-terminated
			buffer[OV_COUNTOF(buffer) - 1] = '\0';

			ov::String time_string = buffer;

			return time_string.Trim();
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

		static ov::String ToString(const ProviderType &type)
		{
			switch(type)
			{
				case ProviderType::Unknown:
					return "Unknown";
				case ProviderType::Rtmp:
					return "RTMP";
				case ProviderType::Rtsp:
					return "RTSP";
				case ProviderType::RtspPull:
					return "RTSP Pull";
				case ProviderType::Ovt:
					return "OVT";
				case ProviderType::Mpegts:
					return "MPEG-TS";
			}

			return "Unknown";
		}

		static ov::String ToString(const PublisherType &type)
		{
			switch(type)
			{
				case PublisherType::Unknown:
				case PublisherType::NumberOfPublishers:
					return "Unknown";
				case PublisherType::Webrtc:
					return "WebRTC";
				case PublisherType::Rtmp:
					return "RTMP";
				case PublisherType::RtmpPush:
					return "RTMPPush";					
				case PublisherType::Hls:
					return "HLS";
				case PublisherType::Dash:
					return "DASH";
				case PublisherType::LlDash:
					return "LLDASH";
				case PublisherType::Ovt:
					return "Ovt";
				case PublisherType::File:
					return "File";
			}

			return "Unknown";
		}

		static ov::String ToString(const common::MediaCodecId &type)
		{
			switch(type)
			{
				case common::MediaCodecId::H264:
					return "H264";
				case common::MediaCodecId::H265:
					return "H265";
				case common::MediaCodecId::Vp8:
					return "VP8";
				case common::MediaCodecId::Vp9:
					return "VP9";
				case common::MediaCodecId::Flv:
					return "FLV";
				case common::MediaCodecId::Aac:
					return "AAC";
				case common::MediaCodecId::Mp3:
					return "MP3";
				case common::MediaCodecId::Opus:
					return "OPUS";
				case common::MediaCodecId::None:
				default:
					return "Unknwon";
			}
		}

		static ov::String ToString(const common::MediaType &type)
		{
			switch(type)
			{
				case common::MediaType::Video:
					return "Video";		
				case common::MediaType::Audio:
					return "Audio";
				case common::MediaType::Data:
					return "Data";
				case common::MediaType::Subtitle:
					return "Subtitle";
				case common::MediaType::Attachment:
					return "Attachment";
				case common::MediaType::Unknown:
				default:
					return "Unknown";
			}
		}

		static ov::String ToSiString(int64_t number, int precision)
		{
			ov::String suf[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y"};
		
			if(number == 0)
			{
				return "0";
			}

			int64_t abs_number = std::abs(number);
			int8_t place = std::floor(std::log10(abs_number) / std::log10(1000));
			if(place > 8)
			{
				place = 8;
			}

			double num = number / std::pow(1000, place);

			ov::String si_number;
			si_number.Format("%.*f%s", precision, num, suf[place].CStr());

			return si_number;
		}

		static ov::String BitToString(int64_t bits)
		{
			return ToSiString(bits, 2) + "b";
		}

		static ov::String BytesToString(int64_t bytes)
		{
			return ToSiString(bytes, 2) + "B";
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
