//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "decoder_avc.h"

#include "../../transcoder_private.h"
#include "base/info/application.h"

bool DecoderAVC::Configure(std::shared_ptr<MediaTrack> context)
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

	// Initialize codec
	if (InitCodec() == false)
	{
		return false;
	}

	try
	{
		_kill_flag = false;

		_codec_thread = std::thread(&TranscodeDecoder::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("Dec%s", avcodec_get_name(GetCodecID())).CStr());
	}
	catch (const std::system_error &e)
	{
		logte("Failed to start decoder thread");
		_kill_flag = true;
		return false;
	}

	return true;
}


bool DecoderAVC::InitCodec()
{
	const AVCodec *_codec = ::avcodec_find_decoder(GetCodecID());
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

	_context->time_base = ffmpeg::Conv::TimebaseToAVRational(GetTimebase());
	_context->thread_count = 2;
	_context->thread_type = FF_THREAD_FRAME;

	// Set the number of b frames for compatibility with specific encoders.
	auto bframes = GetRefTrack()->HasBframes()?1:0;
	if (bframes > 0)
	{
		_context->has_b_frames = bframes;
	}

	if (::avcodec_open2(_context, _codec, nullptr) < 0)
	{
		logte("Could not open codec: %s (%d)", ::avcodec_get_name(GetCodecID()), GetCodecID());
		return false;
	}


	_change_format = false;

	return true;
}

void DecoderAVC::UninitCodec()
{
	::avcodec_close(_context);
	::avcodec_free_context(&_context);

	_context = nullptr;
}

bool DecoderAVC::ReinitCodecIfNeed()
{
	if (_context->width != 0 && _context->height != 0 && (_parser->width != _context->width || _parser->height != _context->height))
	{
		logti("Changed input resolution of %u track. (%dx%d -> %dx%d)", GetRefTrack()->GetId(), _context->width, _context->height, _parser->width, _parser->height);

		UninitCodec();

		if (InitCodec() == false)
		{
			return false;
		}
	}

	return true;
}

void DecoderAVC::CodecThread()
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

		int64_t remained_size = packet_data->GetLength();
		off_t offset = 0LL;
		int64_t pts = (buffer->GetPts() == -1LL) ? AV_NOPTS_VALUE : buffer->GetPts();
		int64_t dts = (buffer->GetDts() == -1LL) ? AV_NOPTS_VALUE : buffer->GetDts();
		[[maybe_unused]] int64_t duration = (buffer->GetDuration() == -1LL) ? AV_NOPTS_VALUE : buffer->GetDuration();
		auto data = packet_data->GetDataAs<uint8_t>();

		///////////////////////////////
		// Send to decoder
		///////////////////////////////
		while (remained_size > 0)
		{
			::av_packet_unref(_pkt);

			int parsed_size = ::av_parser_parse2(_parser, _context, &_pkt->data, &_pkt->size,
												 data + offset, static_cast<int>(remained_size), pts, dts, 0);
			if (parsed_size < 0)
			{
				logte("An error occurred while parsing: %d", parsed_size);
				break;
			}

			if(ReinitCodecIfNeed() == false)
			{
				logte("An error occurred while reinit codec");
				break;
			}

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
					auto empty_frame = std::make_shared<MediaFrame>();
					empty_frame->SetPts(dts);
					empty_frame->SetMediaType(cmn::MediaType::Video);

					Complete(TranscodeResult::NoData, std::move(empty_frame));

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

			OV_ASSERT(remained_size >= parsed_size, "Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld",
					  remained_size, parsed_size);

			offset += parsed_size;
			remained_size -= parsed_size;
		}

		///////////////////////////////
		// Receive from decoder
		///////////////////////////////
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
						logti("[%s/%s(%u)] input track information: %s",
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
				::av_frame_unref(_frame);
				if (decoded_frame == nullptr)
				{
					continue;
				}

				Complete(need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady, std::move(decoded_frame));
			}
		}
	}
}
