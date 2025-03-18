//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "marker_box.h"

#include "marker_box_private.h"

///////////////////////////////////
// Marker
///////////////////////////////////

std::shared_ptr<Marker> Marker::CreateMarker(cmn::BitstreamFormat marker_format, int64_t timestamp, const std::shared_ptr<ov::Data> &data)
{
	std::shared_ptr<Marker> marker(new Marker(marker_format, timestamp, data));
	if (marker == nullptr)
	{
		return nullptr;
	}

	if (marker->Init() == false)
	{
		return nullptr;
	}

	return marker;
}

Marker::Marker(cmn::BitstreamFormat marker_format, int64_t timestamp, const std::shared_ptr<ov::Data> &data)
{
	_marker_format = marker_format;
	_timestamp = timestamp;
	_data = data;
}

bool Marker::Init()
{
	ov::String tag_prefix;

	if (_marker_format == cmn::BitstreamFormat::CUE)
	{
		tag_prefix = "CueEvent-";

		auto cue_event = CueEvent::Parse(_data);
		if (cue_event == nullptr)
		{
			return false;
		}

		_event = cue_event;

		// CueEvent-OUT, CueEvent-CONT, CueEvent-IN
		_tag = tag_prefix + cue_event->GetCueTypeName();
	}
	else if (_marker_format == cmn::BitstreamFormat::SCTE35)
	{
		tag_prefix = "Scte35Event-";

		auto scte_event = Scte35Event::Parse(_data);
		if (scte_event == nullptr)
		{
			return false;
		}

		_event = scte_event;

		// Scte35Event-OUT, Scte35Event-IN
		ov::String tag_suffix = scte_event->IsOutOfNetwork() ? "OUT" : "IN";
		_tag = tag_prefix + tag_suffix;
	}
	else
	{
		return false;
	}

	return true;
}

void Marker::SetParent(const std::shared_ptr<Marker> &parent)
{
	_parent = parent;
}

std::shared_ptr<Marker> Marker::GetParent() const
{
	return _parent;
}

// Getter
cmn::BitstreamFormat Marker::GetMarkerFormat() const
{
	return _marker_format;
}
int64_t Marker::GetTimestamp() const
{
	return _timestamp;
}
ov::String Marker::GetTag() const
{
	return _tag;
}
std::shared_ptr<ov::Data> Marker::GetData() const
{
	return _data;
}
std::optional<bool> Marker::IsOutOfNetwork() const
{
	if (_marker_format == cmn::BitstreamFormat::SCTE35)
	{
		auto scte_event = GetScte35Event();
		if (scte_event == nullptr)
		{
			return std::nullopt;
		}

		return scte_event->IsOutOfNetwork();
	}
	else if (_marker_format == cmn::BitstreamFormat::CUE)
	{
		auto cue_event = GetCueEvent();
		if (cue_event == nullptr)
		{
			return std::nullopt;
		}

		return cue_event->GetCueType() == CueEvent::CueType::OUT;
	}

	return std::nullopt;
}

void Marker::SetDesiredSequenceNumber(int64_t sequence_number)
{
	_desireded_sequence_number = sequence_number;
}

int64_t Marker::GetDesiredSequenceNumber() const
{
	return _desireded_sequence_number;
}

std::shared_ptr<CueEvent> Marker::GetCueEvent() const
{
	if (_marker_format != cmn::BitstreamFormat::CUE)
	{
		return nullptr;
	}

	return std::get<std::shared_ptr<CueEvent>>(_event);
}

std::shared_ptr<Scte35Event> Marker::GetScte35Event() const
{
	if (_marker_format != cmn::BitstreamFormat::SCTE35)
	{
		return nullptr;
	}

	return std::get<std::shared_ptr<Scte35Event>>(_event);
}

