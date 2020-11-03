//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./json.h"
#include "./string.h"

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <string>

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

		static ov::String ToString(const char *str)
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
#endif	// defined(__APPLE__)

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
			char buffer[32]{0};
			::ctime_r(&t, buffer);
			// Ensure null-terminated
			buffer[OV_COUNTOF(buffer) - 1] = '\0';

			ov::String time_string = buffer;

			return time_string.Trim();
		}

		static ov::String ToISO8601String(const std::chrono::system_clock::time_point &tp)
		{
			auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000;
			auto time = std::chrono::system_clock::to_time_t(tp);
			std::tm local_time{};
			::localtime_r(&time, &local_time);

			std::ostringstream oss;
			oss << std::put_time(&local_time, "%Y-%m-%dT%I:%M:%S") << "." << std::setfill('0') << std::setw(3) << milliseconds << std::put_time(&local_time, "%z");

			return oss.str().c_str();
		}

		static ov::String ToSiString(int64_t number, int precision)
		{
			ov::String suf[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y"};

			if (number == 0)
			{
				return "0";
			}

			int64_t abs_number = std::abs(number);
			int8_t place = std::floor(std::log10(abs_number) / std::log10(1000));
			if (place > 8)
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

		static int32_t ToInt32(const char *str, int base = 10)
		{
			if (str != nullptr)
			{
				try
				{
					return std::stoi(str, nullptr, base);
				}
				catch (std::invalid_argument &e)
				{
				}
			}

			return 0;
		}

		static int32_t ToInt32(const ::Json::Value &value, int base = 10)
		{
			if (value.isIntegral())
			{
				return value.asInt();
			}

			return ToInt32(value.toStyledString(), base);
		}

		static uint16_t ToUInt16(const char *str, int base = 10)
		{
			if (str != nullptr)
			{
				try
				{
					return static_cast<uint16_t>(std::stoi(str, nullptr, base));
				}
				catch (std::invalid_argument &e)
				{
				}
			}

			return 0;
		}

		static uint32_t ToUInt32(const char *str, int base = 10)
		{
			if (str != nullptr)
			{
				try
				{
					return static_cast<uint32_t>(std::stoul(str, nullptr, base));
				}
				catch (std::invalid_argument &e)
				{
				}
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

		static int64_t ToInt64(const char *str, int base = 10)
		{
			if (str != nullptr)
			{
				try
				{
					return std::stoll(str, nullptr, base);
				}
				catch (std::invalid_argument &e)
				{
				}
			}

			return 0L;
		}

		static uint64_t ToUInt64(const char *str, int base = 10)
		{
			if (str != nullptr)
			{
				try
				{
					return std::stoull(str, nullptr, base);
				}
				catch (std::invalid_argument &e)
				{
				}
			}

			return 0UL;
		}

		static bool ToBool(const char *str)
		{
			if (str == nullptr)
			{
				return false;
			}

			ov::String value = str;
			value.MakeLower();

			if (value == "true")
			{
				return true;
			}
			else if (value == "false")
			{
				return false;
			}

			return (ToInt64(str) != 0);
		}

		static bool ToBool(const ::Json::Value &value)
		{
			if (value.isBool())
			{
				return value.asBool();
			}

			return ToBool(value.toStyledString().c_str());
		}

		static float ToFloat(const char *str)
		{
			if (str != nullptr)
			{
				try
				{
					return std::stof(str, nullptr);
				}
				catch (std::invalid_argument &e)
				{
				}
			}

			return 0.0f;
		}

		static float ToFloat(const ::Json::Value &value)
		{
			if (value.isDouble())
			{
				return value.asDouble();
			}

			return ToFloat(value.toStyledString().c_str());
		}

		static double ToDouble(const char *str)
		{
			if (str != nullptr)
			{
				try
				{
					return std::stod(str, nullptr);
				}
				catch (std::invalid_argument &e)
				{
				}
			}

			return 0.0;
		}
	};
}  // namespace ov
