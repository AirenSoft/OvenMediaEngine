#pragma once

#include "base/common_types.h"
#include "base/info/media_track.h"
#include "stream.h"

namespace info
{
	class Application;

	//TODO: It should be changed class name to Stream
	class StreamInfo
	{
	public:
		StreamInfo(const info::Application &app_info, StreamSourceType source);
		StreamInfo(const info::Application &app_info, info::stream_id_t stream_id, StreamSourceType source);
		StreamInfo(const StreamInfo &stream_info);

		virtual ~StreamInfo();

		void SetId(info::stream_id_t id);
		info::stream_id_t GetId() const;

		ov::String GetName() const;
		void SetName(ov::String name);

		void SetOriginStreamInfo(const std::shared_ptr<StreamInfo> &stream_info);
		const std::shared_ptr<StreamInfo> GetOriginStreamInfo();

		std::chrono::system_clock::time_point GetCreatedTime() const;
		StreamSourceType GetSourceType() const;

		bool AddTrack(std::shared_ptr<MediaTrack> track);
		const std::shared_ptr<MediaTrack> GetTrack(int32_t id) const;
		const std::map<int32_t, std::shared_ptr<MediaTrack>> &GetTracks() const;

		void ShowInfo();

		const Application &GetApplicationInfo() const
		{
			return *_app_info;
		}

	protected:
		info::stream_id_t _id = 0;
		ov::String _name;

		// MediaTrack ID 값을 Key로 활용함
		std::map<int32_t, std::shared_ptr<MediaTrack>> _tracks;

	private:
		std::chrono::system_clock::time_point _created_time;
		// Where does the stream come from?
		StreamSourceType _source_type;

		std::shared_ptr<Application>	_app_info = nullptr;

		// If the Source Type of this stream is LiveTranscoder,
		// the original stream coming from the Provider can be recognized with _origin_stream_info.
		std::shared_ptr<StreamInfo> 	_origin_stream_info = nullptr;
	};
}  // namespace info