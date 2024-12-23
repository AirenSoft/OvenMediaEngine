//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

/*
Data Structure

CueType : 8 bits 
Duration - 32 bits / milliseconds, only positive value

*/

class CueEvent
{
public:
	enum class CueType : uint8_t
	{
		Unknown = 0,
		OUT, 
		IN
	};

	static std::shared_ptr<CueEvent> Create(CueType cue_type, uint32_t duration_msec);
	static std::shared_ptr<CueEvent> Parse(const std::shared_ptr<ov::Data> &data);
	static CueType GetCueTypeByName(ov::String type);
	
	CueEvent(CueType cue_type, uint32_t duration_sec);
	~CueEvent() = default;

	std::shared_ptr<ov::Data> Serialize() const;

	CueType GetCueType() const;
	ov::String GetCueTypeName() const;
	uint32_t GetDurationMsec() const;

private:
	CueType _cue_type;
	uint32_t _duration_msec;
};