//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./json_builder.h"

#include "./json.h"

namespace ov
{
	JsonBuilder::JsonBuilder(const PrivateToken &token)
	{
	}

	std::shared_ptr<JsonBuilder> JsonBuilder::Builder(JsonBuilderModifier modifier)
	{
		auto builder = std::make_shared<JsonBuilder>(PrivateToken());

		auto modified_builder = (modifier != nullptr) ? modifier(builder.get()) : builder.get();

		if (modified_builder != nullptr)
		{
			return modified_builder->GetSharedPtrAs<JsonBuilder>();
		}

		return nullptr;
	}

	void JsonBuilder::PushBackInternal(const char *key, JsonValueType value)
	{
		if (_value_type == ::Json::ValueType::nullValue)
		{
			_value_type = ::Json::ValueType::objectValue;
		}

		if (_value_type != ::Json::ValueType::objectValue)
		{
			OV_ASSERT(false, "The current JSON value is not an object");
			return;
		}

		_value_map.PushBack(key, std::move(value));
	}

	JsonBuilder *JsonBuilder::PushBack(const char *key, ::Json::Value value)
	{
		PushBackInternal(key, std::move(value));

		return this;
	}

	JsonBuilder *JsonBuilder::PushBack(const char *key, std::shared_ptr<JsonBuilder> builder)
	{
		if (builder == nullptr)
		{
			PushBackInternal(key, ::Json::Value::null);
		}
		else
		{
			PushBackInternal(key, builder);
		}

		return this;
	}

	JsonBuilder *JsonBuilder::PushBack(const char *key, JsonBuilderModifier modifier)
	{
		if (modifier == nullptr)
		{
			PushBackInternal(key, ::Json::Value::null);
		}
		else
		{
			PushBackInternal(key, Builder(modifier));
		}

		return this;
	}

	void JsonBuilder::PushBackInternal(JsonValueType item)
	{
		if (_value_type == ::Json::ValueType::nullValue)
		{
			_value_type = ::Json::ValueType::arrayValue;
		}

		if (_value_type != ::Json::ValueType::arrayValue)
		{
			OV_ASSERT(false, "The current JSON value is not an array");
			return;
		}

		_value_list.push_back(std::move(item));
	}

	JsonBuilder *JsonBuilder::PushBack(::Json::Value value)
	{
		PushBackInternal(std::move(value));

		return this;
	}

	JsonBuilder *JsonBuilder::PushBack(std::shared_ptr<JsonBuilder> builder)
	{
		if (builder == nullptr)
		{
			PushBackInternal(::Json::Value::null);
		}
		else
		{
			PushBackInternal(builder);
		}

		return this;
	}

	JsonBuilder *JsonBuilder::PushBack(JsonBuilderModifier modifier)
	{
		if (modifier == nullptr)
		{
			PushBackInternal(::Json::Value::null);
		}
		else
		{
			PushBackInternal(Builder(modifier));
		}

		return this;
	}

	ov::String JsonBuilder::Stringify() const
	{
		ov::String output;
		return Stringify(output);
	}

	ov::String &JsonBuilder::Stringify(ov::String &output) const
	{
		switch (_value_type)
		{
			case ::Json::ValueType::nullValue:
				output = "";
				break;

			case ::Json::ValueType::objectValue:
				StringifyObject(output);
				break;

			case ::Json::ValueType::arrayValue:
				StringifyArray(output);
				break;

			default:
				OV_ASSERT(false, "Invalid JSON value type");
				break;
		}

		return output;
	}

	static std::string EscapeString(ov::String &output, const ov::String &value)
	{
		::Json::StreamWriterBuilder builder;

		return ::Json::writeString(builder, value.CStr()).c_str();
	}

	ov::String &JsonBuilder::StringifyObject(ov::String &output) const
	{
		auto end = _value_map.end();

		output.Append('{');

		for (auto it = _value_map.begin(); it != end; ++it)
		{
			if (it != _value_map.begin())
			{
				output.Append(',');
			}

			auto &key = it->first;
			auto &value = it->second;

			// Print key
			output.AppendFormat("%s:", EscapeString(output, key).c_str());

			// Print value
			if (std::holds_alternative<::Json::Value>(value))
			{
				auto json = std::get<::Json::Value>(value);

				output.Append(Json::Stringify(json, false));
			}
			else if (std::holds_alternative<std::shared_ptr<JsonBuilder>>(value))
			{
				auto builder = std::get<std::shared_ptr<JsonBuilder>>(value);

				builder->Stringify(output);
			}
			else
			{
				OV_ASSERT(false, "Invalid JSON value type: %d", value.index());
			}
		}

		output.Append('}');

		return output;
	}

	ov::String &JsonBuilder::StringifyArray(ov::String &output) const
	{
		auto end = _value_list.end();

		output.Append('[');

		for (auto it = _value_list.begin(); it != end; ++it)
		{
			if (it != _value_list.begin())
			{
				output.Append(',');
			}

			auto &value = *it;

			// Print value
			if (std::holds_alternative<::Json::Value>(value))
			{
				auto json = std::get<::Json::Value>(value);

				output.Append(Json::Stringify(json, false));
			}
			else if (std::holds_alternative<std::shared_ptr<JsonBuilder>>(value))
			{
				auto builder = std::get<std::shared_ptr<JsonBuilder>>(value);

				builder->Stringify(output);
			}
			else
			{
				OV_ASSERT(false, "Invalid JSON value type: %d", value.index());
			}
		}

		output.Append(']');

		return output;
	}

	::Json::Value JsonBuilder::ToJsonValue() const
	{
		return ::Json::Value();
	}
}  // namespace ov
