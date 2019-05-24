//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_decoder.h"

#include "transcode_codec_dec_aac.h"
#include "transcode_codec_dec_avc.h"

#define OV_LOG_TAG "TranscodeCodec"

TranscodeDecoder::TranscodeDecoder()
{
	avcodec_register_all();

	_pkt = av_packet_alloc();
	_frame = av_frame_alloc();

}

TranscodeDecoder::~TranscodeDecoder()
{
	avcodec_free_context(&_context);
	avcodec_parameters_free(&_codec_par);

	av_frame_free(&_frame);
	av_packet_free(&_pkt);

	av_parser_close(_parser);
}

std::unique_ptr<TranscodeDecoder> TranscodeDecoder::CreateDecoder(common::MediaCodecId codec_id, std::shared_ptr<TranscodeContext> transcode_context)
{
	std::unique_ptr<TranscodeDecoder> decoder = nullptr;

	switch(codec_id)
	{
		case common::MediaCodecId::H264:
			decoder = std::make_unique<OvenCodecImplAvcodecDecAVC>();
			break;

		case common::MediaCodecId::Aac:
			decoder = std::make_unique<OvenCodecImplAvcodecDecAAC>();
			break;

		default:
			OV_ASSERT(false, "Not supported codec: %d", codec_id);
			break;
	}

	if(decoder != nullptr)
	{
		if(!decoder->Configure(transcode_context))
		{
			return nullptr;
		}
	}

	return std::move(decoder);
}

bool TranscodeDecoder::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	_codec = avcodec_find_decoder(GetCodecID());

	if(_codec == nullptr)
	{
		logte("Codec not found");
		return false;
	}

	// create codec context
	_context = avcodec_alloc_context3(_codec);

	if(_context == nullptr)
	{
		logte("Could not allocate video codec context");
		return false;
	}

	if(avcodec_open2(_context, _codec, nullptr) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	_parser = av_parser_init(_codec->id);

	if(!_parser)
	{
		logte("Parser not found");
		return false;
	}

	return true;
}

void TranscodeDecoder::SendBuffer(std::unique_ptr<const MediaPacket> packet)
{
	_input_buffer.push_back(std::move(packet));
}

void TranscodeDecoder::ShowCodecParameters(const AVCodecParameters *parameters)
{
	ov::String message;

	switch(parameters->codec_type)
	{
		case AVMEDIA_TYPE_UNKNOWN:
			message = "Unknown media type";
			break;

		case AVMEDIA_TYPE_VIDEO:
			message.Format(
				"codec_type(%d) "
				"codec_id(%d) "
				"codec_tag(%d) "
				"extra(%d) "
				"format(%d) "
				"bit_rate(%d) "
				"bits_per_coded_sample(%d) "
				"bits_per_raw_sample(%d) "
				"profile(%d) "
				"level(%d) "
				"sample_aspect_ratio(%d/%d) "

				"width(%d) "
				"height(%d) "

				"field_order(%d) "
				"color_range(%d) "
				"color_primaries(%d) "
				"color_trc(%d) "
				"color_space(%d) "
				"chroma_location(%d) "

				"channel_layout(%ld) "
				"channels(%d) "
				"sample_rate(%d) "
				"block_align(%d) "
				"frame_size(%d)",

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
			break;

		case AVMEDIA_TYPE_AUDIO:
			message = "Audio";
			break;

		case AVMEDIA_TYPE_DATA:
			message = "Data";
			break;

		case AVMEDIA_TYPE_SUBTITLE:
			message = "Subtitle";
			break;

		case AVMEDIA_TYPE_ATTACHMENT:
			message = "Attachment";
			break;

		case AVMEDIA_TYPE_NB:
			message = "NB";
			break;
	}

	logti("Codec parameters: %s", message.CStr());
}