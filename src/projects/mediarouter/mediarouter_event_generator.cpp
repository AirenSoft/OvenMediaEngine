//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mediarouter_event_generator.h"

#include <base/ovlibrary/ovlibrary.h>
#include <modules/bitstream/h264/h264_sei.h>
#include <modules/data_format/amf_event/amf_textdata_event.h>
#include <orchestrator/orchestrator.h>

#include "mediarouter_private.h"

/**
==============================================================================* 
	Configuration Examples
==============================================================================
<!-- Server.xml -->
<Application>
	<EventGenerator>
		<Enable>true</Enable>
		<Path>events/send_event_info.xml</Path>
	</EventGenerator>
</Application>

==============================================================================
<!-- send_event_info.xml -->
<?xml version="1.0" encoding="UTF-8"?>
<EventInfo>
	//  H26x SEI Event 
	<Event>
		<Enable>true</Enable>							// Optional (default: false)
		<SourceStreamName>*</SourceStreamName>			// Must
		<Interval>2000</Interval>						// Must
		<KeyframeOnly>true</KeyframeOnly>				// Optional (default: false)
		<EventFormat>sei</EventFormat>					// Must
		<EventType>video</EventType>					// Optional (default: video)
		<Values>
			<SeiType>UserDataUnregistered</SeiType> 	// Optional (default: UserDataUnregistered)
			<Data>Hi! OvenMediaEngine!</Data>			// Must
		</Values>
	</Event>

	// AMF onTextData Event
	<Event>
		<Enable>true</Enable>							// Optional (default: false)
		<SourceStreamName>*</SourceStreamName>			// Must
		<Interval>2000</Interval>						// Must
		<EventFormat>amf</EventFormat>					// Must
		<EventType>video</EventType>					// Optional (default: video)
		<Values>
			<AmfType>onTextData</AmfType>			// Optional (default: onTextData)	
			<Data>
				<key1 type="string">value</key1>,	// String Type
				<key2 type="double">354.1</key2>,	// Number Type [Double]
				<key3 type="boolean">true</key3>	// Boolean Type [true | false]
			</Data>
		</Values>
	</Event>	
</EventInfo>
 */

MediaRouterEventGenerator::MediaRouterEventGenerator()
{
	_stop_watch.Start();
}

MediaRouterEventGenerator::~MediaRouterEventGenerator()
{
}

void MediaRouterEventGenerator::Init(const std::shared_ptr<info::Stream> &stream_info)
{
	_cfg_enabled = stream_info->GetApplicationInfo().GetConfig().GetEventGenerator().IsEnable();
	_cfg_path = stream_info->GetApplicationInfo().GetConfig().GetEventGenerator().GetPath();
	if(_cfg_path.IsEmpty())
	{
		_cfg_enabled = false;
	}
	if(_cfg_path.Get(0) != '/')
	{
		_cfg_path = ov::PathManager::GetAppPath("conf") + _cfg_path;
	}

	logtd("Enabled: %s, Path:%s", _cfg_enabled ? "true" : "false", _cfg_path.CStr());
}

void MediaRouterEventGenerator::Update(
	const std::shared_ptr<info::Stream> &stream_info,
	const std::shared_ptr<MediaTrack> &media_track,
	const std::shared_ptr<MediaPacket> &media_packet)
{
	if (!_cfg_enabled)
	{
		return;
	}

	// If the configuration file has been changed, reload the events
	if (_stop_watch.IsElapsed(1000) && _stop_watch.Update())
	{
		struct stat file_stat = {0};
		auto result = stat(_cfg_path.CStr(), &file_stat);
		if (result != 0 && errno == ENOENT)
		{
			_events.clear();
		}

		if (_last_modified_time == file_stat.st_mtime)
		{
			return;
		}
		_last_modified_time = file_stat.st_mtime;

		auto events = GetMatchedEvents(stream_info);
		_events = events;
	}

	// Send events
	for (auto &event : _events)
	{
		if (!event->IsTrigger())
		{
			continue;
		}
		event->Update();

		logtd("Event Triggered: %s, %s", stream_info->GetName().CStr(), event->GetInfoString().CStr());

		std::shared_ptr<ov::Data> event_data;
		switch (event->_event_format)
		{
			case cmn::BitstreamFormat::SEI: {
				// This event is passed to the TranscoderEvent class and then disappears. 
				// In TranscoderEvent, an SEI NAL unit is inserted into the video frame.
				event_data = MakeSEIData(event);
			}
			break;
			case cmn::BitstreamFormat::AMF:
				// This event creates an AmfTextDataEvent packet and sends it to the Publisher through the Data track.
				event_data = MakeAMFData(event);
			break;
			case cmn::BitstreamFormat::ID3v2:
			case cmn::BitstreamFormat::CUE:
			default:
				// Not implemented
				continue;
		}

		if(event_data == nullptr)
		{
			continue;
		}

		auto stream = GetSourceStream(stream_info);
		if (stream == nullptr)
		{
			logte("Could not find a source stream for [%s]", stream_info->GetName().CStr());
			return;
		}

		if(!stream->SendDataFrame(-1, event->_event_format, event->_event_type, event_data, event->_urgent))
		{
			logtw("Failed to send event data");
		}
	}
}

