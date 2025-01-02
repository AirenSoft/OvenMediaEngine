//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "amf_textdata_event.h"

bool AmfTextDataEvent::IsMatch(const ov::String &amf_type)
{
	return amf_type.UpperCaseString() == ov::String("onTextData").UpperCaseString();
}

std::shared_ptr<AmfTextDataEvent> AmfTextDataEvent::Create()
{
	return std::make_shared<AmfTextDataEvent>();
}

std::shared_ptr<AmfTextDataEvent> AmfTextDataEvent::Parse(const std::shared_ptr<ov::Data> &data)
{
	if (data == nullptr)
	{
		return nullptr;
	}

	auto object = std::make_shared<AmfTextDataEvent>();

	return object;
}

bool AmfTextDataEvent::IsValid(const Json::Value &json)
{
	if (json["data"].isNull() || !json["data"].isObject())
	{
		return false;
	}

	return true;
}

std::shared_ptr<AmfTextDataEvent> AmfTextDataEvent::Parse(const Json::Value &json)
{
	if (json["data"].isNull() || !json["data"].isObject())
	{
		return nullptr;
	}
	
	auto object = std::make_shared<AmfTextDataEvent>();

	auto data = json["data"];
	for (const auto &key : data.getMemberNames())
	{
		auto value = data[key];

		if (value.isBool())
		{
			object->Append(key.c_str(), value.asBool());
		}
		else if (value.isInt() || value.isUInt() || value.isDouble())
		{
			object->Append(key.c_str(), value.asDouble());
		}
		else if (value.isString())
		{
			object->Append(key.c_str(), value.asString().c_str());
		}
	}

	return object;
}

AmfTextDataEvent::AmfTextDataEvent()
{
	_arrays = std::make_shared<AmfEcmaArray>();
}

bool AmfTextDataEvent::Append(const char *name, const bool value)
{
	return _arrays->Append(name, AmfProperty(value));
}

bool AmfTextDataEvent::Append(const char *name, const double value)
{
	return _arrays->Append(name, AmfProperty(value));
}

bool AmfTextDataEvent::Append(const char *name, const char *value)
{
	return _arrays->Append(name, AmfProperty(value));
}

std::shared_ptr<ov::Data> AmfTextDataEvent::Serialize() const
{
	AmfDocument doc;
	doc.AppendProperty(AmfProperty("onTextData"));
	doc.AppendProperty(*_arrays.get());

	ov::ByteStream byte_stream;
	if (doc.Encode(byte_stream) == false)
	{
		return nullptr;
	}

	return byte_stream.GetDataPointer();
}