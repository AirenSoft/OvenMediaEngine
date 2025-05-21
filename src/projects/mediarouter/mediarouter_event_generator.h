//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"
#include <base/provider/application.h>

class MediaRouterEventGenerator
{
public:
	class Event
	{
	public:
		Event(cmn::BitstreamFormat eventFormat, cmn::PacketType eventType, bool urgent, int32_t interval)
		{
			_event_format = eventFormat;
			_event_type = eventType;
			_urgent = urgent;
			_interval = interval;
			_keyframe_only = false;
			
			_stop_watch.Start();
		}
		virtual ~Event() = default;

		virtual bool IsTrigger()
		{
			return _stop_watch.IsElapsed(_interval);
		}

		virtual bool Update()
		{
			return _stop_watch.Update();
		}

		void SetKeyframeOnly(bool keyframe_only)
		{
			_keyframe_only = keyframe_only;
		}

		virtual ov::String GetInfoString()
		{
			return ov::String::FormatString("EventFormat:%s, EventType:%s, Urgent:%s, Interval:%d, KeyframeOnly:%s",
											GetBitstreamFormatString(_event_format),
											GetMediaPacketTypeString(_event_type),
											_urgent ? "true" : "false",
											_interval, 
											_keyframe_only ? "true" : "false");
		}

		int32_t _interval;
		cmn::BitstreamFormat _event_format;
		cmn::PacketType _event_type;
		bool _urgent;
		bool _keyframe_only;
		ov::StopWatch _stop_watch;
	};

	class SeiEvent : public Event
	{
	public:
		SeiEvent(cmn::BitstreamFormat eventFormat, cmn::PacketType eventType, bool urgent, int32_t interval)
			: Event(eventFormat, eventType, urgent, interval)
		{
		}

		void SetSeiType(ov::String sei_type)
		{
			_sei_type = sei_type;
		}

		void SetData(ov::String data)
		{
			_data = data;
		}


		ov::String GetInfoString()
		{
			return ov::String::FormatString("%s, SeiType:%s, Data:%s", Event::GetInfoString().CStr(), _sei_type.CStr(), _data.CStr());
		}

		ov::String _sei_type;
		ov::String _data;
	};

	class AmfEvent : public Event
	{
	public:
		AmfEvent(cmn::BitstreamFormat eventFormat, cmn::PacketType eventType, bool urgent, int32_t interval)
			: Event(eventFormat, eventType, urgent, interval)
		{
		}

		void SetAmfType(ov::String amf_type)
		{
			_amf_type = amf_type;
		}

		ov::String GetAmfType() const
		{
			return _amf_type;
		}

		void AddData(ov::String key, ov::String type, ov::String value)
		{
			_data[key] = std::make_pair(type, value);
		}

		ov::String GetInfoString()
		{
			return ov::String::FormatString("%s, AmfType:%s", Event::GetInfoString().CStr(), _amf_type.CStr());
		}


		ov::String _amf_type;
		std::map<ov::String, std::pair<ov::String, ov::String>> _data;
	};

	MediaRouterEventGenerator();
	~MediaRouterEventGenerator();

	void Init(const std::shared_ptr<info::Stream> &stream_info);

	void Update(
		const std::shared_ptr<info::Stream> &stream_info,
		const std::shared_ptr<MediaTrack> &media_track,
		const std::shared_ptr<MediaPacket> &media_packet);

private:
	bool _cfg_enabled;
	ov::String _cfg_path;
	time_t _last_modified_time;

	std::vector<std::shared_ptr<MediaRouterEventGenerator::Event>> GetMatchedEvents(const std::shared_ptr<info::Stream> &stream_info);
	static const std::shared_ptr<pvd::Stream> GetSourceStream(const std::shared_ptr<info::Stream> &stream_info);

	std::shared_ptr<ov::Data> MakeSEIData(std::shared_ptr<Event> event);
	std::shared_ptr<ov::Data> MakeAMFData(std::shared_ptr<Event> event);

	std::vector<std::shared_ptr<MediaRouterEventGenerator::Event>> _events;
	ov::StopWatch _stop_watch;
};