std::vector<std::shared_ptr<MediaRouterEventGenerator::Event>> MediaRouterEventGenerator::GetMatchedEvents(const std::shared_ptr<info::Stream> &stream_info)
{
	std::vector<std::shared_ptr<MediaRouterEventGenerator::Event>> results;

	ov::String xml_real_path = _cfg_path;

	pugi::xml_document xml_doc;
	auto load_result = xml_doc.load_file(xml_real_path.CStr());
	if (load_result == false)
	{
		logte("Stream(%s/%s) - Failed to load file(%s) status(%d) description(%s)",
			  stream_info->GetApplicationName(), stream_info->GetName(), xml_real_path.CStr(),
			  load_result.status, load_result.description());
		return {};
	}

	auto root_node = xml_doc.child("EventInfo");
	if (root_node.empty())
	{
		logte("Stream(%s/%s) - Failed to load Record info file(%s) because root node is not found",
			  stream_info->GetApplicationName(), stream_info->GetName(), xml_real_path.CStr());
		return {};
	}

	for (pugi::xml_node curr_node = root_node.child("Event"); curr_node; curr_node = curr_node.next_sibling("Event"))
	{
		bool enable = true;
		auto enable_string = curr_node.child_value("Enable");
		if (enable_string != nullptr && strlen(enable_string) > 0)
		{
			enable = (strcmp(enable_string, "true") == 0) ? true : false;
			if (enable == false)
			{
				// Disabled
				continue;
			}
		}

		ov::String source_stream_name = curr_node.child_value("SourceStreamName");

		ov::Regex _source_stream_name_regex = ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(source_stream_name));
		auto match_result = _source_stream_name_regex.Matches(stream_info->GetName().CStr());
		if (!match_result.IsMatched())
		{
			// Not matched
			continue;
		}

		ov::String event_format = curr_node.child_value("EventFormat");
		
		bool urgent = curr_node.child("Urgent") ? (strcmp(curr_node.child_value("Urgent"), "true") == 0) : false;
		
		int32_t interval = curr_node.child("Interval") ? curr_node.child("Interval").text().as_int() : 0;
		
		bool keyframe_only = curr_node.child("KeyframeOnly") ? (strcmp(curr_node.child_value("KeyframeOnly"), "true") == 0) : false;
		
		ov::String event_type_string = curr_node.child_value("EventType");
		
		cmn::PacketType event_type = cmn::PacketType::EVENT;
		if (!event_type_string.IsEmpty())
		{
			if (event_type_string.UpperCaseString() == "VIDEO")
			{
				event_type = cmn::PacketType::VIDEO_EVENT;
			}
			else if (event_type_string.UpperCaseString() == "AUDIO")
			{
				event_type = cmn::PacketType::AUDIO_EVENT;
			}
			else if (event_type_string.UpperCaseString() == "EVENT")
			{
				event_type = cmn::PacketType::EVENT;
			}
			else
			{
				// Unsupported event type
				continue;
			}
		}

		if (event_format.UpperCaseString() == "SEI")
		{
			pugi::xml_node values = curr_node.child("Values");
			ov::String sei_type = values.child_value("SeiType");
			ov::String sei_data = values.child_value("Data");

			if(sei_data.IsEmpty())
			{
				sei_type = "UserDataUnregistered";
			}

			auto event = std::make_shared<SeiEvent>(cmn::BitstreamFormat::SEI, event_type, urgent, interval, keyframe_only);
			event->SetSeiType(sei_type);
			event->SetData(sei_data);

			results.push_back(event);
		}
		else if (event_format.UpperCaseString() == "AMF")
		{
			pugi::xml_node values = curr_node.child("Values");
			ov::String amf_type = values.child_value("AmfType");

			auto event = std::make_shared<AmfEvent>(cmn::BitstreamFormat::AMF, event_type, urgent, interval, keyframe_only);
			event->SetAmfType(amf_type);

			for (pugi::xml_node data_node = values.child("Data").first_child(); data_node; data_node = data_node.next_sibling())
			{
				ov::String key = data_node.name();
				ov::String type = data_node.attribute("type").as_string();
				ov::String value = data_node.child_value();
				
				logtd("key:%s, value:%s, type:%s", key.CStr(), value.CStr(), type.CStr());

				event->AddData(key, type, value);
			}

			results.push_back(event);
		}
		else if (event_format.UpperCaseString() == "CUE")
		{
			// Not implemented
		}
		else if (event_format.UpperCaseString() == "ID3")
		{
			// Not implemented
		}
		else if (event_format.UpperCaseString() == "ID3V2")
		{
			// Not implemented
		}
		else
		{
			// Not supported event format
			continue;
		}
	}

	return results;
}

