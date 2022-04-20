#include "file_writer.h"

#include <modules/bitstream/h264/h264_converter.h>

#include "private.h"

/* 
	[Test Code]

	_writer = FileWriter::Create();
	_writer->SetPath("/tmp/output.ts");

	for(auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		if( track->GetCodecId() == cmn::MediaCodecId::Opus )
			continue;

		auto quality = FileTrackInfo::Create();

		quality->SetCodecId( track->GetCodecId() );
		quality->SetBitrate( track->GetBitrate() );
		quality->SetTimeBase( track->GetTimeBase() );
		quality->SetWidth( track->GetWidth() );
		quality->SetHeight( track->GetHeight() );
		quality->SetSample( track->GetSample() );
		quality->SetChannel( track->GetChannel() );

		_writer->AddTrack(track->GetMediaType(), track->GetId(), quality);
	}

	_writer->Start();

	while(1)
	{
		_writer->PutData(
			media_packet->GetTrackId(), 
			media_packet->GetPts(),
			media_packet->GetDts(), 
			media_packet->GetFlag(), 
			media_packet->GetData());
	}

	_writer->Stop();

*/

std::shared_ptr<FileWriter> FileWriter::Create()
{
	auto object = std::make_shared<FileWriter>();

	return object;
}

FileWriter::FileWriter()
	: _format_context(nullptr)
{
	_start_time = -1L;
	_timestamp_recalc_mode = TIMESTAMP_STARTZERO_MODE;
}

FileWriter::~FileWriter()
{
	Stop();
}

void FileWriter::SetTimestampRecalcMode(int mode)
{
	_timestamp_recalc_mode = mode;
}

int FileWriter::GetTimestampRecalcMode()
{
	return _timestamp_recalc_mode;
}

bool FileWriter::SetPath(const ov::String path, const ov::String format)
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	if (path.IsEmpty() == true)
	{
		logte("The path is empty");
		return false;
	}

	_path = path;

	// Release the allcated context;
	if (_format_context != nullptr)
	{
		avformat_close_input(&_format_context);
		avformat_free_context(_format_context);
		_format_context = nullptr;
	}

	AVOutputFormat *output_format = nullptr;

	// If the format is nullptr, it is automatically set based on the extension.
	if (format != nullptr)
	{
		output_format = av_guess_format(format.CStr(), nullptr, nullptr);
		if (output_format == nullptr)
		{
			logte("Unknown format. format(%s)", format.CStr());
			return false;
		}
	}

	int error = avformat_alloc_output_context2(&_format_context, output_format, nullptr, path.CStr());
	if (error < 0)
	{
		logte("Could not create output context. error(%d), path(%s)", error, path.CStr());

		return false;
	}

	return true;
}

ov::String FileWriter::GetPath()
{
	std::shared_lock<std::shared_mutex> mlock(_lock);

	return _path;
}

bool FileWriter::Start()
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	AVDictionary *options = nullptr;

	_start_time = -1LL;

	if (!(_format_context->oformat->flags & AVFMT_NOFILE))
	{
		int error = avio_open2(&_format_context->pb, _format_context->url, AVIO_FLAG_READ_WRITE, nullptr, &options);
		if (error < 0)
		{
			logte("Error opening file. error(%d), %s", error, _format_context->url);
			return false;
		}
	}

	if (avformat_write_header(_format_context, nullptr) < 0)
	{
		logte("Could not create header");
		return false;
	}

	av_dump_format(_format_context, 0, _format_context->url, 1);

	if (_format_context->oformat != nullptr)
	{
		[[maybe_unused]] auto oformat = _format_context->oformat;
		logtd("name : %s", oformat->name);
		logtd("long_name : %s", oformat->long_name);
		logtd("mime_type : %s", oformat->mime_type);
		logtd("audio_codec : %d", oformat->audio_codec);
		logtd("video_codec : %d", oformat->video_codec);
		logtd("subtitle_codec : %d", oformat->subtitle_codec);
	}

	return true;
}

