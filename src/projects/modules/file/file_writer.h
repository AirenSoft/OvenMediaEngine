#pragma once

#include <base/ovsocket/socket_address.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/mediarouter/media_buffer.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
};


class FileTrackQuality {
public:    
	static std::shared_ptr<FileTrackQuality> Create() {
		auto object = std::make_shared<FileTrackQuality>();
		return object;
	}

	void SetCodecId(common::MediaCodecId codec_id) { _codec_id = codec_id; }
	common::MediaCodecId GetCodecId() { return _codec_id; }

	void SetBitrate(int32_t bitrate) { _bitrate = bitrate; }
	int32_t GetBitrate() { return _bitrate; }

	void SetTimeBase(common::Timebase timebase) { _timebase = timebase; }
	common::Timebase GetTimeBase() { return _timebase; }

	void SetWidth(int32_t width) { _width = width; }
	int32_t GetWidth() { return _width; }

	void SetHeight(int32_t height) { _height = height; }
	int32_t GetHeight() { return _height; }

	void SetSample(common::AudioSample sample) { _sample = sample; }
	common::AudioSample GetSample() { return _sample; }

	void SetChannel(common::AudioChannel channel) { _channel = channel; }
	common::AudioChannel GetChannel() { return _channel; }


private:
	common::MediaCodecId    _codec_id;
	int32_t                 _bitrate;
	common::Timebase        _timebase;

	int32_t                 _width;
	int32_t                 _height;
	double                  _framerate;

	common::AudioSample     _sample;
	common::AudioChannel    _channel;
};

class FileWriter
{
public:
	static std::shared_ptr<FileWriter> Create();

	FileWriter();
	~FileWriter();

	// format is muxer(or container)
	// 	- mpegts
	//  - mp4
	bool SetPath(const ov::String path, const ov::String format = nullptr);
	ov::String GetPath();
	
	bool Start();

	bool Stop();

	bool AddTrack(common::MediaType media_type, int32_t track_id, std::shared_ptr<FileTrackQuality> quality);

	bool PutData(int32_t track_id, int64_t pts, int64_t dts, MediaPacketFlag flag, std::shared_ptr<ov::Data>& data);

	static void FFmpegLog(void *ptr, int level, const char *fmt, va_list vl);

private:
	ov::String 					_path;
	ov::String 					_format;

	AVFormatContext* 			_format_context;

	// Map of track
	// <MediaTrack.id, AVStream.index>
	std::map<int32_t, int64_t> 	_track_map;

	std::mutex 					_lock;

};
