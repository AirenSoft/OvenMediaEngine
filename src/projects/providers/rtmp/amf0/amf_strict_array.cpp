//==============================================================================
//
//  RTMPProvider
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./amf_strict_array.h"

#include "providers/rtmp/rtmp_provider_private.h"

AmfStrictArray::AmfStrictArray()
	: AmfPropertyBase(AmfTypeMarker::StrictArray)
{
}

AmfStrictArray::AmfStrictArray(const AmfStrictArray &other)
	: AmfPropertyBase(other)
{
	_amf_properties = other._amf_properties;
}

AmfStrictArray::AmfStrictArray(AmfStrictArray &&other) noexcept
	: AmfPropertyBase(std::move(other))
{
	std::swap(_amf_properties, other._amf_properties);
}

AmfStrictArray &AmfStrictArray::operator=(const AmfStrictArray &other)
{
	if (this != &other)
	{
		AmfPropertyBase::operator=(other);
		_amf_properties = other._amf_properties;
	}

	return *this;
}

AmfStrictArray &AmfStrictArray::operator=(AmfStrictArray &&other) noexcept
{
	if (this != &other)
	{
		AmfPropertyBase::operator=(std::move(other));
		std::swap(_amf_properties, other._amf_properties);
	}

	return *this;
}

bool AmfStrictArray::Append(const AmfProperty &property)
{
	_amf_properties.push_back(property);
	return true;
}

bool AmfStrictArray::Encode(ov::ByteStream &byte_stream, bool encode_marker) const
{
	if (encode_marker)
	{
		if (EncodeMarker(byte_stream) == false)
		{
			logtd("Failed to encode marker");
			return false;
		}
	}

	// Write the count of items
	if (byte_stream.WriteBE32(_amf_properties.size()) == false)
	{
		logtd("Failed to write the count of items");
		return false;
	}

	// Write the items
	for (auto &property : _amf_properties)
	{
		if (property.Encode(byte_stream, true) == false)
		{
			return false;
		}
	}

	return true;
}

bool AmfStrictArray::Decode(ov::ByteStream &byte_stream, bool decode_marker)
{
	if (decode_marker)
	{
		AmfTypeMarker type;
		if (DecodeMarker(byte_stream, false, &type) == false)
		{
			logtd("Failed to decode marker");
			return false;
		}

		if (type != _amf_data_type)
		{
			OV_ASSERT(type == _amf_data_type, "Type mismatch: expected: %d, actual: %d", _amf_data_type, type);
			return false;
		}
	}

	// Read the count of items
	if (byte_stream.IsRemained(4) == false)
	{
		logtd("Failed to read the count of items");
		return false;
	}

	auto count = byte_stream.ReadBE32();

	// Read the items
	for (size_t index = 0; index < count; ++index)
	{
		AmfProperty property;
		if (property.Decode(byte_stream, true) == false)
		{
			return false;
		}

		_amf_properties.push_back(std::move(property));
	}

	return true;
}

const AmfProperty *AmfStrictArray::GetAt(size_t index) const
{
	if (index >= _amf_properties.size())
	{
		return nullptr;
	}

	return &(_amf_properties[index]);
}

void AmfStrictArray::ToString(ov::String &description, size_t indent) const
{
	auto indent_string = ov::String::Repeat("    ", indent);
	auto item_indent_string = ov::String::Repeat("    ", indent + 1);

	if (_amf_properties.empty())
	{
		description.Append("[]\n");
		return;
	}

	description.Append("[\n");

	for (auto &property : _amf_properties)
	{
		property.ToString(description, indent + 1);
	}

	description.AppendFormat("%s]\n", indent_string.CStr());
}

ov::String AmfStrictArray::ToString(size_t indent) const
{
	ov::String description;
	ToString(description, indent);
	return description;
}