bool FileWriter::Stop()
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	if (_format_context != nullptr)
	{
		ov::String path = _format_context->url;
		if (_format_context->pb != nullptr)
		{
			av_write_trailer(_format_context);
		}

		avformat_close_input(&_format_context);

		avformat_free_context(_format_context);

		_format_context = nullptr;

		if (chmod(path.CStr(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0)
		{
			logtw("Could not change permission. path(%s)", path.CStr());
		}
	}

	return true;
}

bool FileWriter::AddTrack(cmn::MediaType media_type, int32_t track_id, std::shared_ptr<FileTrackInfo> track)
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	AVStream *stream = nullptr;

	switch (media_type)
	{
		case cmn::MediaType::Video: {
			stream = avformat_new_stream(_format_context, nullptr);
			AVCodecParameters *codecpar = stream->codecpar;

			codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
			codecpar->codec_id = (track->GetCodecId() == cmn::MediaCodecId::H264) ? AV_CODEC_ID_H264 : (track->GetCodecId() == cmn::MediaCodecId::H265) ? AV_CODEC_ID_H265
																										: (track->GetCodecId() == cmn::MediaCodecId::Vp8)	  ? AV_CODEC_ID_VP8
																										: (track->GetCodecId() == cmn::MediaCodecId::Vp9)	  ? AV_CODEC_ID_VP9
																																								  : AV_CODEC_ID_NONE;

			codecpar->bit_rate = track->GetBitrate();
			codecpar->width = track->GetWidth();
			codecpar->height = track->GetHeight();
			codecpar->sample_aspect_ratio = AVRational{1, 1};
			codecpar->codec_tag = 0;

			// set extradata for avc_decoder_configuration_record
			if (track->GetExtradata() != nullptr)
			{
				codecpar->extradata_size = track->GetExtradata()->GetLength();
				codecpar->extradata = (uint8_t *)av_malloc(codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				memset(codecpar->extradata, 0, codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				memcpy(codecpar->extradata, track->GetExtradata()->GetDataAs<uint8_t>(), codecpar->extradata_size);
			}

			stream->time_base = AVRational{track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen()};

			_track_to_avstream[track_id] = stream->index;
			_tracks[track_id] = track;
		}
		break;

		case cmn::MediaType::Audio: {
			stream = avformat_new_stream(_format_context, nullptr);
			AVCodecParameters *codecpar = stream->codecpar;

			codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
			codecpar->codec_id = (track->GetCodecId() == cmn::MediaCodecId::Aac) ? AV_CODEC_ID_AAC : (track->GetCodecId() == cmn::MediaCodecId::Mp3) ? AV_CODEC_ID_MP3
																									  : (track->GetCodecId() == cmn::MediaCodecId::Opus)  ? AV_CODEC_ID_OPUS
																																							   : AV_CODEC_ID_NONE;
			codecpar->bit_rate = track->GetBitrate();
			codecpar->channels = static_cast<int>(track->GetChannel().GetCounts());
			codecpar->channel_layout = (track->GetChannel().GetLayout() == cmn::AudioChannel::Layout::LayoutMono) ? AV_CH_LAYOUT_MONO : (track->GetChannel().GetLayout() == cmn::AudioChannel::Layout::LayoutStereo) ? AV_CH_LAYOUT_STEREO
																																																							   : 0;	 // <- Unknown
			codecpar->sample_rate = track->GetSample().GetRateNum();
			codecpar->frame_size = 1024;  // Default Frame Size(1024)
			codecpar->codec_tag = 0;

			// set extradata for aac_specific_config
			if (track->GetExtradata() != nullptr)
			{
				codecpar->extradata_size = track->GetExtradata()->GetLength();
				codecpar->extradata = (uint8_t *)av_malloc(codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				memset(codecpar->extradata, 0, codecpar->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
				memcpy(codecpar->extradata, track->GetExtradata()->GetDataAs<uint8_t>(), codecpar->extradata_size);
			}

			stream->time_base = AVRational{track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen()};

			_track_to_avstream[track_id] = stream->index;
			_tracks[track_id] = track;
		}
		break;

		default: {
			logtw("This media type is not supported. media_type(%d)", media_type);
			return false;
		}
		break;
	}

	return true;
}

// MP4
//	- H264 : AnnexB bitstream
// 	- AAC : ASC(Audio Specific Config) bitstream

bool FileWriter::PutData(int32_t track_id, int64_t pts, int64_t dts, MediaPacketFlag flag, cmn::BitstreamFormat format, std::shared_ptr<ov::Data> &data)
{
	std::lock_guard<std::shared_mutex> mlock(_lock);

	if (_format_context == nullptr)
		return false;

	auto iter = _track_to_avstream.find(track_id);
	if (iter == _track_to_avstream.end())
	{
		// If track_id is not on the recording target list, drop it without errors.
		return true;
	}

	int32_t stream_index = iter->second;
	
	AVStream *stream = _format_context->streams[stream_index];
	if (stream == nullptr)
	{
		logtw("There is no stream");
		return false;
	}
	
	// Find TrackInfo
	auto track = _tracks[track_id];


	if (GetTimestampRecalcMode() == TIMESTAMP_STARTZERO_MODE)
	{
		if (_start_time == -1LL)
		{
			// Convert media track's timebase to standard timebase
			_start_time = av_rescale_q(pts, AVRational{track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen()}, AV_TIME_BASE_Q);
		}
	}
	else if (GetTimestampRecalcMode() == TIMESTAMP_PASSTHROUGH_MODE)
	{
		_start_time = 0LL;
	}

	// / Convert standard timebase to media track's timebase
	int64_t start_time = av_rescale_q(_start_time, AV_TIME_BASE_Q, AVRational{track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen()});

	AVPacket av_packet = {0};
	av_packet.stream_index = stream_index;
	av_packet.flags = (flag == MediaPacketFlag::Key) ? AV_PKT_FLAG_KEY : 0;
	av_packet.pts = av_rescale_q(pts - start_time, AVRational{track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen()}, stream->time_base);
	av_packet.dts = av_rescale_q(dts - start_time, AVRational{track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen()}, stream->time_base);

	logtp("%d] %lld / %10lld[%d/%5d] -> %10lld[%d/%5d]",
		  stream_index, _start_time,
		  pts, track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen(),
		  av_packet.pts, stream->time_base.num, stream->time_base.den);

	// Depending on the extension, the bitstream format should be changed.
	// format(mpegts)
	// 	- H264 : Passthrough (AnnexB)
	//  - H265 : Passthrough (AnnexB)
	//  - AAC : Passthrough (ADTS)
	//  - OPUS : Passthrough 
	//  - VP8 : Passthrough 
	// format(mp4)
	//	- H264 : AVCC
	//	- AAC : to RA
	// format(webm)
	//	- VP8 : Passthrough
	//	- VP9 : Passthrough
	//  - OPUS : Passthrough
	uint8_t ADTS_HEADER_LENGTH = 7;

	std::shared_ptr<const ov::Data> cdata = nullptr;
	if (strcmp(_format_context->oformat->name, "mp4") == 0)
	{
		switch (format)
		{
			case cmn::BitstreamFormat::H264_ANNEXB:
				cdata = H264Converter::ConvertAnnexbToAvcc(data);
				av_packet.size = cdata->GetLength();
				av_packet.data = (uint8_t *)cdata->GetDataAs<uint8_t>();
				break;
			case cmn::BitstreamFormat::AAC_ADTS:
				// Skip ADTS header
				av_packet.size = data->GetLength() - ADTS_HEADER_LENGTH;
				av_packet.data = (uint8_t *)data->GetDataAs<uint8_t>() + ADTS_HEADER_LENGTH;
				break;
			case cmn::BitstreamFormat::AAC_RAW:
			case cmn::BitstreamFormat::H264_AVCC:
				av_packet.size = data->GetLength();
				av_packet.data = (uint8_t *)data->GetDataAs<uint8_t>();
				break;
			case cmn::BitstreamFormat::AAC_LATM:
			default: {
				logte("Could not support bitstream format");
				return false;
			}
		}
	}
	else if (strcmp(_format_context->oformat->name, "mpegts") == 0)
	{
		av_packet.size = data->GetLength();
		av_packet.data = (uint8_t *)data->GetDataAs<uint8_t>();
	}
	else if (strcmp(_format_context->oformat->name, "webm") == 0)
	{
		av_packet.size = data->GetLength();
		av_packet.data = (uint8_t *)data->GetDataAs<uint8_t>();
	}
	else
	{
		logte("Could not support output file format");
		return false;
	}

	int ret = av_interleaved_write_frame(_format_context, &av_packet);
	if (ret != 0)
	{
		logte("error = %d", ret);
		return false;
	}

	return true;
}

bool FileWriter::IsWritable()
{
	std::shared_lock<std::shared_mutex> mlock(_lock);

	if (_format_context == nullptr)
	{
		return false;
	}

	return true;
}

ov::String FileWriter::GetFormatByExtension(ov::String extension, ov::String default_format)
{
	if (extension == "mp4")
	{
		return "mp4";
	}
	else if (extension == "ts")
	{
		return "mpegts";
	}
	else if (extension == "webm")
	{
		return "webm";
	}

	return default_format;
}

bool FileWriter::IsSupportCodec(ov::String format, cmn::MediaCodecId codec_id)
{
	if (format == "mp4")
	{
		if (codec_id == cmn::MediaCodecId::H264 ||
			codec_id == cmn::MediaCodecId::H265 ||
			codec_id == cmn::MediaCodecId::Aac ||
			codec_id == cmn::MediaCodecId::Mp3)
		{
			return true;
		}
	}
	else if (format == "mpegts")
	{
		if (codec_id == cmn::MediaCodecId::H264 ||
			codec_id == cmn::MediaCodecId::H265 ||
			codec_id == cmn::MediaCodecId::Vp8 ||
			codec_id == cmn::MediaCodecId::Vp9 ||
			codec_id == cmn::MediaCodecId::Aac ||
			codec_id == cmn::MediaCodecId::Mp3 ||
			codec_id == cmn::MediaCodecId::Opus)
		{
			return true;
		}
	}
	else if (format == "webm")
	{
		if (codec_id == cmn::MediaCodecId::Vp8 ||
			codec_id == cmn::MediaCodecId::Vp9 ||
			codec_id == cmn::MediaCodecId::Opus)
		{
			return true;
		}
	}

	return false;
}