ov::String Marker::ToHlsTag(int64_t timestamp_offset) const
{
	if (_marker_format == cmn::BitstreamFormat::CUE)
	{
		// #EXT-X-CUE-OUT:DURATION=3.000
		// #EXT-X-CUE-IN

		auto cue_event = GetCueEvent();
		if (cue_event == nullptr)
		{
			return "";
		}

		if (cue_event->GetCueType() == CueEvent::CueType::OUT)
		{
			return ov::String::FormatString("#EXT-X-CUE-OUT:DURATION=%.3f\n", static_cast<double>(cue_event->GetDurationMsec()) / 1000.0);
		}
		else if (cue_event->GetCueType() == CueEvent::CueType::CONT)
		{
			return ov::String::FormatString("#EXT-X-CUE-OUT-CONT:ElapsedTime=%.3f,Duration=%.3f\n", static_cast<double>(cue_event->GetElapsedMsec()) / 1000.0, static_cast<double>(cue_event->GetDurationMsec()) / 1000.0);
		}
		else if (cue_event->GetCueType() == CueEvent::CueType::IN)
		{
			return "#EXT-X-CUE-IN\n";
		}
	}
	else if (_marker_format == cmn::BitstreamFormat::SCTE35)
	{
		// #EXT-X-DATERANGE:ID="1234",START-DATE="2025-01-01T00:00:00Z",PLANNED-DURATION=60.0,SCTE35-OUT=0xFC002F000000000000FF
		// #EXT-X-DATERANGE:ID="1234",START-DATE="2025-01-01T00:00:00Z",END-DATE="2025-01-01T00:01:00Z",DURATION=60.0,SCTE35-IN=0xFC002F000000000000FF

		auto scte_event = GetScte35Event();
		if (scte_event == nullptr)
		{
			return "";
		}

		auto scte_data = scte_event->MakeScteData();
		if (scte_data == nullptr)
		{
			return "";
		}

		ov::String tag = "#EXT-X-DATERANGE:";

		// ID
		tag += ov::String::FormatString("ID=\"%u\"", scte_event->GetID());

		if (scte_event->IsOutOfNetwork())
		{
			// START-DATE
			std::chrono::system_clock::time_point tp{std::chrono::milliseconds{scte_event->GetTimestampMsec() + timestamp_offset}};
			tag += ov::String::FormatString(",START-DATE=\"%s\"", ov::Converter::ToISO8601String(tp).CStr());

			// PLANNED-DURATION
			tag += ov::String::FormatString(",PLANNED-DURATION=%.3f", static_cast<double>(scte_event->GetDurationMsec()) / 1000.0);
			tag += ov::String::FormatString(",SCTE35-OUT=%s", scte_data->ToHexString().CStr());
		}
		else
		{
			// START-DATE from the parent
			if (GetParent() == nullptr || GetParent()->GetScte35Event() == nullptr)
			{
				return "";
			}
			auto parent_scte_event = GetParent()->GetScte35Event();
			if (parent_scte_event == nullptr)
			{
				return "";
			}

			// START-DATE
			std::chrono::system_clock::time_point tp{std::chrono::milliseconds{parent_scte_event->GetTimestampMsec() + timestamp_offset}};
			tag += ov::String::FormatString(",START-DATE=\"%s\"", ov::Converter::ToISO8601String(tp).CStr());

			// END-DATE
			// tp = std::chrono::system_clock::time_point{std::chrono::milliseconds{scte_event->GetTimestampMsec() + scte_event->GetDurationMsec() + timestamp_offset}};
			// tag += ov::String::FormatString(",END-DATE=\"%s\"", ov::Converter::ToISO8601String(tp).CStr());

			// // DURATION
			// tag += ov::String::FormatString(",DURATION=%.3f", static_cast<double>(scte_event->GetDurationMsec()) / 1000.0);

			tag += ov::String::FormatString(",SCTE35-IN=%s", scte_data->ToHexString().CStr());
		}

		tag += "\n";
		return tag;
	}

	return "";
}

