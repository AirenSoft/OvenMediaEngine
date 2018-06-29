//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcode_codec_dec_avc.h"
#include <base/ovlibrary/ovlibrary.h>

#include "../utilities.h"

#define OV_LOG_TAG "TranscodeCodec"

#define DEBUG_PREVIEW 0


OvenCodecImplAvcodecDecAVC::OvenCodecImplAvcodecDecAVC()
{

	avcodec_register_all();

	_pkt_buf.clear();

	_pkt = av_packet_alloc();
	
	_frame = av_frame_alloc();
	
	_codec_par = avcodec_parameters_alloc();
	
	_change_format = false;
	
	_decoded_frame_num = 0;
	
#if DEBUG_PREVIEW_ENABLE && DEBUG_PREVIEW
	cv::namedWindow( "Decoded", WINDOW_AUTOSIZE );
#endif


}

OvenCodecImplAvcodecDecAVC::~OvenCodecImplAvcodecDecAVC()
{
    avcodec_free_context(&_context);

    avcodec_parameters_free(&_codec_par);
    
    av_frame_free(&_frame);
    
    av_packet_free(&_pkt);
}

int32_t OvenCodecImplAvcodecDecAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	AVCodecParameters *origin_par = NULL;

    _codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!_codec) 
    {
        logte("Codec not found");
        return 1;
    }

    // create codec context
	_context = avcodec_alloc_context3(_codec);
	if (!_context) 
	{
		logte("Could not allocate video codec context");
		return 1;
	} 

	// _context->thread_count = 1;

	if (avcodec_open2(_context, _codec, NULL) < 0) 
	{
		logte("Could not open codec");
		return 1;
	}

	_parser = av_parser_init(_codec->id);
    if (!_parser) 
    {
        logte("Parser not found");
		return 1;
    }	

    return 0;
}

void OvenCodecImplAvcodecDecAVC::sendbuf(std::unique_ptr<MediaBuffer> buf)
{
	// logtd("Stage-2 : %f", (float)_last_recv_packet_pts);

	_pkt_buf.push_back(std::move(buf));
}

std::pair<int32_t, std::unique_ptr<MediaBuffer>> OvenCodecImplAvcodecDecAVC::recvbuf()
{
	int ret;

	///////////////////////////////////////////////////
	// 디코딩 가능한 프레임이 존재하는지 확인한다.
	///////////////////////////////////////////////////
	ret = avcodec_receive_frame(_context, _frame);
	if(ret == AVERROR(EAGAIN) )
	{
		// 패킷을 넣음
	}
	else if(ret == AVERROR_EOF)
	{
		logtw("\r\nError receiving a packet for decoding : AVERROR_EOF");
		return std::make_pair(-1, nullptr);
	}
	else if(ret < 0) 
	{
		logte("Error receiving a packet for decoding : %d", ret);
		return std::make_pair(-1, nullptr);
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
				logti("codec parameters : codec_type(%d), codec_id(%d), codec_tag(%d), extra(%d), format(%d), bit_rate(%d),  bits_per_coded_sample(%d), bits_per_raw_sample(%d), profile(%d), level(%d), sample_aspect_ratio(%d/%d) width(%d), height(%d) field_order(%d) color_range(%d) color_primaries(%d) color_trc(%d) color_space(%d) chroma_location(%d), channel_layout(%.0f) channels(%d) sample_rate(%d) block_align(%d) frame_size(%d)", 
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

					(float)_codec_par->channel_layout,
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

		auto out_buf = std::make_unique<MediaBuffer>();

		out_buf->SetWidth(_frame->width);
		out_buf->SetHeight(_frame->height);
		// TODO: Trans
		out_buf->SetFormat(_frame->format);
		out_buf->SetPts((_frame->pts==AV_NOPTS_VALUE)?-1.0f:_frame->pts);
		out_buf->SetStride(_frame->linesize[0], 0);
		out_buf->SetStride(_frame->linesize[1], 1);
		out_buf->SetStride(_frame->linesize[2], 2);

		out_buf->SetBuffer(_frame->data[0], out_buf->GetStride(0) * out_buf->_height, 0);		// Y-Plane
		out_buf->SetBuffer(_frame->data[1], out_buf->GetStride(1) * out_buf->_height/2, 1); 	// Cb Plane
		out_buf->SetBuffer(_frame->data[2], out_buf->GetStride(2) * out_buf->_height/2, 2);		// Cr Plane

		_last_recv_packet_pts = (int64_t)_frame->pts;


		// logtd("Stage-5 : %f", (float)_frame->pts);




#if DEBUG_PREVIEW_ENABLE && DEBUG_PREVIEW	// DEBUG for OpenCV
			Mat *display_frame = nullptr;

			AVUtilities::FrameConvert::AVFrameToCvMat(_frame, &display_frame, _frame->width);

			cv::imshow("Decoded", *display_frame );  
			waitKey(1);   
			delete display_frame;
#endif

#if 0	// DEBUG
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
		return std::make_pair(need_to_change_notify?1:0, std::move(out_buf));
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while (_pkt_buf.size() > 0) {

		MediaBuffer* cur_pkt = _pkt_buf[0].get();

		// logtd("Stage-3 : %f", (float)_pkt->pts);



        int parsed_size = av_parser_parse2(
        	_parser, 
        	_context, 
        	&_pkt->data, &_pkt->size,
			cur_pkt->GetBuffer() + cur_pkt->_offset, 
			cur_pkt->GetBufferSize() - cur_pkt->_offset, 
			cur_pkt->GetPts(), cur_pkt->GetPts(), 
			0);

        if (parsed_size < 0) 
        {
            logte("Error while parsing");
            return std::make_pair(2, nullptr);
        }

		if(_pkt->size > 0)
        {
        	_pkt->pts = _parser->pts;
        	_pkt->dts = _parser->dts;
			_pkt->flags = (_parser->key_frame==1)?AV_PKT_FLAG_KEY:0;

			_last_send_packet_pts = (int64_t)_pkt->pts;
			// logtd("send frame tinestamp : %f", (float)_last_send_packet_pts);

			// logtd("Stage-4 : %f", (float)_pkt->pts);


			int ret = avcodec_send_packet(_context, _pkt);
			if(ret == AVERROR(EAGAIN)) 
			{
				// 더이상 디코딩할 데이터가 없다면 빠짐
				// printf("Error sending a packet for decoding : AVERROR(EAGAIN)\n");
				return std::make_pair(1, nullptr);
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
				return std::make_pair(-1, nullptr);
			}
		}


		// send_packet 이 완료된 이후에 데이터를 삭제해야함.
        if(parsed_size > 0)
        {
        	cur_pkt->_offset += parsed_size;
        	if(cur_pkt->_offset >= (int32_t)cur_pkt->GetBufferSize())
        	{
        		_pkt_buf.erase(_pkt_buf.begin(), _pkt_buf.begin()+1);
        	}
     	}
	}

	return std::make_pair(-1, nullptr);
}
