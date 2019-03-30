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
#include "./error.h"
#include "./data.h"

#include <jsoncpp-1.8.4/json/json.h>

namespace ov
{
	class JsonObject
	{
	public:
		friend class Json;

		JsonObject();
		~JsonObject();

		JsonObject(JsonObject &&object)
			: _value(object._value)
		{
			object._value = Json::Value(Json::ValueType::nullValue);
		}

		JsonObject(::Json::Value &value)
		{
			_value = value;
		}

		static JsonObject NullObject()
		{
			static ::Json::Value null_value;

			return JsonObject(null_value);
		}

		bool IsNull() const noexcept
		{
			return _value.isNull();
		}

		bool IsArray() const noexcept
		{
			return _value.isArray();
		}

		bool IsObject() const noexcept
		{
			return _value.isObject();
		}

		// jsoncpp를 wrapping 하기 전까지 사용할 임시 인터페이스
		::Json::Value &GetJsonValue()
		{
			return _value;
		}

		int GetIntValue(const ov::String &key) const
		{
			auto &value = _value[key];

			if(value.isIntegral())
			{
				return value.asInt();
			}

			return 0;
		}

		int64_t GetInt64Value(const ov::String &key) const
		{
			auto &value = _value[key];

			if(value.isIntegral())
			{
				return value.asInt64();
			}

			return 0;
		}

		const ::Json::Value &GetJsonValue(const ov::String &key) const
		{
			return _value[key];
		}

		String ToString() const;

		std::shared_ptr<Error> Parse(const String &str)
		{
			return Parse(str.CStr(), str.GetLength());
		}

		std::shared_ptr<Error> Parse(const std::shared_ptr<const Data> &data)
		{
			return Parse(data->GetData(), data->GetLength());
		}

	protected:
		std::shared_ptr<Error> Parse(const void *data, ssize_t length);

		::Json::Value _value;
		std::map<ov::String, JsonObject &> _properties;
	};
}
