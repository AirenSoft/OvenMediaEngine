//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "writer.h"

extern "C"
{
#include <libavutil/avassert.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/h264/h264_converter.h>
#include <modules/bitstream/h265/h265_converter.h>

#define OV_LOG_TAG "Writer"

#define logad(format, ...) logtd("[%p] " format, this, ##__VA_ARGS__)
#define logas(format, ...) logts("[%p] " format, this, ##__VA_ARGS__)

#define logai(format, ...) logti("[%p] " format, this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%p] " format, this, ##__VA_ARGS__)
#define logae(format, ...) logte("[%p] " format, this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[%p] " format, this, ##__VA_ARGS__)

static AVRational RationalFromTimebase(const cmn::Timebase &timebase)
{
	return {timebase.GetNum(), timebase.GetDen()};
}

static ov::String StringFromTs(int64_t timestamp)
{
	ov::String str = (timestamp == AV_NOPTS_VALUE) ? "NOPTS" : ov::String::FormatString("%" PRId64, timestamp);
	return std::move(str);
}

static ov::String StringFromTime(int64_t timestamp, const AVRational *time_base)
{
	ov::String str = (timestamp == AV_NOPTS_VALUE) ? "NOPTS" : ov::String::FormatString("%.6g", ::av_q2d(*time_base) * timestamp);
	return std::move(str);
}

static ov::String StringFromError(int code)
{
	char str[AV_TS_MAX_STRING_SIZE]{0};
	::av_strerror(code, str, AV_TS_MAX_STRING_SIZE);
	return str;
}

Writer::Writer(Type type, MediaType media_type)
	: _type(type),
	  _media_type(media_type)
{
}

Writer::~Writer()
{
	OV_ASSERT((_data_stream == nullptr), "It seems to Writer is not finalized");
}

std::shared_ptr<AVFormatContext> Writer::CreateFormatContext(Type type)
{
	const char *format = nullptr;

	switch (type)
	{
		case Type::MpegTs:
			format = "mpegts";
			break;

		case Type::M4s:
			format = "mp4";
			break;
	}

	if (format == nullptr)
	{
		// Unknown type
		return nullptr;
	}

	AVFormatContext *format_context = nullptr;

	::avformat_alloc_output_context2(&format_context, nullptr, format, nullptr);

	if (format_context == nullptr)
	{
		logae("Could not create format context");
		return nullptr;
	}

	return std::shared_ptr<AVFormatContext>(format_context, [](AVFormatContext *format_context) {
		if (format_context != nullptr)
		{
			::avformat_free_context(format_context);
		}
	});
}

std::shared_ptr<AVIOContext> Writer::CreateAvioContext()
{
	auto buffer = static_cast<unsigned char *>(::av_malloc(32768));
	auto avio_context = ::avio_alloc_context(buffer, 32768, 1, this, nullptr, OnWrite, OnSeek);

	return std::shared_ptr<AVIOContext>(avio_context, [](AVIOContext *avio_context) {
		if (avio_context != nullptr)
		{
			OV_SAFE_FUNC(avio_context->buffer, nullptr, ::av_free, );
			::avio_context_free(&avio_context);
		}
	});
}

void Writer::LogPacket(const AVPacket *packet) const
{
	// Retains _format_context
	auto format_context = _format_context;
	const AVRational *time_base = &(format_context->streams[packet->stream_index]->time_base);

	logai("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d",
		  StringFromTs(packet->pts).CStr(), StringFromTime(packet->pts, time_base).CStr(),
		  StringFromTs(packet->dts).CStr(), StringFromTime(packet->dts, time_base).CStr(),
		  StringFromTs(packet->duration).CStr(), StringFromTime(packet->duration, time_base).CStr(),
		  packet->stream_index);
}

#define WRITER_CASE(cond, state) \
	case cond:                   \
		return state;

static AVCodecID AvCodecIdFromMediaCodecId(cmn::MediaCodecId codec_id)
{
	switch (codec_id)
	{
		WRITER_CASE(cmn::MediaCodecId::None, AV_CODEC_ID_NONE)
		WRITER_CASE(cmn::MediaCodecId::H264, AV_CODEC_ID_H264)
		WRITER_CASE(cmn::MediaCodecId::H265, AV_CODEC_ID_H265)
		WRITER_CASE(cmn::MediaCodecId::Vp8, AV_CODEC_ID_VP8)
		WRITER_CASE(cmn::MediaCodecId::Vp9, AV_CODEC_ID_VP9)
		WRITER_CASE(cmn::MediaCodecId::Flv, AV_CODEC_ID_FLV1)
		WRITER_CASE(cmn::MediaCodecId::Aac, AV_CODEC_ID_AAC)
		WRITER_CASE(cmn::MediaCodecId::Mp3, AV_CODEC_ID_MP3)
		WRITER_CASE(cmn::MediaCodecId::Opus, AV_CODEC_ID_OPUS)
		WRITER_CASE(cmn::MediaCodecId::Jpeg, AV_CODEC_ID_JPEG2000)
		WRITER_CASE(cmn::MediaCodecId::Png, AV_CODEC_ID_PNG)
	}

	return AV_CODEC_ID_NONE;
}

static int AvChannelLayoutFromAudioChannelLayout(const cmn::AudioChannel::Layout &layout)
{
	switch (layout)
	{
		WRITER_CASE(cmn::AudioChannel::Layout::LayoutUnknown, 0)
		WRITER_CASE(cmn::AudioChannel::Layout::LayoutMono, AV_CH_LAYOUT_MONO)
		WRITER_CASE(cmn::AudioChannel::Layout::LayoutStereo, AV_CH_LAYOUT_STEREO)
	}

	return 0;
}

bool Writer::FillCodecParameters(const std::shared_ptr<const Track> &track, AVCodecParameters *codec_parameters)
{
	auto media_track = track->track;

	if (media_track == nullptr)
	{
		return false;
	}

	switch (media_track->GetMediaType())
	{
		case cmn::MediaType::Unknown:
			return false;

		case cmn::MediaType::Data:
			[[fallthrough]];
		case cmn::MediaType::Subtitle:
			[[fallthrough]];
		case cmn::MediaType::Attachment:
			[[fallthrough]];
		case cmn::MediaType::Nb:
			break;

		case cmn::MediaType::Video: {
			codec_parameters->codec_type = AVMEDIA_TYPE_VIDEO;
			codec_parameters->codec_id = AvCodecIdFromMediaCodecId(media_track->GetCodecId());
			codec_parameters->bit_rate = media_track->GetBitrate();
			codec_parameters->width = media_track->GetWidth();
			codec_parameters->height = media_track->GetHeight();
			codec_parameters->format = media_track->GetFormat();
			if (media_track->GetCodecId() == cmn::MediaCodecId::H265)
			{
				codec_parameters->codec_tag = MKTAG('h', 'v', 'c', '1');
			}
			else
			{
				codec_parameters->codec_tag = 0;
			}

			auto &extra_data = media_track->GetCodecExtradata();
			if (extra_data.size() > 0)
			{
				codec_parameters->extradata_size = extra_data.size();
				codec_parameters->extradata = static_cast<uint8_t *>(::av_malloc(codec_parameters->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
				::memset(codec_parameters->extradata, 0, codec_parameters->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				::memcpy(codec_parameters->extradata, extra_data.data(), codec_parameters->extradata_size);
			}

			// _output_format->video_codec = codec_parameters->codec_id;

			break;
		}

		case cmn::MediaType::Audio: {
			codec_parameters->codec_type = AVMEDIA_TYPE_AUDIO;
			codec_parameters->codec_id = AvCodecIdFromMediaCodecId(media_track->GetCodecId());
			codec_parameters->bit_rate = media_track->GetBitrate();
			codec_parameters->channels = static_cast<int>(media_track->GetChannel().GetCounts());
			codec_parameters->channel_layout = AvChannelLayoutFromAudioChannelLayout(media_track->GetChannel().GetLayout());
			codec_parameters->sample_rate = media_track->GetSample().GetRateNum();
			codec_parameters->frame_size = 1024;
			codec_parameters->format = static_cast<int>(media_track->GetSample().GetFormat());
			codec_parameters->codec_tag = 0;

			auto &extra_data = media_track->GetCodecExtradata();
			if (extra_data.size() > 0)
			{
				codec_parameters->extradata_size = extra_data.size();
				codec_parameters->extradata = static_cast<uint8_t *>(::av_malloc(codec_parameters->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
				::memset(codec_parameters->extradata, 0, codec_parameters->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				::memcpy(codec_parameters->extradata, extra_data.data(), codec_parameters->extradata_size);
			}

			// _output_format->audio_codec = codec_parameters->codec_id;

			break;
		}
	}

	return true;
}

int Writer::OnWrite(const uint8_t *buf, int buf_size)
{
	logad("Writing %d bytes", buf_size);

	auto data_stream = _data_stream;
	if (data_stream != nullptr)
	{
		if (data_stream->Write(buf, buf_size))
		{
			return buf_size;
		}
	}

	return -1;
}

int64_t Writer::OnSeek(int64_t offset, int whence)
{
	auto data_stream = _data_stream;
	auto data = data_stream->GetData();

	int64_t new_offset = -1;
	int64_t length = static_cast<int64_t>(data->GetLength());

	switch (whence)
	{
		case SEEK_SET:
			new_offset = offset;
			break;

		case SEEK_CUR:
			new_offset = data_stream->GetOffset() + offset;
			break;

		case SEEK_END:
			new_offset = length + offset;
			break;
	}

	if (data_stream->SetOffset(new_offset) == false)
	{
		// Invalid seek offset
		return -1;
	}

	return 0;
}

bool Writer::AddTrack(const std::shared_ptr<const MediaTrack> &media_track)
{
	if (media_track == nullptr)
	{
		return false;
	}

	auto lock_guard = std::lock_guard(_track_mutex);

	auto track = std::make_shared<Track>(_track_list.size(), RationalFromTimebase(media_track->GetTimeBase()), media_track);
	_track_list.emplace_back(track);
	_track_map[media_track->GetId()] = track;

	logad("Track %s is added", ::StringFromMediaType(media_track->GetMediaType()).CStr());

	return true;
}

bool Writer::Prepare()
{
	if (_format_context != nullptr)
	{
		logae("Writer is already started");
		return false;
	}

	auto format_context = CreateFormatContext(_type);

	if (format_context == nullptr)
	{
		logae("Could not allocate format context");
		return false;
	}

	// Prepare AVStreams
	{
		auto lock_guard = std::lock_guard(_track_mutex);
		int index = 0;

		for (auto &track : _track_list)
		{
			auto stream = ::avformat_new_stream(format_context.get(), nullptr);

			if (stream == nullptr)
			{
				logae("Could not allocate stream");
				return false;
			}

			stream->id = index;
			stream->time_base = track->rational;

			if (FillCodecParameters(track, stream->codecpar) == false)
			{
				logae("Could not fill codec parameters");
				return false;
			}

			index++;
		}
	}

	auto avio_context = CreateAvioContext();

	if (avio_context == nullptr)
	{
		logae("Could not allocate avio context");
		return false;
	}

	format_context->pb = avio_context.get();

	::av_dict_set(&(format_context->metadata), "service_name", "OvenMediaEngine", 0);
	::av_dict_set(&(format_context->metadata), "service_provider", "OvenMediaEngine", 0);

	AVDictionary *options = nullptr;

	switch (_type)
	{
		case Type::MpegTs:
			break;

		case Type::M4s:
			// +dash: Creates data for DASH streaming
			// +frag_custom: Writes packets when flushing is requested
			// +skip_trailer: No trailer
			::av_dict_set(&options, "movflags", "+dash+frag_custom+skip_trailer", 0);
			::av_dict_set(&options, "brand", "mp42", 0);
			// ::av_dict_set(&options, "movflags", "+dash+frag_custom+delay_moov+skip_trailer", 0);
			// ::av_dict_set_int(&options, "use_editlist", 1, 0);
			break;
	}

	int result = ::avformat_init_output(format_context.get(), &options);

	if (result < 0)
	{
		logae("Could not init output: %s", StringFromError(result).CStr());
		return false;
	}

	// ::av_dump_format(format_context.get(), 0, "Writer", 1);

	if (ResetData() == false)
	{
		return false;
	}

	result = ::avformat_write_header(format_context.get(), &_options);
	if (result < 0)
	{
		logae("Could not write header: %s", StringFromError(result).CStr());
		return false;
	}

	_format_context = format_context;
	_avio_context = avio_context;
	_output_format = format_context->oformat;

	return true;
}

bool Writer::PrepareIfNeeded()
{
	if (_format_context != nullptr)
	{
		return true;
	}

	return Prepare();
}

bool Writer::ResetData(int track_id)
{
	_data_stream = std::make_shared<ov::ByteStream>(std::make_shared<ov::Data>());

	{
		auto lock_guard = std::lock_guard(_track_mutex);

		if (track_id == -1)
		{
			for (auto track : _track_list)
			{
				track->Reset();
			}
		}
		else
		{
			auto item = _track_map.find(track_id);

			if (item == _track_map.end())
			{
				return false;
			}

			item->second->Reset();
		}
	}

	return true;
}

std::shared_ptr<const ov::Data> Writer::GetData() const
{
	auto data_stream = _data_stream;

	if (data_stream != nullptr)
	{
		return data_stream->GetDataPointer();
	}

	return nullptr;
}

off_t Writer::GetCurrentOffset() const
{
	auto data_stream = _data_stream;

	if (data_stream != nullptr)
	{
		return data_stream->GetOffset();
	}

	return 0;
}

int64_t Writer::GetFirstPts(uint32_t track_id) const
{
	auto lock_guard = std::lock_guard(_track_mutex);
	auto track_item = _track_map.find(track_id);

	if (track_item != _track_map.end())
	{
		auto &track = track_item->second;
		return (track->first_packet_received) ? track->first_pts : 0L;
	}

	return 0L;
}

int64_t Writer::GetFirstPts(cmn::MediaType type) const
{
	auto lock_guard = std::lock_guard(_track_mutex);
	for (auto &track_item : _track_list)
	{
		if (track_item->track->GetMediaType() == type)
		{
			return track_item->first_pts;
		}
	}

	return 0L;
}

int64_t Writer::GetFirstPts() const
{
	auto lock_guard = std::lock_guard(_track_mutex);
	if (_track_list.size() > 0)
	{
		auto &track = _track_list[0];
		return (track->first_packet_received) ? track->first_pts : 0L;
	}

	return 0L;
}

int64_t Writer::GetDuration(uint32_t track_id) const
{
	auto lock_guard = std::lock_guard(_track_mutex);
	auto track_item = _track_map.find(track_id);

	if (track_item != _track_map.end())
	{
		return track_item->second->duration;
	}

	return 0L;
}

int64_t Writer::GetDuration() const
{
	auto lock_guard = std::lock_guard(_track_mutex);
	if (_track_list.size() > 0)
	{
		return _track_list[0]->duration;
	}

	return 0L;
}

bool Writer::WritePacket(const std::shared_ptr<const MediaPacket> &packet, const std::shared_ptr<const ov::Data> &data, const std::vector<size_t> &length_list, size_t skip_count, size_t split_count)
{
	auto format_context = _format_context;

	std::shared_ptr<Writer::Track> track;

	{
		auto lock_guard = std::lock_guard(_track_mutex);
		auto track_item = _track_map.find(packet->GetTrackId());

		if (track_item == _track_map.end())
		{
			OV_ASSERT2(false);
			logac("Could not find the track in _track_map: %d (%zu)", packet->GetTrackId(), _track_map.size());
			return false;
		}

		track = track_item->second;

		OV_ASSERT2(track != nullptr);
	}

	if (data == nullptr)
	{
		logte("Could not convert packet: %d (writer type: %d)",
			  static_cast<int>(packet->GetBitstreamFormat()),
			  static_cast<int>(_type));

		return false;
	}

	int stream_index = track->stream_index;

	if ((format_context == nullptr) || (stream_index < 0) || (stream_index >= static_cast<int>(format_context->nb_streams)))
	{
		OV_ASSERT2(false);
		logac("Format context: %p, Stream index: %d, Number of streams: %u", format_context.get(), stream_index, format_context->nb_streams);
		return false;
	}

	if ((length_list.size() == 0) || (split_count == 0))
	{
		OV_ASSERT2(false);
		logac("length_list must contains at least 1 item");
		return false;
	}

	AVStream *stream = format_context->streams[stream_index];
	auto pts = packet->GetPts();
	auto dts = packet->GetDts();
	auto duration_per_packet = packet->GetDuration() / split_count;
	pts += (duration_per_packet * skip_count);
	dts += (duration_per_packet * skip_count);
	// It's muxing only, so we can change the constness temporary
	auto buffer = const_cast<uint8_t *>(data->GetDataAs<uint8_t>());

	for (auto length : length_list)
	{
		AVPacket av_packet;
		::av_init_packet(&av_packet);

		av_packet.stream_index = stream_index;
		av_packet.flags = (packet->GetFlag() == MediaPacketFlag::Key) ? AV_PKT_FLAG_KEY : 0;
		av_packet.pts = pts;
		av_packet.dts = dts;
		av_packet.size = length;
		av_packet.duration = duration_per_packet;
		av_packet.data = buffer;

		// logaw("#%d [%s] Writing a packet: %15ld, %15ld (dur: %ld, %zu)",
		// 	  track->track->GetId(), (track->track->GetMediaType() == cmn::MediaType::Video) ? "V" : "A",
		// 	  pts, dts, duration_per_packet, length_list.size());

		::av_packet_rescale_ts(&av_packet, track->rational, stream->time_base);

		// LogPacket(&av_packet);

		int result = ::av_interleaved_write_frame(format_context.get(), &av_packet);

		if (result != 0)
		{
			logae("Could not write the frame: %s", StringFromError(result).CStr());

			return false;
		}

		buffer += length;
		pts += duration_per_packet;
		dts += duration_per_packet;

		track->duration += duration_per_packet;

		if (track->first_packet_received == false)
		{
			track->first_pts = pts;
			track->first_packet_received = true;
		}
	}

	return true;
}

bool Writer::WritePacket(const std::shared_ptr<const MediaPacket> &packet)
{
	std::shared_ptr<const ov::Data> data;
	std::vector<size_t> length_list;

	switch (packet->GetBitstreamFormat())
	{
		case cmn::BitstreamFormat::H264_AVCC:
			data = packet->GetData();
			length_list.push_back(data->GetLength());
			break;

		case cmn::BitstreamFormat::H264_ANNEXB:
			data = packet->GetData();
			if (_type == Type::M4s)
			{
				data = H264Converter::ConvertAnnexbToAvcc(data);
			}
			length_list.push_back(data->GetLength());
			break;

		case cmn::BitstreamFormat::H265_ANNEXB:
			data = packet->GetData();
			length_list.push_back(data->GetLength());
			break;

		case cmn::BitstreamFormat::AAC_LATM:
			data = packet->GetData();
			length_list.push_back(data->GetLength());
			break;

		case cmn::BitstreamFormat::AAC_ADTS:
			if (_type != Type::M4s)
			{
				data = packet->GetData();
				length_list.push_back(data->GetLength());
			}
			else
			{
				data = AacConverter::ConvertAdtsToLatm(packet->GetData(), &length_list);
			}
			break;

		case cmn::BitstreamFormat::Unknown:
			[[fallthrough]];
		case cmn::BitstreamFormat::VP8:
			[[fallthrough]];
		case cmn::BitstreamFormat::OPUS:
			[[fallthrough]];
		case cmn::BitstreamFormat::JPEG:
			[[fallthrough]];
		case cmn::BitstreamFormat::PNG:
			// Not supported
			break;
	}

	return WritePacket(packet, data, length_list, 0, length_list.size());
}

bool Writer::Flush()
{
	auto format_context = _format_context;

	if (format_context != nullptr)
	{
		int result = ::av_write_frame(format_context.get(), nullptr);
		::avio_flush(format_context->pb);

		if (result >= 0)
		{
			return true;
		}

		logae("%s", ::StringFromError(result).CStr());
	}
	else
	{
		logae("Format context is nullptr");
	}

	return false;
}

std::shared_ptr<const ov::Data> Writer::Finalize()
{
	auto format_context = _format_context;
	if (format_context != nullptr)
	{
		Flush();

		::av_write_trailer(format_context.get());
	}

	_format_context = nullptr;
	_avio_context = nullptr;
	_output_format = nullptr;

	auto data_stream = _data_stream;
	_data_stream = nullptr;

	if (data_stream != nullptr)
	{
		auto data = data_stream->GetDataPointer();

		return std::move(data);
	}

	return nullptr;
}
