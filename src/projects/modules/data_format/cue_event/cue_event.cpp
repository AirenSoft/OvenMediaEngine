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

	CueType cue_type = static_cast<CueEvent::CueType>(stream.Read8());
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