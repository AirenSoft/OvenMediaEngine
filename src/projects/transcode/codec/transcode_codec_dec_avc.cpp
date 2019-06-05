//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_dec_avc.h"

#define OV_LOG_TAG "TranscodeCodec"

std::unique_ptr<MediaFrame> OvenCodecImplAvcodecDecAVC::RecvBuffer(TranscodeResult *result)
{
	// Check the decoded frame is available
	int ret = avcodec_receive_frame(_context, _frame);

	if(ret == AVERROR(EAGAIN))
	{
		// Need more data to decode
	}
	else if(ret == AVERROR_EOF)
	{
		logtw("Error receiving a packet for decoding : AVERROR_EOF");
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else if(ret < 0)
	{
		logte("Error receiving a packet for decoding : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		bool need_to_change_notify = false;

		// Update codec information if needed
		if(_change_format == false)
		{
			ret = avcodec_parameters_from_context(_codec_par, _context);

			if(ret == 0)
			{
				ShowCodecParameters(_codec_par);

				_change_format = true;

				// If the format is changed, notify to another module
				need_to_change_notify = true;
			}
		}

		_decoded_frame_num++;

		auto decoded_frame = std::make_unique<MediaFrame>();

		decoded_frame->SetWidth(_frame->width);
		decoded_frame->SetHeight(_frame->height);
		decoded_frame->SetFormat(_frame->format);
        decoded_frame->SetPts(_parser->pts - (33 * 2));
		//decoded_frame->SetPts((_frame->pts == AV_NOPTS_VALUE) ? -1 : _frame->pts);

		decoded_frame->SetStride(_frame->linesize[0], 0);
		decoded_frame->SetStride(_frame->linesize[1], 1);
		decoded_frame->SetStride(_frame->linesize[2], 2);

		decoded_frame->SetBuffer(_frame->data[0], decoded_frame->GetStride(0) * decoded_frame->GetHeight(), 0);        // Y-Plane
		decoded_frame->SetBuffer(_frame->data[1], decoded_frame->GetStride(1) * decoded_frame->GetHeight() / 2, 1);    // Cb Plane
		decoded_frame->SetBuffer(_frame->data[2], decoded_frame->GetStride(2) * decoded_frame->GetHeight() / 2, 2);        // Cr Plane

#if DEBUG_PREVIEW_ENABLE && DEBUG_PREVIEW    // DEBUG for OpenCV
		Mat *display_frame = nullptr;

		AVUtilities::FrameConvert::AVFrameToCvMat(_frame, &display_frame, _frame->width);

		cv::imshow("Decoded", *display_frame );
		waitKey(1);
		delete display_frame;
#endif

#if 0    // DEBUG
		if(_decoded_frame_num % 10 == 0)
		logti("decoded. num(%d), width(%d), height(%d), linesize(%d/%d/%d), format(%s), pts/dts(%.0f/%.0f)"
				, _decoded_frame_num
				, _frame->width
				, _frame->height
				, _frame->linesize[0]	// stride
				, _frame->linesize[1]	// stride
				, _frame->linesize[2]	// stride
				, av_get_pix_fmt_name((AVPixelFormat)_frame->format)
				, (float)(_frame->pts==AV_NOPTS_VALUE)? -1LL : _frame->pts
				, (float)(_frame->pkt_dts==AV_NOPTS_VALUE)? -1LL : _frame->pkt_dts
				);
#endif

		av_frame_unref(_frame);

		*result = need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady;

		return std::move(decoded_frame);
	}

	if(_input_buffer.empty() == false)
	{
		// Pop the first packet
		auto buffer = std::move(_input_buffer.front());
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		auto packet_data = buffer->GetData();

		int64_t remained = packet_data->GetLength();
		off_t offset = 0LL;
		int64_t pts = buffer->GetPts();
        int64_t cts  = buffer->GetCts();
		auto data = packet_data->GetDataAs<uint8_t>();

		static int64_t first_pts = -1;

		if(first_pts == -1)
		{
			first_pts = pts;
		}

        // logtd("decode frame - pts(%lld) cts(%lld)", buffer->GetPts(), buffer->GetCts());

		while(remained > 0)
		{
			av_init_packet(_pkt);

			int parsed_size = av_parser_parse2(_parser, _context, &_pkt->data, &_pkt->size,
			                                   data + offset, static_cast<int>(remained), pts, pts, 0);

			if(parsed_size < 0)
			{
				logte("An error occurred while parsing: %d", parsed_size);
				*result = TranscodeResult::ParseError;
				return nullptr;
			}

			if(_pkt->size > 0)
			{
				_pkt->pts = _parser->pts;
				_pkt->flags = (_parser->key_frame == 1) ? AV_PKT_FLAG_KEY : 0;

				ret = avcodec_send_packet(_context, _pkt);

				if(ret == AVERROR(EAGAIN))
				{
					// Need more data
				}
				else if(ret == AVERROR_EOF)
				{
					logte("An error occurred while sending a packet for decoding: End of file (%d)", ret);
				}
				else if(ret == AVERROR(EINVAL))
				{
					logte("An error occurred while sending a packet for decoding: Invalid argument (%d)", ret);
					*result = TranscodeResult::DataError;
					return nullptr;
				}
				else if(ret == AVERROR(ENOMEM))
				{
					logte("An error occurred while sending a packet for decoding: No memory (%d)", ret);
				}
				else if(ret < 0)
				{
					logte("An error occurred while sending a packet for decoding: Unhandled error (%d)", ret);
					*result = TranscodeResult::DataError;
					return nullptr;
				}
			}

			OV_ASSERT(
				remained >= parsed_size,
				"Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld",
				remained, parsed_size
			);

			offset += parsed_size;
			remained -= parsed_size;
		}
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}
