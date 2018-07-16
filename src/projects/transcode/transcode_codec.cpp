#include "transcode_codec.h"

#include "codec/transcode_codec_dec_avc.h"
#include "codec/transcode_codec_enc_avc.h"

#include "codec/transcode_codec_dec_aac.h"
#include "codec/transcode_codec_enc_aac.h"

#include "codec/transcode_codec_enc_vp8.h"
#include "codec/transcode_codec_enc_opus.h"

TranscodeCodec::TranscodeCodec() :
	_impl(NULL)
{
	av_log_set_level(AV_LOG_DEBUG);
}

TranscodeCodec::TranscodeCodec(MediaCodecId codec_id, bool is_encoder, std::shared_ptr<TranscodeContext> context) :
	_impl(NULL)
{
	Configure(codec_id, is_encoder, context);
}

TranscodeCodec::~TranscodeCodec()
{
	if(_impl != nullptr)
		delete _impl;
}


void TranscodeCodec::Configure(MediaCodecId codec_id, bool is_encoder, std::shared_ptr<TranscodeContext> context)
{	
	//////////////////////////////////////////////////////////////////
	// 디코더 
	//////////////////////////////////////////////////////////////////
	if(is_encoder == false)
	{
		switch(codec_id)
		{
		case MediaCodecId::H264:
			_impl = new OvenCodecImplAvcodecDecAVC();
		break;
		
		case MediaCodecId::Aac:
			_impl = new OvenCodecImplAvcodecDecAAC();
		break;
		
		default:
		break;
		}	
	}

	//////////////////////////////////////////////////////////////////
	// 인코더 
	//////////////////////////////////////////////////////////////////
	else
	{
		switch(codec_id)
		{
		case MediaCodecId::H264:
			_impl = new OvenCodecImplAvcodecEncAVC();
		break;

		case MediaCodecId::Aac:
			_impl = new OvenCodecImplAvcodecEncAAC();
		break;

		case MediaCodecId::Vp8:
			_impl = new OvenCodecImplAvcodecEncVP8();
			break;
			
		case MediaCodecId::Opus:
			_impl = new OvenCodecImplAvcodecEncOpus();
		default:
		break;
		}		
	}

	// 트랜스코딩 컨텍스트 정보 전달
	_impl->Configure(context);
}

int32_t TranscodeCodec::SendBuffer(std::unique_ptr<MediaBuffer> buf)
{
	_impl->sendbuf(std::move(buf));

	return 0;
}


std::pair<int32_t, std::unique_ptr<MediaBuffer>> TranscodeCodec::RecvBuffer()
{
	auto obj = _impl->recvbuf();

	return obj;
}
