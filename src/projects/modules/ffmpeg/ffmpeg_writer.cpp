#include "ffmpeg_writer.h"

#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/nalu/nal_stream_converter.h>
#include <modules/ffmpeg/ffmpeg_conv.h>

#define OV_LOG_TAG "FFmpegWriter"

namespace ffmpeg
{
	std::shared_ptr<Writer> Writer::Create()
	{
		auto object = std::make_shared<Writer>();

		return object;
	}

	Writer::Writer()
		: _av_format(nullptr)
	{
	}

	Writer::~Writer()
	{
		Stop();
	}

	bool Writer::SetUrl(const ov::String url, const ov::String format)
	{
		std::unique_lock<std::mutex> mlock(_lock);

		if (!url || url.IsEmpty() == true)
		{
			logte("Destination url is empty");
			return false;
		}

		_url = url;
		_format = format;

		int error = avformat_alloc_output_context2(&_av_format, nullptr, (_format != nullptr) ? _format.CStr() : nullptr, _url.CStr());
		if (error < 0)
		{
			logte("Could not create output context. error(%s), url(%s)", ffmpeg::Conv::AVErrorToString(error).CStr(), _url.CStr());

			return false;
		}

		return true;
	}

	ov::String Writer::GetUrl()
	{
		return _url;
	}

	void Writer::SetTimestampMode(TimestampMode mode)
	{
		std::unique_lock<std::mutex> mlock(_lock);

		_timestamp_mode = mode;
	}

	Writer::TimestampMode Writer::GetTimestampMode()
	{
		return _timestamp_mode;
	}

	bool Writer::AddTrack(std::shared_ptr<MediaTrack> media_track)
	{
		std::unique_lock<std::mutex> mlock(_lock);

		AVStream *stream = avformat_new_stream(_av_format, nullptr);
		if (!stream)
		{
			logte("Could not allocate stream");

			return false;
		}

		// Convert MediaTrack to AVStream
		if (ffmpeg::Conv::ToAVStream(media_track, stream) == false)
		{
			logte("Could not convert track info to AVStream");

			return false;
		}

		_id_map[media_track->GetId()] = stream->index;
		_track_map[media_track->GetId()] = media_track;

		return true;
	}

	bool Writer::Start()
	{
		std::unique_lock<std::mutex> mlock(_lock);

		_start_time = -1LL;
		_need_to_flush = false;
		_need_to_close = false;

		AVDictionary *options = nullptr;
		av_dict_set(&options, "fflags", "flush_packets", 0);

		if (!(_av_format->oformat->flags & AVFMT_NOFILE))
		{
			int error = avio_open2(&_av_format->pb, _av_format->url, AVIO_FLAG_WRITE, nullptr, &options);
			if (error < 0)
			{
				logte("Error opening file. error(%s), url(%s)", ffmpeg::Conv::AVErrorToString(error).CStr(), _av_format->url);

				return false;
			}
		}
		_need_to_close = true;

		// Write header
		if (avformat_write_header(_av_format, nullptr) < 0)
		{
			logte("Could not create header");
			return false;
		}
		_need_to_flush = true;

		// Dump format
		// av_dump_format(_av_format, 0, _av_format->url, true);

		return true;
	}

	bool Writer::Stop()
	{
		std::unique_lock<std::mutex> mlock(_lock);

		if (_av_format)
		{
			// Write trailer
			if (_need_to_flush)
			{
				av_write_trailer(_av_format);
			}

			// Close file
			if (_need_to_close)
			{
				avformat_close_input(&_av_format);
			}

			// Free context
			avformat_free_context(_av_format);
			_av_format = nullptr;
		}

		return true;
	}

	bool Writer::SendPacket(std::shared_ptr<MediaPacket> packet)
	{
		std::unique_lock<std::mutex> mlock(_lock);

		if (!_av_format || !packet)
		{
			return false;
		}

		// Find index of AVStream
		auto it = _id_map.find(packet->GetTrackId());
		if (it == _id_map.end())
		{
			// There is no track. Ignore packet
			return true;
		}
		auto av_stream_index = it->second;

		// Find AVStream
		AVStream *av_stream = _av_format->streams[av_stream_index];
		if (!av_stream)
		{
			logtw("Cloud not find stream. av_stream_index(%d)", av_stream_index);

			return false;
		}

		// Find MediaTrack
		auto media_track = _track_map[av_stream_index];
		if (!media_track)
		{
			logtw("Cloud not find track. track_id(%d)", av_stream_index);

			return false;
		}

		// Start Timestamp
		if (_start_time == -1LL)
		{
			if (GetTimestampMode() == TIMESTAMP_STARTZERO_MODE)
			{
				_start_time = av_rescale_q(packet->GetPts(), AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()}, AV_TIME_BASE_Q);
			}
			else if (GetTimestampMode() == TIMESTAMP_PASSTHROUGH_MODE)
			{
				_start_time = 0LL;
			}
		}