bool MarkerBox::CanInsertMarker(const std::shared_ptr<Marker> &marker) const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);

	auto curr_out_of_network = marker->IsOutOfNetwork();
	if (curr_out_of_network.has_value() == false)
	{
		logtw("Failed to get out of network value of the marker : %s", marker->GetTag().CStr());
		return false;
	}
	bool curr_out_of_network_value = curr_out_of_network.value();

	if (_last_inserted_marker == nullptr)
	{
		if (curr_out_of_network_value == false)
		{
			logte("First marker must be OUT marker");
			return false;
		}

		// First marker always can be inserted
		return true;
	}

	auto last_out_of_network = _last_inserted_marker->IsOutOfNetwork();
	if (last_out_of_network.has_value() == false)
	{
		logtw("Failed to get out of network value of the last inserted marker", marker->GetTag().CStr());
		return false;
	}
	bool last_out_of_network_value = last_out_of_network.value();

	if (curr_out_of_network_value == true)
	{
		if (last_out_of_network_value == true)
		{
			logtw("OUT marker cannot be inserted after OUT marker");
			return false;
		}
		else if (last_out_of_network_value == false && _last_inserted_marker->GetTimestamp() > marker->GetTimestamp())
		{
			logtw("OUT marker cannot be inserted before IN marker");
			return false;
		}
	}
	else
	{
		if (last_out_of_network_value == false)
		{
			if (_last_inserted_marker->GetTimestamp() > marker->GetTimestamp())
			{
			}
			else
			{
				logtw("IN marker only can be modified with the less timestamp");
				return false;
			}
		}
		else if (last_out_of_network_value == true && _last_inserted_marker->GetTimestamp() > marker->GetTimestamp())
		{
			logtw("IN marker cannot be inserted before OUT marker");
			return false;
		}
	}

	return true;
}

bool MarkerBox::InsertMarker(const std::shared_ptr<Marker> &marker)
{
	if (CanInsertMarker(marker) == false)
	{
		return false;
	}

	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	auto curr_out_of_network = marker->IsOutOfNetwork();
	if (curr_out_of_network.has_value() == false)
	{
		logtw("Failed to get out of network value of the marker : %s", marker->GetTag().CStr());
		return false;
	}
	bool curr_out_of_network_value = curr_out_of_network.value();

	if (_last_inserted_marker == nullptr)
	{
		if (curr_out_of_network_value == false)
		{
			logte("First marker must be OUT marker");
			return false;
		}

		_markers_by_timestamp.emplace(marker->GetTimestamp(), marker);
		if (marker->GetDesiredSequenceNumber() >= 0)
		{
			// Update the sequence number
			_markers_by_sequence_number.emplace(marker->GetDesiredSequenceNumber(), marker);
		}

		_last_inserted_marker = marker;
		return true;
	}

	auto last_out_of_network = _last_inserted_marker->IsOutOfNetwork();
	if (last_out_of_network.has_value() == false)
	{
		logtw("Failed to get out of network value of the last inserted marker", marker->GetTag().CStr());
		return false;
	}
	bool last_out_of_network_value = last_out_of_network.value();

	// Cancel the last xxx-IN marker
	if (curr_out_of_network_value == false)
	{
		if (last_out_of_network_value == false)
		{
			if (_last_inserted_marker->GetTimestamp() > marker->GetTimestamp())
			{
				// remove the last CUEEVENT-IN marker
				_markers_by_timestamp.erase(_last_inserted_marker->GetTimestamp());
			}
			
			// Inherit the parent
			marker->SetParent(_last_inserted_marker->GetParent());
		}
		else
		{
			marker->SetParent(_last_inserted_marker);
		}
	}

	if (marker->GetDesiredSequenceNumber() >= 0)
	{
		// Update the sequence number
		_markers_by_sequence_number.emplace(marker->GetDesiredSequenceNumber(), marker);
	}

	_markers_by_timestamp.emplace(marker->GetTimestamp(), marker);
	_last_inserted_marker = marker;

	return true;
}

bool MarkerBox::HasMarker() const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	return _markers_by_timestamp.empty() == false;
}

bool MarkerBox::HasMarker(int64_t start_timestamp, int64_t end_timestamp) const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	for (auto &it : _markers_by_timestamp)
	{
		auto &marker = it.second;
		if (marker->GetTimestamp() >= start_timestamp && marker->GetTimestamp() < end_timestamp)
		{
			return true;
		}
	}

	return false;
}

bool MarkerBox::HasMarker(int64_t end_timestamp) const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	for (auto &it : _markers_by_timestamp)
	{
		auto &marker = it.second;
		if (marker->GetTimestamp() < end_timestamp)
		{
			return true;
		}
	}

	return false;
}

bool MarkerBox::HasMarkerWithSeq(int64_t sequence_number) const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	return _markers_by_sequence_number.find(sequence_number) != _markers_by_sequence_number.end();
}

const std::shared_ptr<Marker> MarkerBox::GetFirstMarker() const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	if (_markers_by_timestamp.empty() == true)
	{
		return nullptr;
	}

	return _markers_by_timestamp.begin()->second;
}

