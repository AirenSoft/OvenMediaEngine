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

bool MarkerBox::InsertMarker(const Marker &marker)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	if (marker.tag.UpperCaseString() == "CUEEVENT-OUT")
	{
		if (_last_inserted_marker.tag.UpperCaseString() == "CUEEVENT-OUT")
		{
			logtw("CUEEVENT-OUT marker cannot be inserted after CUEEVENT-OUT marker");
			return false;
		}
		else if (_last_inserted_marker.tag.UpperCaseString() == "CUEEVENT-IN" && _last_inserted_marker.timestamp > marker.timestamp)
		{
			logtw("CUEEVENT-OUT marker cannot be inserted before CUEEVENT-IN marker");
			return false;
		}
	}
	else if (marker.tag.UpperCaseString() == "CUEEVENT-IN")
	{
		if (_last_inserted_marker.tag.UpperCaseString() == "CUEEVENT-IN")
		{
			if (_last_inserted_marker.timestamp > marker.timestamp)
			{
				// remove the last CUEEVENT-IN marker
				_markers.erase(_last_inserted_marker.timestamp);
			}
			else
			{
				logtw("CUEEVENT-IN marker only can be modified with the less timestamp");
				return false;
			}
		}
		else if (_last_inserted_marker.tag.UpperCaseString() == "CUEEVENT-OUT" && _last_inserted_marker.timestamp > marker.timestamp)
		{
			logtw("CUEEVENT-IN marker cannot be inserted before CUEEVENT-OUT marker");
			return false;
		}
	}
	else 
	{
		logtw("Unsupported marker tag: %s", marker.tag.CStr());
		return false;
	}

	_markers.emplace(marker.timestamp, marker);
	_last_inserted_marker = marker;

	return true;
}

bool MarkerBox::HasMarker() const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	return _markers.empty() == false;
}

bool MarkerBox::HasMarker(int64_t start_timestamp, int64_t end_timestamp) const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	for (auto &it : _markers)
	{
		auto &marker = it.second;
		if (marker.timestamp >= start_timestamp && marker.timestamp < end_timestamp)
		{
			return true;
		}
	}

	return false;
}

bool MarkerBox::HasMarker(int64_t end_timestamp) const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	for (auto &it : _markers)
	{
		auto &marker = it.second;
		if (marker.timestamp < end_timestamp)
		{
			return true;
		}
	}

	return false;
}

const Marker MarkerBox::GetFirstMarker() const
{
	std::shared_lock<std::shared_mutex> lock(_markers_guard);
	if (_markers.empty() == true)
	{
		return Marker();
	}

	return _markers.begin()->second;
}

std::vector<Marker> MarkerBox::PopMarkers(int64_t start_timestamp, int64_t end_timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	std::vector<Marker> markers;
	for (auto it = _markers.begin(); it != _markers.end();)
	{
		auto &marker = it->second;
		if (marker.timestamp >= start_timestamp && marker.timestamp < end_timestamp)
		{
			markers.push_back(marker);
			it = _markers.erase(it);
		}
		else
		{
			++it;
		}
	}

	return markers;
}

std::vector<Marker> MarkerBox::PopMarkers(int64_t end_timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	std::vector<Marker> markers;
	for (auto it = _markers.begin(); it != _markers.end();)
	{
		auto &marker = it->second;
		if (marker.timestamp < end_timestamp)
		{
			markers.push_back(marker);
			it = _markers.erase(it);
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

	auto it = _markers.find(timestamp);
	if (it == _markers.end())
	{
		return false;
	}

	_markers.erase(it);
	return true;
}

void MarkerBox::RemoveExpiredMarkers(int64_t current_timestamp)
{
	std::lock_guard<std::shared_mutex> lock(_markers_guard);

	for (auto it = _markers.begin(); it != _markers.end();)
	{
		auto &marker = it->second;
		if (marker.timestamp < current_timestamp)
		{
			logtc("Remove expired marker:(%lld) %lld - %s", current_timestamp, marker.timestamp, marker.tag.CStr());
			it = _markers.erase(it);
		}
		else
		{
			++it;
		}
	}
}