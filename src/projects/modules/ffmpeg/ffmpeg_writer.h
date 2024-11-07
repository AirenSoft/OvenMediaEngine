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

		enum WriterState
		{
			WriterStateNone = 0,
			WriterStateConnecting = 1,
			WriterStateConnected = 2,
			WriterStateError = 3,
			WriterStateClosed = 4
		};

	public:
		static std::shared_ptr<Writer> Create();

		Writer();
		~Writer();

		static int InterruptCallback(void *opaque);

		// format is muxer(or container)
		// 	- RTMP : flv
		// 	- MPEGTS : mpegts, mp4
		//  - SRT : mpegts
		bool SetUrl(const ov::String url, const ov::String format = nullptr);
		ov::String GetUrl();

		bool Start();
		bool Stop();

		bool AddTrack(const std::shared_ptr<MediaTrack> &media_track);
		bool SendPacket(const std::shared_ptr<MediaPacket> &packet, uint64_t *sent_bytes = nullptr);
		
		std::chrono::high_resolution_clock::time_point GetLastPacketSentTime();

		void SetTimestampMode(TimestampMode mode);
		TimestampMode GetTimestampMode();

		void SetState(WriterState state);
		WriterState GetState();
		
	private:
		std::shared_ptr<AVFormatContext> GetAVFormatContext() const;
		void SetAVFormatContext(AVFormatContext* av_format);
		void ReleaseAVFormatContext();
		std::pair<std::shared_ptr<AVStream>, std::shared_ptr<MediaTrack>> GetTrack(int32_t track_id) const;

		WriterState _state;

		ov::String _url;
		ov::String _format;

		int64_t _start_time = -1LL;
		TimestampMode _timestamp_mode = TIMESTAMP_STARTZERO_MODE;

		bool _need_to_flush = false;
		bool _need_to_close = false;

		// MediaTrackId -> AVStream, MediaTrack
		std::map<int32_t, std::pair<std::shared_ptr<AVStream>, std::shared_ptr<MediaTrack>>> _track_map;
		mutable std::shared_mutex _track_map_lock;

		std::shared_ptr<AVFormatContext> _av_format = nullptr;
		mutable std::shared_mutex _av_format_lock;

		ov::String _output_format_name;
		AVIOInterruptCB _interrupt_cb;
		std::chrono::high_resolution_clock::time_point _last_packet_sent_time;
		int32_t _connection_timeout = 5000;	// 5s
		int32_t _send_timeout 		= 1000;	// 1s
	};
}  // namespace ffmpeg