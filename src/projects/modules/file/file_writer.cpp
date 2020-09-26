#include <base/ovlibrary/converter.h>

#include "file_writer.h"
#include "private.h"

/* 
	[Test Code]

	_writer = FileWriter::Create();
	_writer->SetPath("/tmp/output.ts");

	for(auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		if( track->GetCodecId() == common::MediaCodecId::Opus )
			continue;

		auto quality = FileTrackQuality::Create();

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

FileWriter::FileWriter() :
	_format_context(nullptr)
{
	av_register_all();
	// av_log_set_callback(FileWriter::FFmpegLog);
	// av_log_set_level(AV_LOG_TRACE);
}

FileWriter::~FileWriter()
{
	Stop();
}


bool FileWriter::SetPath(const ov::String path, const ov::String format)
{
	std::unique_lock<std::mutex> mlock(_lock);

	if(path.IsEmpty() == true)
	{
		logte("The path is empty");
		return false;
	}

	_path = path;

	// Release the allcated context;
	if(_format_context != nullptr)
	{
		avformat_close_input(&_format_context);
		avformat_free_context(_format_context);
		_format_context = nullptr;
	}

	AVOutputFormat *output_format = nullptr;

	// If the format is nullptr, it is automatically set based on the extension.
	if(format != nullptr)
	{
		output_format = av_guess_format(format.CStr(), nullptr, nullptr);
		if(output_format == nullptr)
		{
			logte("Unknown format. format(%s)", format.CStr());
			return false;
		}
	}

	int error = avformat_alloc_output_context2(&_format_context, output_format, nullptr, path.CStr());
	if(error < 0)
	{
		logte("Could not create output context. error(%d), path(%s)", error, path.CStr());	

		return false;
	}	

	return true;
}

ov::String FileWriter::GetPath()
{
	return _path;
}

bool FileWriter::Start()
{
	std::unique_lock<std::mutex> mlock(_lock);

	AVDictionary *options = nullptr;

	// Examples
	// av_dict_set(&out_options, "timeout", "1000000", 0);
	// av_dict_set(&out_options, "tcp_nodelay", "1", 0);
	// _format_context->flags = AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_FLUSH_PACKETS;

	if (!(_format_context->oformat->flags & AVFMT_NOFILE))
	{
		int error = avio_open2(&_format_context->pb, _format_context->filename, AVIO_FLAG_WRITE, nullptr, &options);
		if (error < 0)
		{
			logte("Error opening file. error(%d), %s", error, _format_context->filename);
			return false;
		}
	}

	if(avformat_write_header(_format_context, nullptr) < 0)
	{
		logte("Could not create header");
		return false;
	}

	av_dump_format(_format_context, 0, _format_context->filename, 1);

	if(_format_context->oformat != nullptr)
	{
		auto oformat = _format_context->oformat;
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
	std::unique_lock<std::mutex> mlock(_lock);

	if(_format_context != nullptr)
	{
		av_write_trailer(_format_context);

		avformat_close_input(&_format_context);

		avformat_free_context(_format_context);

		_format_context = nullptr;
	}

	return true;
}

bool FileWriter::AddTrack(common::MediaType media_type, int32_t track_id, std::shared_ptr<FileTrackQuality> quality)
{
	std::unique_lock<std::mutex> mlock(_lock);

	AVStream *stream = nullptr; 

	switch(media_type)
	{
		case common::MediaType::Video:
		{
			stream = avformat_new_stream(_format_context, nullptr);
			AVCodecParameters *codecpar = stream->codecpar;

			codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
			codecpar->codec_id = 
				(quality->GetCodecId() == common::MediaCodecId::H264)?AV_CODEC_ID_H264:
				(quality->GetCodecId() == common::MediaCodecId::H265)?AV_CODEC_ID_H265:
				(quality->GetCodecId() == common::MediaCodecId::Vp8)?AV_CODEC_ID_VP8:
				(quality->GetCodecId() == common::MediaCodecId::Vp9)?AV_CODEC_ID_VP9:
				AV_CODEC_ID_NONE;		
				
			codecpar->bit_rate = quality->GetBitrate();
			codecpar->width = quality->GetWidth();
			codecpar->height = quality->GetHeight();
			codecpar->sample_aspect_ratio = AVRational{1, 1};
			codecpar->codec_tag = 0;
			
			stream->display_aspect_ratio = AVRational{1, 1};
			stream->time_base = AVRational{quality->GetTimeBase().GetNum(), quality->GetTimeBase().GetDen()};	

			// Reserved					
			// codecpar->profile = FF_PROFILE_H264_BASELINE;
			// codecpar->level = 31;
			_track_map[track_id] = stream->index;
		}
		break;

		case common::MediaType::Audio:
		{
			stream = avformat_new_stream(_format_context, nullptr);
			AVCodecParameters *codecpar = stream->codecpar;

			codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
			codecpar->codec_id = 
				(quality->GetCodecId() == common::MediaCodecId::Aac)?AV_CODEC_ID_AAC:
				(quality->GetCodecId() == common::MediaCodecId::Mp3)?AV_CODEC_ID_MP3:
				(quality->GetCodecId() == common::MediaCodecId::Opus)?AV_CODEC_ID_OPUS:
				AV_CODEC_ID_NONE;		
			codecpar->bit_rate = quality->GetBitrate();
			codecpar->channels = static_cast<int>(quality->GetChannel().GetCounts());
			codecpar->channel_layout = (quality->GetChannel().GetLayout() == common::AudioChannel::Layout::LayoutMono)?AV_CH_LAYOUT_MONO:
									   (quality->GetChannel().GetLayout() == common::AudioChannel::Layout::LayoutStereo)?AV_CH_LAYOUT_STEREO:
									   0; // <- Unknown
			codecpar->sample_rate = quality->GetSample().GetRateNum();
			codecpar->frame_size = 1024;	// TODO: Need to Frame Size
			codecpar->codec_tag = 0;

			stream->time_base = AVRational{quality->GetTimeBase().GetNum(), quality->GetTimeBase().GetDen()};	

			// Reserved					
			// codecpar->profile = FF_PROFILE_AAC_LOW;	
			_track_map[track_id] = stream->index;
		}
		break;	

		default:
		{
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

bool FileWriter::PutData(int32_t track_id, int64_t pts, int64_t dts, MediaPacketFlag flag, std::shared_ptr<ov::Data>& data)
{
	std::unique_lock<std::mutex> mlock(_lock);

	int stream_index = 0;

	auto iter = _track_map.find(track_id);
	if(iter == _track_map.end())
	{
		logtw("There is no track id %d", track_id);
		return true;
	}

	stream_index = iter->second;	

	AVPacket pkt = { 0 };
	av_init_packet(&pkt);

	pkt.stream_index = stream_index;
	pkt.flags = (flag==MediaPacketFlag::Key)?AV_PKT_FLAG_KEY:0;
	pkt.pts = pts;
	pkt.dts = dts;
	pkt.size = data->GetLength();
	pkt.data = (uint8_t *)data->GetDataAs<uint8_t>();

	// TODO: Depending on the extension, the bitstream format should be changed.
	// if(_format_context->streams[stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
	// {
	// 	pkt.size = data->GetLength() - 7;
	// 	pkt.data = (uint8_t *)data->GetDataAs<uint8_t>() + 7;
	// }

	int ret = av_interleaved_write_frame(_format_context, &pkt);	
	if(ret != 0)
	{
		logte("error = %d", ret);
		return false;
	}

	return true;
}

// FFMPEG DEBUG
void FileWriter::FFmpegLog(void *ptr, int level, const char *fmt, va_list vl)
{

	va_list vl2;
	char line[1024];
	static int print_prefix = 1;

	va_copy(vl2, vl);
	av_log_default_callback(ptr, level, fmt, vl);
	av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
	va_end(vl2);

	// if(level >= AV_LOG_DEBUG) return;

	switch (level)
	{
	case AV_LOG_QUIET:
	case AV_LOG_DEBUG:
	case AV_LOG_VERBOSE:
		logd("FFmpeg", line, vl);
		break;
	case AV_LOG_INFO:
		logi("FFmpeg", line, vl);
		break;
	case AV_LOG_WARNING:
		logw("FFmpeg", line, vl);
		break;
	case AV_LOG_ERROR:
	case AV_LOG_FATAL:
	case AV_LOG_PANIC:
		loge("FFmpeg", line, vl);
		break;
	case AV_LOG_TRACE:
//		log_level = ANDROID_LOG_VERBOSE;
		break;
	}
}
