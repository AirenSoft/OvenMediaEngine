#include "rtsp_header_field.h"

RtspHeaderField::RtspHeaderField()
{

}

RtspHeaderField::RtspHeaderField(RtspHeaderFieldType type, ov::String value)
{

}

RtspHeaderField::RtspHeaderField(RtspHeaderFieldType type, int32_t value)
{

}

RtspHeaderField::RtspHeaderField(ov::String name, int32_t value)
{
	SetContent(name, value);
}

RtspHeaderField::RtspHeaderField(ov::String name, ov::String value)
{
	SetContent(name, value);
}

ov::String RtspHeaderField::FieldTypeToString(RtspHeaderFieldType type)
{
	switch(type)
	{
		case RtspHeaderFieldType::ContentType:
			return "ContentType";
		case RtspHeaderFieldType::ContentBase:
			return "ContentBase";
		case RtspHeaderFieldType::Date:
			return "Date";
		case RtspHeaderFieldType::CSeq:
			return "CSeq";
		case RtspHeaderFieldType::UserAgent:
			return "UserAgent";
		case RtspHeaderFieldType::Server:
			return "Server";
		case RtspHeaderFieldType::CacheControl:
			return "CacheControl";
		case RtspHeaderFieldType::Public:
			return "Public";
		case RtspHeaderFieldType::Supported:
			return "Supported";
		case RtspHeaderFieldType::Accept:
			return "Accept";
		case RtspHeaderFieldType::ContentLength:
			return "ContentLength";
		case RtspHeaderFieldType::Session:
			return "Session";
		case RtspHeaderFieldType::Transport:
			return "Transport";
		case RtspHeaderFieldType::Expires:
			return "Expires";
		case RtspHeaderFieldType::Range:
			return "Range";
		case RtspHeaderFieldType::Unknown:
		default:
			return "Unknown";	
	}
}

bool RtspHeaderField::Parse(const ov::String &message)
{
	std::shared_ptr<RtspHeaderField> field = nullptr;
	
	// Parsing
	auto name_value = message.Split(":");
	if(name_value.size() != 2)
	{
		return false;
	}

	return SetContent(name_value[0], name_value[1]);
}

bool RtspHeaderField::SetContent(ov::String name, ov::String value)
{
	_name = name;
	_value = value;

	return true;
}

bool RtspHeaderField::SetContent(ov::String name, int32_t value)
{
	return SetContent(name, ov::String::FormatString("%d", value));
}

ov::String RtspHeaderField::Serialize()
{
	return ov::String::FormatString("%s: %s", _name.CStr(), _value.CStr()); 
}

ov::String RtspHeaderField::GetName()
{
	return _name;
}

ov::String RtspHeaderField::GetValue()
{
	return _value;
}

int32_t RtspHeaderField::GetValueAsInteger()
{
	return std::atoi(_value.CStr());
}