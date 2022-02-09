#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/ovlibrary/ovlibrary.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
};

class MpegtsTrackInfo
{
public:
	static std::shared_ptr<MpegtsTrackInfo> Create()
	{
		auto object = std::make_shared<MpegtsTrackInfo>();
		return object;
	}

	void SetCodecId(cmn::MediaCodecId codec_id)
	{
		_codec_id = codec_id;
	}
	cmn::MediaCodecId GetCodecId()
	{
		return _codec_id;
	}

	void SetBitrate(int32_t bitrate)
	{
		_bitrate = bitrate;
	}
	int32_t GetBitrate()
	{
		return _bitrate;
	}

	void SetTimeBase(cmn::Timebase timebase)
	{
		_timebase = timebase;
	}
	cmn::Timebase GetTimeBase()
	{
		return _timebase;
	}

	void SetWidth(int32_t width)
	{
		_width = width;
	}
	int32_t GetWidth()
	{
		return _width;
	}

	void SetHeight(int32_t height)
	{
		_height = height;
	}
	int32_t GetHeight()
	{
		return _height;
	}

	void SetSample(cmn::AudioSample sample)
	{
		_sample = sample;
	}
	cmn::AudioSample GetSample()
	{
		return _sample;
	}

	void SetChannel(cmn::AudioChannel channel)
	{
		_channel = channel;
	}
	cmn::AudioChannel GetChannel()
	{
		return _channel;
	}

	void SetExtradata(const std::shared_ptr<ov::Data>& extradata)
	{
		_extradata = extradata;
	}
	
	std::shared_ptr<ov::Data>& GetExtradata()
	{
		return _extradata;
	}

private:
	cmn::MediaCodecId _codec_id;
	int32_t _bitrate;
	cmn::Timebase _timebase;

	int32_t _width;
	int32_t _height;
	double _framerate;

	cmn::AudioSample _sample;
	cmn::AudioChannel _channel;

	std::shared_ptr<ov::Data> _extradata;
};

class MpegtsWriter
{
public:
	static std::shared_ptr<MpegtsWriter> Create();

	MpegtsWriter();
	~MpegtsWriter();

	// format is muxer(or container)
	// 	- mpegts
	//  - mp4
	bool SetPath(const ov::String path, const ov::String format = nullptr);
	ov::String GetPath();

	bool Start();

	bool Stop();

	bool AddTrack(cmn::MediaType media_type, int32_t track_id, std::shared_ptr<MpegtsTrackInfo> trackinfo);

	bool PutData(int32_t track_id, int64_t pts, int64_t dts, MediaPacketFlag flag, cmn::BitstreamFormat format, std::shared_ptr<ov::Data>& data);

	static void FFmpegLog(void* ptr, int level, const char* fmt, va_list vl);

private:
	ov::String _path;
	ov::String _format;

	AVFormatContext* _format_context;

	// <MediaTrack.id, std::hsared_ptr<MpegtsTrackInfo>>
	std::map<int32_t, std::shared_ptr<MpegtsTrackInfo>> _trackinfo_map;

	// Map of track
	// <MediaTrack.id, AVStream.index>
	std::map<int32_t, int64_t> _track_map;

	std::mutex _lock;
};
