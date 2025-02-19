//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "base/info/dump.h"

struct Marker
{
	int64_t timestamp = -1;
	ov::String tag;
	std::shared_ptr<ov::Data> data = nullptr;
};

class MarkerBox
{
public:
	bool InsertMarker(const Marker &marker);

protected:
	bool HasMarker() const;
	// Has marker with the given range
	bool HasMarker(int64_t start_timestamp, int64_t end_timestamp) const;
	bool HasMarker(int64_t end_timestamp) const;
	const Marker GetFirstMarker() const;
	std::vector<Marker> PopMarkers(int64_t start_timestamp, int64_t end_timestamp);
	std::vector<Marker> PopMarkers(int64_t end_timestamp);
	bool RemoveMarker(int64_t timestamp);
	// remove expired markers
	void RemoveExpiredMarkers(int64_t current_timestamp);

private:


private:
	std::map<int64_t, Marker> _markers;
	mutable std::shared_mutex _markers_guard;
	Marker _last_inserted_marker;
};