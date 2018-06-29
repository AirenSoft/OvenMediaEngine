//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcode_codec_dec_aac.h"

#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "TranscodeCodec"

OvenCodecImplAvcodecDecAAC::OvenCodecImplAvcodecDecAAC()
{
	avcodec_register_all();
	_pkt_buf.clear();
	_pkt = av_packet_alloc();
	_frame = av_frame_alloc();
	_codec_par = avcodec_parameters_alloc();
	_change_format = false;
	_decoded_frame_num = 0;

}

OvenCodecImplAvcodecDecAAC::~OvenCodecImplAvcodecDecAAC()
{
    avcodec_free_context(&_context);
    av_frame_free(&_frame);
    av_packet_free(&_pkt);
}

int32_t OvenCodecImplAvcodecDecAAC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	AVCodecParameters *origin_par = NULL;

	/* find the MPEG-1 video decoder */
    AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!codec) {
        logte("Codec not found\n");
        return 1;
    }

    // create codec context
	_context = avcodec_alloc_context3(codec);
	if (!_context) {
		logte("Could not allocate video codec context\n");
		return 1;
	} 

	// open codec
	if (avcodec_open2(_context, codec, NULL) < 0) {
		logte("Could not open codec\n");
		return 1;
	}

	_parser = av_parser_init(codec->id);
    if (!_parser) {
        logte("Parser not found\n");
		return 1;
    }	

    return 0;
}

void OvenCodecImplAvcodecDecAAC::sendbuf(std::unique_ptr<MediaBuffer> buf)
{
	_pkt_buf.push_back(std::move(buf));
}

// ==========================
// 리턴값 정의
// ==========================
//  0 : 프레임 디코딩 완료, 아직 디코딩할 데이터가 대기하고 있음.
//  1 : 포맷이 변경이 되었다.
//  < 0 : 뭐든간에 에러가 있다.

std::pair<int32_t, std::unique_ptr<MediaBuffer>> OvenCodecImplAvcodecDecAAC::recvbuf()
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
		logte("Error receiving a packet for decoding : AVERROR_EOF");
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

#if 0
		if(_decoded_frame_num % 100 == 0)
			printf("decoded audio frame. num(%d), bytes_per_sample(%d), nb_samples(%d), channels(%d), pts/dts(%.0f/%.0f/%.0f)\n"
				, _decoded_frame_num
				, av_get_bytes_per_sample(_context->sample_fmt)
				, _frame->nb_samples
				, _context->channels	// stride
				, (float)(_frame->pts==AV_NOPTS_VALUE)?-1.0f:_frame->pts
				, (float)(_frame->pkt_dts==AV_NOPTS_VALUE)?-1.0f:_frame->pkt_dts
				, (float)(_frame->pts==AV_NOPTS_VALUE)?-1.0f:_frame->pts);
#endif

		auto out_buf = std::make_unique<MediaBuffer>();

		out_buf->_bytes_per_sample 	= av_get_bytes_per_sample(_context->sample_fmt);
		out_buf->_nb_samples = _frame->nb_samples;
		out_buf->_channels = _context->channels;
		out_buf->_sample_rate = _context->sample_rate;
		out_buf->_channel_layout = _context->channel_layout;
		out_buf->_format = _context->sample_fmt;
		out_buf->SetPts((_frame->pts==AV_NOPTS_VALUE)?-1.0f:_frame->pts);

		uint32_t data_length = (uint32_t)(out_buf->_bytes_per_sample * out_buf->_nb_samples * out_buf->_channels);

		// 메모리를 미리 할당함
		out_buf->Resize(data_length);

		uint8_t *buf_ptr = out_buf->GetBuffer();

		for(int i = 0; i < out_buf->_nb_samples ; i++)
		{
			for(int ch = 0; ch < out_buf->_channels ; ch++)
			{
				memcpy(buf_ptr + ch + out_buf->_bytes_per_sample*i, _frame->data[ch] + out_buf->_bytes_per_sample*i, out_buf->_bytes_per_sample);
			}
		}

		av_frame_unref(_frame);

		// Notify가 필요한 경우에 1을 반환, 아닌 경우에는 일반적인 경우로 0을 반환
		return std::make_pair(need_to_change_notify?1:0, std::move(out_buf));
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	while (_pkt_buf.size() > 0) {

		MediaBuffer* cur_pkt = _pkt_buf[0].get();

		if(cur_pkt->GetBufferSize() == 0)
		{
			_pkt_buf.erase(_pkt_buf.begin(), _pkt_buf.begin()+1);
			continue;
		}

        int parsed_size = av_parser_parse2(
        	_parser, 
        	_context, 
        	&_pkt->data, &_pkt->size,
			cur_pkt->GetBuffer() + cur_pkt->_offset, 
			cur_pkt->GetBufferSize() - cur_pkt->_offset, 
			cur_pkt->GetPts(), cur_pkt->GetPts(), 
			0);

        if (parsed_size < 0) {
            logte("Error while parsing\n");
			return std::make_pair(-2, nullptr);
        }

		if(_pkt->size > 0)
        {
        	_pkt->pts = _parser->pts;
        	_pkt->dts = _parser->dts;
			
			int ret = avcodec_send_packet(_context, _pkt);
			if(ret == AVERROR(EAGAIN)) 
			{
				// 더이상 디코딩할 데이터가 없다면 빠짐
				// printf("Error sending a packet for decoding : AVERROR(EAGAIN)\n");
				return std::make_pair(0, nullptr);
			}
			else if(ret == AVERROR_EOF) 
			{
				logte("Error sending a packet for decoding : AVERROR_EOF\n");
			}
			else if(ret == AVERROR(EINVAL))
			{
				logte("Error sending a packet for decoding : AVERROR(EINVAL)\n");
			}
			else if(ret == AVERROR(ENOMEM))
			{
				logte("Error sending a packet for decoding : AVERROR(ENOMEM)\n");
			}
			else if(ret < 0)
			{
				logte("Error sending a packet for decoding : ERROR(Unknown %d)\n", ret);
				return std::make_pair(-1, nullptr);
			}
		}

		// send_packet 이 완료된 이후에 데이터를 삭제해야함.
        if(parsed_size > 0)
        {
        	cur_pkt->_offset += parsed_size;
        	if(cur_pkt->_offset >= cur_pkt->GetBufferSize())
        	{
        		_pkt_buf.erase(_pkt_buf.begin(), _pkt_buf.begin()+1);
        	}
     	}
	}

	return std::make_pair(-1, nullptr);
}
