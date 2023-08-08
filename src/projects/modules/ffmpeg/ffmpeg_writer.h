#pragma once

#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>
#include <base/ovlibrary/ovlibrary.h>

extern "C"
{
#include <libavformat/avformat.h>
};

namespace ffmpeg
{
	class Writer
	{
	public:
		enum TimestampMode
		{
			TIMESTAMP_STARTZERO_MODE = 0,
			TIMESTAMP_PASSTHROUGH_MODE = 1
		};

	public:
		static std::shared_ptr<Writer> Create();

		Writer();
		~Writer();

		// format is muxer(or container)
		// 	- RTMP : flv
		// 	- MPEGTS : mpegts, mp4
		//  - SRT : mpegts
		bool SetUrl(const ov::String url, const ov::String format = nullptr);
		ov::String GetUrl();

		bool Start();
		bool Stop();

		bool AddTrack(std::shared_ptr<MediaTrack> media_track);
		bool SendPacket(std::shared_ptr<MediaPacket> packet);

		void SetTimestampMode(TimestampMode mode);
		TimestampMode GetTimestampMode();

	private:
		ov::String _url;
		ov::String _format;

		int64_t _start_time = -1LL;
		TimestampMode _timestamp_mode = TIMESTAMP_STARTZERO_MODE;

		bool _need_to_flush = false;
		bool _need_to_close = false;

		// MediaTrackId -> AVStream Index
		std::map<int32_t, int64_t> _id_map;
		// MediaTrackId -> MediaTrack
		std::map<int32_t, std::shared_ptr<MediaTrack>> _track_map;

		AVFormatContext* _av_format = nullptr;

		std::mutex _lock;
	};
}  // namespace ffmpeg