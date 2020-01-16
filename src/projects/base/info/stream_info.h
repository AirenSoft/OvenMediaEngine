#pragma once

#include "base/common_types.h"
#include "base/info/media_track.h"

class StreamInfo
{
public:
	StreamInfo(StreamSourceType source);
	StreamInfo(uint32_t stream_id, StreamSourceType source);
	StreamInfo(const StreamInfo &stream_info);

	virtual ~StreamInfo();

	void SetId(uint32_t id);
	uint32_t GetId() const;

	ov::String GetName();
	void SetName(ov::String name);

	std::chrono::system_clock::time_point GetCreatedTime() const;
	StreamSourceType  GetSourceType() const;

	bool AddTrack(std::shared_ptr<MediaTrack> track);
	const std::shared_ptr<MediaTrack> GetTrack(int32_t id) const;
	const std::map<int32_t, std::shared_ptr<MediaTrack>> &GetTracks() const;

	void ShowInfo();

protected:
	uint32_t _id;
	ov::String _name;

	// MediaTrack ID 값을 Key로 활용함
	std::map<int32_t, std::shared_ptr<MediaTrack>> _tracks;

private:
	std::chrono::system_clock::time_point 			_created_time;
	// Where does the stream come from?
	StreamSourceType 								_source_type;
};
