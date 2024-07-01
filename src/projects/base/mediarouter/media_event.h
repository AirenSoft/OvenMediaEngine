//==============================================================================
//
//  MediaEvent
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "media_buffer.h"

// This event is delivered to each module sorted by occurrence time along with MediaPackets.
class MediaEvent : public MediaPacket
{
public:
	enum class CommandType : uint32_t
	{
		NOP = 0, // No Operation
		ConcludeLive, // Conclude Live, transits the stream to VOD if possible
	};

	MediaEvent(CommandType command_type, const std::shared_ptr<ov::Data> &command_data)
		: MediaPacket(0, cmn::MediaType::Data, 0, nullptr, -1LL, -1LL, -1LL, MediaPacketFlag::Unknown, cmn::BitstreamFormat::OVEN_EVENT, cmn::PacketType::EVENT)
	{
		SetEvent(command_type, command_data);
	}

	void SetEvent(CommandType command_type, const std::shared_ptr<ov::Data> &command_data)
	{
		_command_type = command_type;
		_command_data = command_data;

		if (_command_data == nullptr)
		{
			// Data of MediaPacket should not be nullptr
			_command_data = std::make_shared<ov::Data>();
			_command_data->SetLength(0);
			SetData(_command_data);
		}

		UpdateData();
	}

	void SetEmergency(bool emergency)
	{
		_emergency = emergency;
	}

	CommandType GetCommandType() const
	{
		return _command_type;
	}

	std::shared_ptr<ov::Data> GetCommandData() const
	{
		return _command_data;
	}

	bool IsEmergency() const
	{
		return _emergency;
	}

	std::shared_ptr<MediaEvent> Clone() const
	{
		auto cloned_event = std::make_shared<MediaEvent>(_command_type, _command_data);
		cloned_event->SetEmergency(_emergency);

		// Copy MediaPacket members
		cloned_event->SetMsid(GetMsid());
		cloned_event->SetTrackId(GetTrackId());

		cloned_event->SetPts(GetPts());
		cloned_event->SetDts(GetDts());
		cloned_event->SetDuration(GetDuration());
		cloned_event->SetFlag(GetFlag());
		cloned_event->SetBitstreamFormat(GetBitstreamFormat());
		cloned_event->SetPacketType(GetPacketType());
		cloned_event->SetFragHeader(GetFragHeader());
		cloned_event->SetHighPriority(IsHighPriority());

		return cloned_event;
	}

	ov::String ToString() const
	{
		size_t data_length = _command_data ? _command_data->GetLength() : 0;
		return ov::String::FormatString("MediaEvent: %s, Data Size(%d)", MediaEvent::CommandTypeToString(_command_type).CStr(), data_length);
	}

	static ov::String CommandTypeToString(CommandType command_type)
	{
		switch (command_type)
		{
		case CommandType::NOP:
			return "NOP";
		case CommandType::ConcludeLive:
			return "ConcludeLive";
		default:
			return "Unknown";
		}
	}

private:
	void UpdateData()
	{
		// Serialize the command type

		// TODO(Getroot): In the future, we will need to serialize this event and allow the origin to pass it to the edge over the OVT (we don't have such a case right now, so we skip it).
	}

	CommandType _command_type = CommandType::NOP;
	std::shared_ptr<ov::Data> _command_data = nullptr;
	bool _emergency = false;
};