#include "ffmpeg_writer.h"

#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/nalu/nal_stream_converter.h>
#include <modules/bitstream/opus/opus_specific_config.h>
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
		: _state(WriterStateNone)
	{
	}

	Writer::~Writer()
	{
		Stop();
	}

	void Writer::SetState(WriterState state)
	{
		_state = state;
	}

	Writer::WriterState Writer::GetState()
	{
		return _state;
	}

	int Writer::InterruptCallback(void *opaque)
	{
		Writer *writer = (Writer *)opaque;
		if(writer == nullptr)
		{
			logte("Writer is null. stop the writer.");
			return 1;
		}

		auto ellapse = std::chrono::high_resolution_clock::now() - writer->GetLastPacketSentTime();
		if(writer->GetState() == WriterStateClosed)
		{
			logte("Writer is closed. stop the writer.");
			return 1;
		}
		else if(writer->GetState() == WriterStateConnecting)
		{
			if(std::chrono::duration_cast<std::chrono::milliseconds>(ellapse).count() > writer->_connection_timeout)
			{
				logte("connection timeout occurred. stop the writer. %d milliseconds. ", writer->_connection_timeout);
				return 1;
			}
		}
		else if(writer->GetState() == WriterStateConnected)
		{
			auto ellapse_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ellapse).count();
			if(ellapse_ms > writer->_send_timeout)
			{
				logte("Send timeout occurred. stop the writer. %d milliseconds. ", ellapse_ms);
				return 1;
			}
		}

		return 0;
	}

	bool Writer::SetUrl(const ov::String url, const ov::String format)
	{
		if (!url || url.IsEmpty() == true)
		{
			logte("Destination url is empty");
			return false;
		}

		_url = url;
		_format = format;

		// Create output context
		AVFormatContext *av_format = nullptr;
		int error = avformat_alloc_output_context2(&av_format, nullptr, (_format != nullptr) ? _format.CStr() : nullptr, _url.CStr());
		if (error < 0)
		{
			logte("Could not create output context. error(%s), url(%s)", ffmpeg::Conv::AVErrorToString(error).CStr(), _url.CStr());

			return false;
		}

		SetAVFormatContext(av_format);

		return true;
	}

	ov::String Writer::GetUrl()
	{
		return _url;
	}

	void Writer::SetTimestampMode(TimestampMode mode)
	{
		_timestamp_mode = mode;
	}

	Writer::TimestampMode Writer::GetTimestampMode()
	{
		return _timestamp_mode;
	}

	bool Writer::AddTrack(const std::shared_ptr<MediaTrack> &media_track)
	{
		// TODO: This code will move to mediarouter.
		if (media_track->GetCodecId() == cmn::MediaCodecId::Opus &&
			media_track->GetDecoderConfigurationRecord() == nullptr)
		{
			auto opus_config { std::make_shared<OpusSpecificConfig>(media_track->GetChannel().GetCounts(), media_track->GetSampleRate()) };
			media_track->SetDecoderConfigurationRecord(opus_config);
		}

		auto av_format = GetAVFormatContext();
		if (av_format == nullptr)
		{
			logte("AVFormatContext is null");
			return false;
		}

		AVStream *av_stream = avformat_new_stream(av_format.get(), nullptr);
		if (!av_stream)
		{
			logte("Could not allocate stream");

			return false;
		}

		// Convert MediaTrack to AVStream
		if (ffmpeg::Conv::ToAVStream(media_track, av_stream) == false)
		{
			logte("Could not convert track info to AVStream");

			return false;
		}

		{
			std::lock_guard<std::shared_mutex> mlock(_track_map_lock);
			// MediaTrackID -> AVStream, MediaTrack
			// AVStream doesn't need to be released. It will be released when AVFormatContext is released.
			std::shared_ptr<AVStream> av_stream_ptr(av_stream, [](AVStream *av_stream) { });
			_track_map[media_track->GetId()] = std::make_pair(av_stream_ptr, media_track);
		}

		return true;
	}

	bool Writer::Start()
	{
		SetState(WriterStateConnecting);

		_start_time = -1LL;
		_need_to_flush = false;
		_need_to_close = false;

		auto av_format = GetAVFormatContext();
		if (av_format == nullptr)
		{
			logte("AVFormatContext is null");
			return false;
		}

		av_format->flush_packets = 1;

		// Set Interrupt Callback
		_interrupt_cb = {InterruptCallback, this};

		if (!(av_format->oformat->flags & AVFMT_NOFILE))
		{
			_last_packet_sent_time = std::chrono::high_resolution_clock::now();
			int error = avio_open2(&av_format->pb, av_format->url, AVIO_FLAG_WRITE, &_interrupt_cb, nullptr);
			if (error < 0)
			{
				SetState(WriterStateError);
				
				logte("Error opening file. error(%s), url(%s)", ffmpeg::Conv::AVErrorToString(error).CStr(), av_format->url);

				return false;
			}
		}
		_need_to_close = true;

		// Write header
		_last_packet_sent_time = std::chrono::high_resolution_clock::now();

		// Disable edts box
		AVDictionary *format_options = nullptr;
		av_dict_set_int(&format_options, "use_editlist", 0, 0);

		if (avformat_write_header(av_format.get(), &format_options) < 0)
		{
			SetState(WriterStateError);
			
			logte("Could not create header");
			
			return false;
		}
		
		// Set output format name
		_output_format_name = av_format->oformat->name;

		_need_to_flush = true;

		SetState(WriterStateConnected);
		
		return true;
	}

	bool Writer::Stop()
	{
		ReleaseAVFormatContext();
		SetState(WriterStateClosed);
				
		return true;
	}

	bool Writer::SendPacket(const std::shared_ptr<MediaPacket> &packet, uint64_t *sent_bytes)
	{
		if (!packet)
		{
			logte("Packet is null");
			return false;
		}

		// Drop packets that do not need to be transmitted
		auto [av_stream, media_track] = GetTrack(packet->GetTrackId());
		if (av_stream == nullptr || media_track == nullptr)
		{
			return true;
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

		av_packet.stream_index = av_stream->index;
		av_packet.flags = (packet->GetFlag() == MediaPacketFlag::Key) ? AV_PKT_FLAG_KEY : 0;
		av_packet.pts = av_rescale_q(packet->GetPts() - start_time, AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()}, av_stream->time_base);
		av_packet.dts = av_rescale_q(packet->GetDts() - start_time, AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()}, av_stream->time_base);
		av_packet.duration = av_rescale_q(packet->GetDuration(), AVRational{media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen()}, av_stream->time_base);
		av_packet.size = packet->GetData()->GetLength();
		av_packet.data = (uint8_t *)packet->GetData()->GetDataAs<uint8_t>();

		std::shared_ptr<const ov::Data> new_data = nullptr;

		if (_output_format_name == "flv")
		{
			// ANNEXB -> AVCC
			// ADTS -> RAW
			switch (packet->GetBitstreamFormat())
			{
				case cmn::BitstreamFormat::H264_AVCC:
					// Do nothing
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
				case cmn::BitstreamFormat::AMF:
					// Do nothing
					break;
				default:
					// Unsupported bitstream foramt, but it is not an error.
					return true;
			}
		}
		else if (_output_format_name == "mp4")
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
				case cmn::BitstreamFormat::OPUS:
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
					// Unsupported bitstream foramt, but it is not an error.
					return true;
			}
		}
		else if (_output_format_name == "mpegts") // TS, SRT
		{
			switch(packet->GetBitstreamFormat())
			{
				case cmn::BitstreamFormat::H264_ANNEXB:
				case cmn::BitstreamFormat::H264_AVCC:
				case cmn::BitstreamFormat::H265_ANNEXB:
				case cmn::BitstreamFormat::HVCC:
				case cmn::BitstreamFormat::AAC_RAW:
				case cmn::BitstreamFormat::AAC_ADTS:
				case cmn::BitstreamFormat::AAC_LATM:
				case cmn::BitstreamFormat::OPUS:
				case cmn::BitstreamFormat::MP3:
					break;
				default:
					// Unsupported bitstream foramt, but it is not an error.
					return true;
			}
		}

		auto av_format = GetAVFormatContext();
		if (!av_format)
		{
			av_packet_unref(&av_packet);
			SetState(WriterStateError);

			return false;
		}

		_last_packet_sent_time = std::chrono::high_resolution_clock::now();

		if (sent_bytes != nullptr)
		{
			*sent_bytes = av_packet.size;
		}

		std::unique_lock<std::shared_mutex> mlock(_av_format_lock);

		int error = ::av_write_frame(av_format.get(), &av_packet);
		if (error != 0)
		{
			SetState(WriterStateError);

			logte("Send packet error(%s)", ffmpeg::Conv::AVErrorToString(error).CStr());

			return false;
		}

		mlock.unlock();

		return true;
	}

	std::chrono::high_resolution_clock::time_point Writer::GetLastPacketSentTime()
	{
		return _last_packet_sent_time;
	}

	std::shared_ptr<AVFormatContext> Writer::GetAVFormatContext() const
	{
		std::shared_lock<std::shared_mutex> mlock(_av_format_lock);
		return _av_format;
	}

	void Writer::SetAVFormatContext(AVFormatContext *av_format)
	{
		std::lock_guard<std::shared_mutex> mlock(_av_format_lock);
		// forward _need_to_flush and _need_to_close to lambda
		_av_format.reset(av_format, [&need_to_flush = _need_to_flush, &need_to_close = _need_to_close](AVFormatContext *av_format_ptr) {
			if (av_format_ptr == nullptr)
			{
				return;
			}

			if (need_to_flush)
			{
				av_write_trailer(av_format_ptr);
			}

			if (need_to_close)
			{
				avformat_close_input(&av_format_ptr);
			}
			
			avformat_free_context(av_format_ptr);
		});
	}

	void Writer::ReleaseAVFormatContext()
	{
		std::lock_guard<std::shared_mutex> mlock(_av_format_lock);
		_av_format = nullptr;
		_need_to_flush = false;
		_need_to_close = false;
	}

	std::pair<std::shared_ptr<AVStream>, std::shared_ptr<MediaTrack>> Writer::GetTrack(int32_t track_id) const
	{
		std::shared_lock<std::shared_mutex> mlock(_track_map_lock);
		auto it = _track_map.find(track_id);
		if (it == _track_map.end())
		{
			return std::make_pair(nullptr, nullptr);
		}
		return it->second;
	}
}  // namespace ffmpeg
