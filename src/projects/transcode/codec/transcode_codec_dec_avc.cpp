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
    ///////////////////////////////////////////////////
    // 디코딩 가능한 프레임이 존재하는지 확인한다.
    ///////////////////////////////////////////////////
    int ret = avcodec_receive_frame(_context, _frame);

    if(ret == AVERROR(EAGAIN))
    {
        // 패킷을 넣음
    }
    else if(ret == AVERROR_EOF)
    {
        logtw("\r\nError receiving a packet for decoding : AVERROR_EOF");
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

        // 디코딩된 코덱의 정보를 업데이트 함.
        if(_change_format == false)
        {
            ret = avcodec_parameters_from_context(_codec_par, _context);

            if(ret == 0)
            {
                logti("Codec parameters : codec_type(%d), codec_id(%d), codec_tag(%d), extra(%d), format(%d), bit_rate(%d),  bits_per_coded_sample(%d), bits_per_raw_sample(%d), profile(%d), level(%d), sample_aspect_ratio(%d/%d) width(%d), height(%d) field_order(%d) color_range(%d) color_primaries(%d) color_trc(%d) color_space(%d) chroma_location(%d), channel_layout(%ld) channels(%d) sample_rate(%d) block_align(%d) frame_size(%d)",
                      _codec_par->codec_type,
                      _codec_par->codec_id,
                      _codec_par->codec_tag,
                      _codec_par->extradata_size,
                      _codec_par->format,
                      _codec_par->bit_rate,
                      _codec_par->bits_per_coded_sample,
                      _codec_par->bits_per_raw_sample,
                      _codec_par->profile,
                      _codec_par->level,
                      _codec_par->sample_aspect_ratio.num, _codec_par->sample_aspect_ratio.den,

                      _codec_par->width,
                      _codec_par->height,

                      _codec_par->field_order,
                      _codec_par->color_range,
                      _codec_par->color_primaries,
                      _codec_par->color_trc,
                      _codec_par->color_space,
                      _codec_par->chroma_location,

                      _codec_par->channel_layout,
                      _codec_par->channels,
                      _codec_par->sample_rate,
                      _codec_par->block_align,
                      _codec_par->frame_size
                );

                _change_format = true;

                // 포맷이 변경이 되면, 변경된 정보를 알려줘야함.
                need_to_change_notify = true;
            }
        }

        _decoded_frame_num++;

        auto out_buf = std::make_unique<MediaFrame>();

        out_buf->SetWidth(_frame->width);
        out_buf->SetHeight(_frame->height);
        // TODO: Trans
        out_buf->SetFormat(_frame->format);
        out_buf->SetPts((_frame->pts == AV_NOPTS_VALUE) ? -1.0f : _frame->pts);
        out_buf->SetStride(_frame->linesize[0], 0);
        out_buf->SetStride(_frame->linesize[1], 1);
        out_buf->SetStride(_frame->linesize[2], 2);

        out_buf->SetBuffer(_frame->data[0], out_buf->GetStride(0) * out_buf->GetHeight(), 0);        // Y-Plane
        out_buf->SetBuffer(_frame->data[1], out_buf->GetStride(1) * out_buf->GetHeight() / 2, 1);    // Cb Plane
        out_buf->SetBuffer(_frame->data[2], out_buf->GetStride(2) * out_buf->GetHeight() / 2, 2);        // Cr Plane

        _last_recv_packet_pts = (int64_t)_frame->pts;

        // logtd("Stage-5 : %f", (float)_frame->pts);

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
				, (float)(_frame->pts==AV_NOPTS_VALUE)?-1.0f:_frame->pts
				, (float)(_frame->pkt_dts==AV_NOPTS_VALUE)?-1.0f:_frame->pkt_dts
				);
#endif

        av_frame_unref(_frame);

        // Notify가 필요한 경우에 1을 반환, 아닌 경우에는 일반적인 경우로 0을 반환
        *result = need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady;
        return std::move(out_buf);
    }

    ///////////////////////////////////////////////////
    // 인코딩 요청
    ///////////////////////////////////////////////////
    off_t offset = 0;

    while(_input_buffer.size() > 0)
    {
        const MediaPacket *cur_pkt = _input_buffer[0].get();
        std::shared_ptr<const ov::Data> cur_data = nullptr;

        if(cur_pkt != nullptr)
        {
            cur_data = cur_pkt->GetData();
        }

#if 0
        if((cur_data == nullptr) || (cur_data->GetLength() == 0))
		{
			_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);
			continue;
		}
#endif

#if 0
        int parsed_size = av_parser_parse2(
        	_parser,
        	_context,
        	&_pkt->data, &_pkt->size,
			cur_pkt->GetBuffer() + cur_pkt->_offset,
			cur_pkt->GetBufferSize() - cur_pkt->_offset,
			cur_pkt->GetPts(), cur_pkt->GetPts(),
			0);
#endif
        int parsed_size = av_parser_parse2(
                _parser,
                _context,
                &_pkt->data, &_pkt->size,
                cur_data->GetDataAs<uint8_t>() + offset,
                static_cast<int>(cur_data->GetLength() - offset),
                cur_pkt->GetPts(), cur_pkt->GetPts(),
                0);

        if(parsed_size < 0)
        {
            logte("Error while parsing");
            *result = TranscodeResult::ParseError;
            return nullptr;
        }

        if(_pkt->size > 0)
        {
            _pkt->pts = _parser->pts;
            _pkt->dts = _parser->dts;
            _pkt->flags = (_parser->key_frame == 1) ? AV_PKT_FLAG_KEY : 0;

            _last_send_packet_pts = (int64_t)_pkt->pts;
            // logtd("send frame tinestamp : %f", (float)_last_send_packet_pts);

            // logtd("Stage-4 : %f", (float)_pkt->pts);


            int ret = avcodec_send_packet(_context, _pkt);
            if(ret == AVERROR(EAGAIN))
            {
                // 더이상 디코딩할 데이터가 없다면 빠짐
                // printf("Error sending a packet for decoding : AVERROR(EAGAIN)\n");
            }
            else if(ret == AVERROR_EOF)
            {
                logte("Error sending a packet for decoding : AVERROR_EOF");
            }
            else if(ret == AVERROR(EINVAL))
            {
                logte("Error sending a packet for decoding : AVERROR(EINVAL)");
            }
            else if(ret == AVERROR(ENOMEM))
            {
                logte("Error sending a packet for decoding : AVERROR(ENOMEM)");
            }
            else if(ret < 0)
            {
                logte("Error sending a packet for decoding : ERROR(Unknown %d)", ret);
                *result = TranscodeResult::DataError;
                return nullptr;
            }
        }

        // send_packet 이 완료된 이후에 데이터를 삭제해야함.
        if(parsed_size > 0)
        {
            OV_ASSERT(cur_data->GetLength() >= parsed_size, "Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld", cur_data->GetLength(), parsed_size);

            offset += parsed_size;

            if(offset >= cur_data->GetLength())
            {
                // pop the first item
                _input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);
                offset = 0;
            }
        }
    }

    *result = TranscodeResult::NoData;
    return nullptr;
}