		// Convert 1/AV_TIME_BASE -> 1/TimeBase of MediaTrack
		int64_t start_time = av_rescale_q(_start_time, AV_TIME_BASE_Q, AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()});

		// Convert MediaPacket to AVPacket
		AVPacket av_packet = {0};

		av_packet.stream_index = av_stream_index;
		av_packet.flags = (packet->GetFlag() == MediaPacketFlag::Key) ? AV_PKT_FLAG_KEY : 0;
		av_packet.pts = av_rescale_q(packet->GetPts() - start_time, AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()}, av_stream->time_base);
		av_packet.dts = av_rescale_q(packet->GetDts() - start_time, AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()}, av_stream->time_base);
		av_packet.size = packet->GetData()->GetLength();
		av_packet.data = (uint8_t *)packet->GetData()->GetDataAs<uint8_t>();

		std::shared_ptr<const ov::Data> new_data = nullptr;

		if (strcmp(_av_format->oformat->name, "flv") == 0)
		{
			// ANNEXB -> AVCC
			// ADTS -> RAW
			switch (packet->GetBitstreamFormat())
			{
				case cmn::BitstreamFormat::H264_AVCC:
					break;
				case cmn::BitstreamFormat::H264_ANNEXB: {
					new_data = NalStreamConverter::ConvertAnnexbToXvcc(packet->GetData(), packet->GetFragHeader());
					if (new_data == nullptr)
					{
						logtw("Failed to convert annexb to avcc");
						return false;
					}
					av_packet.size = new_data->GetLength();
					av_packet.data = (uint8_t *)new_data->GetDataAs<uint8_t>();
				}
				break;
				case cmn::BitstreamFormat::AAC_RAW:
					break;
				case cmn::BitstreamFormat::AAC_ADTS: {
					new_data = AacConverter::ConvertAdtsToRaw(packet->GetData(), nullptr);
					if (new_data == nullptr)
					{
						logtw("Failed to convert adts to raw");
						return false;
					}
					av_packet.size = new_data->GetLength();
					av_packet.data = (uint8_t *)new_data->GetDataAs<uint8_t>();
				}
				break;
				default:
					// Unsupported bitstream foramt
					return false;
			}
		}
		else if (strcmp(_av_format->oformat->name, "mp4") == 0)
		{
			switch (packet->GetBitstreamFormat())
			{
				case cmn::BitstreamFormat::HVCC:
					break;
				case cmn::BitstreamFormat::H265_ANNEXB: {
					new_data = NalStreamConverter::ConvertAnnexbToXvcc(packet->GetData(), packet->GetFragHeader());
					if (new_data == nullptr)
					{
						logtw("Failed to convert annexb to avcc");
						return false;
					}
					av_packet.size = new_data->GetLength();
					av_packet.data = (uint8_t *)new_data->GetDataAs<uint8_t>();
				}
				break;
				case cmn::BitstreamFormat::H264_AVCC:
					break;
				case cmn::BitstreamFormat::H264_ANNEXB: {
					new_data = NalStreamConverter::ConvertAnnexbToXvcc(packet->GetData(), packet->GetFragHeader());
					if (new_data == nullptr)
					{
						logtw("Failed to convert annexb to avcc");
						return false;
					}
					av_packet.size = new_data->GetLength();
					av_packet.data = (uint8_t *)new_data->GetDataAs<uint8_t>();
				}
				break;
				case cmn::BitstreamFormat::AAC_RAW:
					break;
				case cmn::BitstreamFormat::AAC_ADTS: {
					new_data = AacConverter::ConvertAdtsToRaw(packet->GetData(), nullptr);
					if (new_data == nullptr)
					{
						logtw("Failed to convert adts to raw");
						return false;
					}
					av_packet.size = new_data->GetLength();
					av_packet.data = (uint8_t *)new_data->GetDataAs<uint8_t>();
				}
				break;

				default:
					// Unsupported bitstream foramt
					return false;
			}
		}
		else if (strcmp(_av_format->oformat->name, "mpegts") == 0)
		{
			// Passthrough
		}

		int error = av_interleaved_write_frame(_av_format, &av_packet);
		if (error != 0)
		{
			logte("Send packet error(%s)", ffmpeg::Conv::AVErrorToString(error).CStr());
			return false;
		}

		return true;
	}
}  // namespace ffmpeg