std::vector<std::shared_ptr<Marker>> MarkerBox::PopMarkers(int64_t start_timestamp, int64_t end_timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	std::vector<std::shared_ptr<Marker>> markers;
	for (auto it = _markers_by_timestamp.begin(); it != _markers_by_timestamp.end();)
	{
		auto &marker = it->second;
		if (marker->GetTimestamp() >= start_timestamp && marker->GetTimestamp() < end_timestamp)
		{
			markers.push_back(marker);
			it = _markers_by_timestamp.erase(it);
			_markers_by_sequence_number.erase(marker->GetDesiredSequenceNumber());
		}
		else
		{
			++it;
		}
	}

	return markers;
}

std::vector<std::shared_ptr<Marker>> MarkerBox::PopMarkers(int64_t end_timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	std::vector<std::shared_ptr<Marker>> markers;
	for (auto it = _markers_by_timestamp.begin(); it != _markers_by_timestamp.end();)
	{
		auto &marker = it->second;
		if (marker->GetTimestamp() < end_timestamp)
		{
			markers.push_back(marker);
			it = _markers_by_timestamp.erase(it);
			_markers_by_sequence_number.erase(marker->GetDesiredSequenceNumber());
		}
		else
		{
			++it;
		}
	}

	return markers;
}

bool MarkerBox::RemoveMarker(int64_t timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	auto it = _markers_by_timestamp.find(timestamp);
	if (it == _markers_by_timestamp.end())
	{
		return false;
	}

	_markers_by_timestamp.erase(it);
	_markers_by_sequence_number.erase(it->second->GetDesiredSequenceNumber());

	return true;
}

void MarkerBox::RemoveExpiredMarkers(int64_t current_timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	for (auto it = _markers_by_timestamp.begin(); it != _markers_by_timestamp.end();)
	{
		auto &marker = it->second;
		if (marker->GetTimestamp() < current_timestamp)
		{
			logtc("Remove expired marker:(%lld) %lld - %s", current_timestamp, marker->GetTimestamp(), marker->GetTag().CStr());
			it = _markers_by_timestamp.erase(it);
			_markers_by_sequence_number.erase(marker->GetDesiredSequenceNumber());
		}
		else
		{
			++it;
		}
	}
}

int64_t MarkerBox::GetEstimatedSequenceNumber(int64_t timestamp_ms) const
{
	auto segment_info = GetSegmentationInfo();
	if (segment_info.has_value() == false)
	{
		return -1;
	}

	if (segment_info->media_type != cmn::MediaType::Video)
	{
		return -1;
	}

	auto keyframe_interval_ms = segment_info->keyframe_interval / segment_info->framerate * 1000;
	auto actual_segment_duration_ms = std::ceil(segment_info->target_segment_duration_ms / keyframe_interval_ms) * keyframe_interval_ms;
	auto last_sample_end_timestamp_ms = segment_info->last_sample_timestamp_ms + segment_info->last_sample_duration_ms;
	auto time_until_marker_ms = timestamp_ms - last_sample_end_timestamp_ms;
	auto remaining_time_in_current_segment_ms = segment_info->is_last_segment_completed ? 0 : actual_segment_duration_ms - segment_info->last_segement_duration_ms;


	int64_t estimated_sequence_number = segment_info->last_segment_number;
	
	if (time_until_marker_ms <= remaining_time_in_current_segment_ms)
	{
		// In the current segment
	}
	else 
	{
		estimated_sequence_number += std::ceil((time_until_marker_ms - remaining_time_in_current_segment_ms) / actual_segment_duration_ms);
	}
	

	logtd("keyframe_interval_ms: %f, actual_segment_duration_ms: %f, last_sample_end_timestamp_ms: %f, time_until_marker_ms: %f, remaining_time_in_current_segment_ms: %f, estimated_sequence_number: %lld last_segment_number: %lld  last_segment_duration: %f is_last_segment_completed: %d",
		  keyframe_interval_ms, actual_segment_duration_ms, last_sample_end_timestamp_ms, time_until_marker_ms, remaining_time_in_current_segment_ms, estimated_sequence_number, segment_info->last_segment_number, segment_info->last_segement_duration_ms, segment_info->is_last_segment_completed);

	return estimated_sequence_number;
}
