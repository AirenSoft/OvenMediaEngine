//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cue_event.h"

std::shared_ptr<CueEvent> CueEvent::Create(CueType cue_type, uint32_t duration_sec)
{
	return std::make_shared<CueEvent>(cue_type, duration_sec);
}

std::shared_ptr<CueEvent> CueEvent::Parse(const std::shared_ptr<ov::Data> &data)
{
	if (data == nullptr)
	{
		return nullptr;
	}

	ov::ByteStream stream(data);

	if (data->GetLength() != 5)
	{
		return nullptr;
	}

	uint8_t cue_type_value = stream.Read8();
	if (cue_type_value > static_cast<uint8_t>(CueType::IN))
	{
		return nullptr;
	}

	CueType cue_type = static_cast<CueEvent::CueType>(cue_type_value);
	if (cue_type == CueType::Unknown)
	{
		return nullptr;
	}

	uint32_t duration_msec = stream.ReadBE32();

	return Create(cue_type, duration_msec);
}

CueEvent::CueType CueEvent::GetCueTypeByName(ov::String type)
{
	if (type.UpperCaseString() == "OUT")
	{
		return CueType::OUT;
	}
	else if (type.UpperCaseString() == "IN")
	{
		return CueType::IN;
	}

	return CueType::Unknown;
}

CueEvent::CueEvent(CueType cue_type, uint32_t duration_sec)
{
	_cue_type = cue_type;
	_duration_msec = duration_sec;
}

std::shared_ptr<ov::Data> CueEvent::Serialize() const
{
	ov::ByteStream stream(5);

	stream.Write8(static_cast<uint8_t>(_cue_type));
	stream.WriteBE32(_duration_msec);

	return stream.GetDataPointer();
}

CueEvent::CueType CueEvent::GetCueType() const
{
	return _cue_type;
}

ov::String CueEvent::GetCueTypeName() const
{
	switch (_cue_type)
	{
	case CueType::OUT:
		return "OUT";
	case CueType::IN:
		return "IN";
	default:
		return "Unknown";
	}
}

uint32_t CueEvent::GetDurationMsec() const
{
	return _duration_msec;
}