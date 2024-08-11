//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "decoder_hevc_nilogan.h"

#include "../../transcoder_gpu.h"
#include "../../transcoder_private.h"
#include "base/info/application.h"

#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/nalu/nal_stream_converter.h>

bool DecoderHEVCxNILOGAN::Configure(std::shared_ptr<MediaTrack> context)
{
	if (TranscodeDecoder::Configure(context) == false)
	{
		return false;
	}

	
	// Create packet parser
	_parser = ::av_parser_init(GetCodecID());
	if (_parser == nullptr)
	{
		logte("Parser not found");
		return false;
	}
	_parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
	
	if (InitCodec() == false)
	{
		return false;
	}
	
	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&TranscodeDecoder::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("Dec%sNilogan", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start decoder thread");
		_kill_flag = true;
		return false;
	}

	return true;
}

bool DecoderHEVCxNILOGAN::InitCodec()
{
	const AVCodec *_codec = ::avcodec_find_decoder_by_name("h265_ni_logan_dec");
	if (_codec == nullptr)
	{
		logte("Codec not found: %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}

	_context = ::avcodec_alloc_context3(_codec);
	if (_context == nullptr)
	{
		logte("Could not allocate codec context for %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}

	_context->hw_device_ctx = ::av_buffer_ref(TranscodeGPU::GetInstance()->GetDeviceContext(cmn::MediaCodecModuleId::NILOGAN, _track->GetCodecDeviceId()));
	if(_context->hw_device_ctx == nullptr)
	{
		logte("Could not allocate hw device context for %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}
	
	

	_context->time_base = ffmpeg::Conv::TimebaseToAVRational(GetTimebase());
	_context->pkt_timebase = ffmpeg::Conv::TimebaseToAVRational(GetTimebase());
	_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
	
	auto decoder_config = std::static_pointer_cast<HEVCDecoderConfigurationRecord>(GetRefTrack()->GetDecoderConfigurationRecord());
	
	if(decoder_config->ChromaFormat() > 1) {			
		logte("Could not initialize codec because nilogan decoder support only AV_PIX_FMT_YUV420P pixel format: %d", decoder_config->ChromaFormat());
		return false;
	}

	if (decoder_config != nullptr)
	{		
		_context->pix_fmt = AV_PIX_FMT_YUV420P; //Forced here nilogan decoder support only AV_PIX_FMT_YUV420P pixel format
		_context->width = decoder_config->GetWidth();
		_context->height = decoder_config->GetHeight();
	}

	//dec_options
	::av_opt_set(_context->priv_data, "xcoder-params", "out=sw:lowDelayMode=1:lowDelay=100", 0);
	//::av_opt_set(_context->priv_data, "dec", 0, 0);
	
	if (::avcodec_open2(_context, _codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}
	
	return true;
}

void DecoderHEVCxNILOGAN::CodecThread()
{
	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
		{
			// logte("An error occurred while dequeue : no data");
			continue;
		}

		auto buffer = std::move(obj.value());

		auto packet_data = buffer->GetData();

		int64_t remained = packet_data->GetLength();
		off_t offset = 0LL;
		int64_t pts = (buffer->GetPts() == -1LL) ? AV_NOPTS_VALUE : buffer->GetPts();
		int64_t dts = (buffer->GetDts() == -1LL) ? AV_NOPTS_VALUE : buffer->GetDts();
		[[maybe_unused]] int64_t duration = (buffer->GetDuration() == -1LL) ? AV_NOPTS_VALUE : buffer->GetDuration();
		auto data = packet_data->GetDataAs<uint8_t>();

		while (remained > 0)
		{
			::av_packet_unref(_pkt);

			int parsed_size = ::av_parser_parse2(_parser, _context, &_pkt->data, &_pkt->size,
												 data + offset, static_cast<int>(remained), pts, dts, 0);

			if (parsed_size < 0)
			{
				logte("An error occurred while parsing: %d", parsed_size);
				break;
			}
			
			// if activated, I got Warning: time out on receiving a decoded framefrom the decoder, assume dropped, received frame_num: 0, sent pkt_num: 1, pkt_num-frame_num: 1, sending another packet.
			// ReinitCodecIfNeed();			
 
			if (_pkt->size > 0)
			{
				_pkt->pts = _parser->pts;
				_pkt->dts = _parser->dts;
				_pkt->flags = (_parser->key_frame == 1) ? AV_PKT_FLAG_KEY : 0;
				_pkt->duration = _pkt->dts - _parser->last_dts;
				if (_pkt->duration <= 0LL)
				{
					// It may not be the exact packet duration.
					// However, in general, this method is applied under the assumption that the duration of all packets is similar.
					_pkt->duration = duration;
				}

				int ret = ::avcodec_send_packet(_context, _pkt);

				if (ret == AVERROR(EAGAIN))
				{
					// Need more data
				}
				else if (ret == AVERROR_EOF)
				{
					logte("An error occurred while sending a packet for decoding: End of file (%d)", ret);
					break;
				}
				else if (ret == AVERROR(EINVAL))
				{
					logte("An error occurred while sending a packet for decoding: Invalid argument (%d)", ret);
					break;
				}
				else if (ret == AVERROR(ENOMEM))
				{
					logte("An error occurred while sending a packet for decoding: No memory (%d)", ret);
					break;
				}
				else if (ret == AVERROR_INVALIDDATA)
				{
					// If only SPS/PPS Nalunit is entered in the decoder, an invalid data error occurs.
					// There is no particular problem.
					logtd("Invalid data found when processing input (%d)", ret);
					break;
				}
				else if (ret < 0)
				{
					char err_msg[1024];
					av_strerror(ret, err_msg, sizeof(err_msg));
					logte("An error occurred while sending a packet for decoding: Unhandled error (%d:%s) ", ret, err_msg);
					break;
				}
			}

			OV_ASSERT(
				remained >= parsed_size,
				"Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld",
				remained, parsed_size);

			offset += parsed_size;
			remained -= parsed_size;
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////
		while (!_kill_flag)
		{
			// Check the decoded frame is available
			int ret = ::avcodec_receive_frame(_context, _frame);

			if (ret == AVERROR(EAGAIN))
			{
				break;
			}
			else if (ret == AVERROR_EOF)
			{
				logtw("Error receiving a packet for decoding : AVERROR_EOF");
				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				break;
			}
			else
			{
				bool need_to_change_notify = false;

				// Update codec information if needed
				if (_change_format == false)
				{
					ret = ::avcodec_parameters_from_context(_codec_par, _context);
					if (ret == 0)
					{
						auto codec_info = ffmpeg::Conv::CodecInfoToString(_context, _codec_par);
						logti("[%s/%s(%u)] input stream information: %s",
							  _stream_info.GetApplicationInfo().GetVHostAppName().CStr(),
							  _stream_info.GetName().CStr(),
							  _stream_info.GetId(),
							  codec_info.CStr());

						_change_format = true;

						// If the format is changed, notify to another module
						need_to_change_notify = true;
					}
					else
					{
						logte("Could not obtain codec parameters from context %p", _context);
					}
				}

				// If there is no duration, the duration is calculated by framerate and timebase.
				if(_frame->pkt_duration <= 0LL && _context->framerate.num > 0 && _context->framerate.den > 0)
				{
					_frame->pkt_duration = (int64_t)( ((double)_context->framerate.den / (double)_context->framerate.num) / ((double) GetRefTrack()->GetTimeBase().GetNum() / (double) GetRefTrack()->GetTimeBase().GetDen()) );
				}

				auto decoded_frame = ffmpeg::Conv::ToMediaFrame(cmn::MediaType::Video, _frame);
				if (decoded_frame == nullptr)
				{
					continue;
				}

				// logtd("%d / %d / fmt(%d)", decoded_frame->GetWidth(), decoded_frame->GetHeight(), decoded_frame->GetFormat());

				::av_frame_unref(_frame);

				Complete(need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady, std::move(decoded_frame));
			}
		}
	}
}
