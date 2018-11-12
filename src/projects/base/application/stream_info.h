#pragma once

#include "base/common_types.h"
#include "base/application/media_track.h"

class StreamInfo
{
public:
	// 입력 소스의 타입
	enum InputSourceType
	{
		kSourceTypeOrigin = 0,
		kSourceTypeTranscoded
	};

	StreamInfo();
	StreamInfo(uint32_t stream_id);
	StreamInfo(const StreamInfo &stream_info);

	virtual ~StreamInfo();

	void SetId(uint32_t id);
	uint32_t GetId() const;

	ov::String GetName();
	void SetName(ov::String name);

	bool AddTrack(std::shared_ptr<MediaTrack> track);
	const std::shared_ptr<MediaTrack> GetTrack(int32_t id) const;
	const std::map<int32_t, std::shared_ptr<MediaTrack>> &GetTracks() const;

	void ShowInfo();

protected:
	uint32_t _id;
	ov::String _name;

	// MediaTrack ID 값을 Key로 활용함
	std::map<int32_t, std::shared_ptr<MediaTrack>> _tracks;

};
