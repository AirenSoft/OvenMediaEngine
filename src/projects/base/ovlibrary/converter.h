//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./string.h"
#include "./json.h"

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
			if(value.isString())
			{
				return value.asCString();
			}

			return ov::Json::Stringify(value);
		}

		static int32_t ToInt32(const ov::String &str, int base = 10)
		{
			try
			{
				return std::stoi(str.CStr(), nullptr, base);
			}
			catch(std::invalid_argument &e)
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
			catch(std::invalid_argument &e)
			{
				return 0;
			}
		}

		static uint32_t ToUInt32(const ov::String &str, int base = 10)
		{
			try
			{
				return (uint32_t)std::stoul(str.CStr(), nullptr, base);
			}
			catch(std::invalid_argument &e)
			{
				return 0;
			}
		}

		static uint32_t ToUInt32(const ::Json::Value &value, int base = 10)
		{
			try
			{
				if(value.isNumeric())
				{
					return (uint32_t)value.asUInt();
				}

				if(value.isString())
				{
					return (uint32_t)std::stoul(value.asCString(), nullptr, base);
				}

				return 0;
			}
			catch(std::invalid_argument &e)
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
			catch(std::invalid_argument &e)
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
			catch(std::invalid_argument &e)
			{
				return 0UL;
			}
		}

		static bool ToBool(const ov::String &str)
		{
			ov::String value = str.LowerCaseString();

			if(str == "true")
			{
				return true;
			}
			else if(str == "false")
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
			catch(std::invalid_argument &e)
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
			catch(std::invalid_argument &e)
			{
				return 0.0;
			}
		}
	};
}
