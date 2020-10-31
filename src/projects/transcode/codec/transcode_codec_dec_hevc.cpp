//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_dec_hevc.h"
#include "base/info/application.h"

#define OV_LOG_TAG "TranscodeCodec"

std::shared_ptr<MediaFrame> OvenCodecImplAvcodecDecHEVC::RecvBuffer(TranscodeResult *result)
{
	// Check the decoded frame is available
	int ret = ::avcodec_receive_frame(_context, _frame);

	if (ret == AVERROR(EAGAIN))
	{
		// Need more data to decode
	}
	else if (ret == AVERROR_EOF)
	{
		logtw("Error receiving a packet for decoding : AVERROR_EOF");
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if (ret < 0)
	{
		logte("Error receiving a packet for decoding : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
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
				auto codec_info = ShowCodecParameters(_context, _codec_par);
				logti("[%s/%s(%u)] input stream information: %s", 
					_stream_info.GetApplicationInfo().GetName().CStr(), _stream_info.GetName().CStr(), _stream_info.GetId(), codec_info.CStr());

				_change_format = true;

				// If the format is changed, notify to another module
				need_to_change_notify = true;
			}
			else
			{
				logte("Could not obtain codec paramters from context %p", _context);
			}
		}

		_decoded_frame_num++;

		auto decoded_frame = std::make_shared<MediaFrame>();

		decoded_frame->SetMediaType(common::MediaType::Video);
		decoded_frame->SetWidth(_frame->width);
		decoded_frame->SetHeight(_frame->height);
		decoded_frame->SetFormat(_frame->format);
		decoded_frame->SetPts((_frame->pts == AV_NOPTS_VALUE) ? -1LL : _frame->pts);

		// logte("%s out %lld", (need_to_change_notify==true)?"notify":"ready", decoded_frame->GetPts());
				
		// Calculate duration using framerate in timebase
		int den = _input_context->GetTimeBase().GetDen();
		int64_t duration = (den == 0) ? 0LL : (float)den / _input_context->GetFrameRate();
		decoded_frame->SetDuration(duration);

		decoded_frame->SetStride(_frame->linesize[0], 0);
		decoded_frame->SetStride(_frame->linesize[1], 1);
		decoded_frame->SetStride(_frame->linesize[2], 2);

		decoded_frame->SetBuffer(_frame->data[0], decoded_frame->GetStride(0) * decoded_frame->GetHeight(), 0);		 // Y-Plane
		decoded_frame->SetBuffer(_frame->data[1], decoded_frame->GetStride(1) * decoded_frame->GetHeight() / 2, 1);  // Cb Plane
		decoded_frame->SetBuffer(_frame->data[2], decoded_frame->GetStride(2) * decoded_frame->GetHeight() / 2, 2);  // Cr Plane

		::av_frame_unref(_frame);

		*result = need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady;

		return std::move(decoded_frame);
	}

	if (_input_buffer.empty() == false)
	{
		// Pop the first packet
		auto buffer = std::move(_input_buffer.front());
		_input_buffer.pop_front();

		auto packet_data = buffer->GetData();

		int64_t remained = packet_data->GetLength();
		off_t offset = 0LL;
		int64_t pts = (buffer->GetPts()==-1LL)?AV_NOPTS_VALUE:buffer->GetPts();
		int64_t dts = (buffer->GetDts()==-1LL)?AV_NOPTS_VALUE:buffer->GetDts();
		auto data = packet_data->GetDataAs<uint8_t>();

		while (remained > 0)
		{
			::av_init_packet(_pkt);

			int parsed_size = ::av_parser_parse2(_parser, _context, &_pkt->data, &_pkt->size,
												 data + offset, static_cast<int>(remained), pts, dts, 0);

			if (parsed_size < 0)
			{
				logte("An error occurred while parsing: %d", parsed_size);
				*result = TranscodeResult::ParseError;
				return nullptr;
			}

			if (_pkt->size > 0)
			{
				_pkt->pts = _parser->pts;
				_pkt->dts = _parser->dts;
				_pkt->flags = (_parser->key_frame == 1) ? AV_PKT_FLAG_KEY : 0;

				ret = ::avcodec_send_packet(_context, _pkt);

				if (ret == AVERROR(EAGAIN))
				{
					// Need more data
				}
				else if (ret == AVERROR_EOF)
				{
					logte("An error occurred while sending a packet for decoding: End of file (%d)", ret);
					*result = TranscodeResult::EndOfFile;
					return nullptr;
				}
				else if (ret == AVERROR(EINVAL))
				{
					logte("An error occurred while sending a packet for decoding: Invalid argument (%d)", ret);
					*result = TranscodeResult::DataError;
					return nullptr;
				}
				else if (ret == AVERROR(ENOMEM))
				{
					logte("An error occurred while sending a packet for decoding: No memory (%d)", ret);
					*result = TranscodeResult::DataError;
					return nullptr;
				}
				else if (ret == AVERROR_INVALIDDATA)
				{
					// If only SPS/PPS Nalunit is entered in the decoder, an invalid data error occurs.
					// There is no particular problem.
					logtd("Invalid data found when processing input (%d)", ret);
					*result = TranscodeResult::DataError;
					return nullptr;
				}				
				else if (ret < 0)
				{
					char err_msg[1024];
					av_strerror(ret, err_msg, sizeof(err_msg));
					logte("An error occurred while sending a packet for decoding: Unhandled error (%d:%s) ", ret, err_msg);
					*result = TranscodeResult::DataError;
					return nullptr;
				}
			}

			OV_ASSERT(
				remained >= parsed_size,
				"Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld",
				remained, parsed_size);

			offset += parsed_size;
			remained -= parsed_size;
		}
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}