const std::shared_ptr<pvd::Stream> MediaRouterEventGenerator::GetSourceStream(const std::shared_ptr<info::Stream> &stream_info)
{
	auto provider = std::dynamic_pointer_cast<pvd::Provider>(ocst::Orchestrator::GetInstance()->GetProviderFromType(stream_info->GetProviderType()));
	if (provider == nullptr)
	{
		logte("Could not find a provider for scheme [%s]", stream_info->GetProviderType());
		return nullptr;
	}

	auto application = provider->GetApplicationByName(stream_info->GetApplicationInfo().GetVHostAppName());
	if (application == nullptr)
	{
		logte("Could not find an application for [%s]", stream_info->GetApplicationInfo().GetVHostAppName().CStr());
		return nullptr;
	}

	return application->GetStreamByName(stream_info->GetName());
}

std::shared_ptr<ov::Data> MediaRouterEventGenerator::MakeSEIData(std::shared_ptr<Event> event)
{
	std::shared_ptr<SeiEvent> sei_event = std::static_pointer_cast<SeiEvent>(event);

	H264SEI::PayloadType payload_type = H264SEI::PayloadType::USER_DATA_UNREGISTERED;
	if (!sei_event->_sei_type.IsEmpty())
	{
		payload_type = H264SEI::StringToPayloadType(sei_event->_sei_type);
	}
	// data (optional)
	std::shared_ptr<ov::Data> payload_data = (!sei_event->_data.IsEmpty()) ? std::make_shared<ov::Data>(sei_event->_data.CStr(), sei_event->_data.GetLength()) : std::make_shared<ov::Data>();

	auto sei = std::make_shared<H264SEI>();
	sei->SetPayloadType(payload_type);
	sei->SetPayloadTimeCode(ov::Time::GetTimestampInMs());
	sei->SetPayloadData(payload_data);

	return sei->Serialize();
}

std::shared_ptr<ov::Data> MediaRouterEventGenerator::MakeAMFData(std::shared_ptr<Event> event)
{
	std::shared_ptr<AmfEvent> amf_event = std::static_pointer_cast<AmfEvent>(event);

	if(amf_event->GetAmfType().UpperCaseString() != "ONTEXTDATA")
	{
		logte("Unsupported AMF event type [%s]", amf_event->GetAmfType().CStr());
		return nullptr;
	}

	auto amf = std::make_shared<AmfTextDataEvent>();
	
	for(auto &data : amf_event->_data)
	{
		auto key = data.first;
		auto type = data.second.first;
		auto value = data.second.second;

		// Replace ${EpochTime} with current epoch time
		{
			value = value.Replace("${EpochTime}", ov::String::FormatString("%lld", ov::Time::GetTimestampInMs()));
		}
		
		if(type == "string")
		{
			amf->Append(key.CStr(), value.CStr());
		}
		else if(type == "double")
		{
			double double_value = atof(value.CStr());
			amf->Append(key.CStr(), double_value);
		}
		else if(type == "boolean")
		{
			bool bool_value = (strcmp(value.CStr(), "true") == 0) ? true : false;
			amf->Append(key.CStr(), bool_value);
		}
	}

	return amf->Serialize();
}