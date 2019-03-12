//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <config/config.h>
#include <base/common_types.h>
#include <base/application/media_track.h>

namespace info
{
	//struct stream_id_t
	//{
	//	bool operator <(const stream_id_t &string) const
	//	{
	//		return true;
	//	}
	//
	//	bool operator >(const stream_id_t &string) const
	//	{
	//		return true;
	//	}
	//};

	typedef uint32_t stream_id_t;

	class Stream : public cfg::Stream
	{
	public:
		Stream()
		{
			_stream_id = ov::Random::GenerateUInt32();
		}

		stream_id_t GetId() const
		{
			return _stream_id;
		}

		bool AddTrack(std::shared_ptr<MediaTrack> track);
		const std::shared_ptr<MediaTrack> GetTrack(int32_t id) const;
		const std::map<int32_t, std::shared_ptr<MediaTrack>> &GetTracks() const;

		void ShowInfo();

	protected:
		uint32_t _stream_id = 0;

		// MediaTrack ID 값을 Key로 활용함
		std::map<int32_t, std::shared_ptr<MediaTrack>> _tracks;
	};
}