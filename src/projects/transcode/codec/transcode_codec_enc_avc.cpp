//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_avc.h"
#include <unistd.h>

#define OV_LOG_TAG "TranscodeCodec"

OvenCodecImplAvcodecEncAVC::~OvenCodecImplAvcodecEncAVC()
{
	if(_encoder)
	{
		_encoder->Uninitialize();
		WelsDestroySVCEncoder(_encoder);
	}
}

bool OvenCodecImplAvcodecEncAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	if(WelsCreateSVCEncoder(&_encoder))
	{
		_encoder->Uninitialize();
		logte("Unable to create H264 encoder");
		return false;
	}

	SEncParamExt param;
	::memset(&param, 0, sizeof(SEncParamExt));

	_encoder->GetDefaultParams(&param);

	param.fMaxFrameRate = context->GetFrameRate();
	param.iPicWidth = context->GetVideoWidth();
	param.iPicHeight = context->GetVideoHeight();
	param.iTargetBitrate = context->GetBitrate();
	param.iRCMode = RC_OFF_MODE;
	param.iTemporalLayerNum = 1;
	param.iSpatialLayerNum = 1;
	param.bEnableDenoise = false;
	param.bEnableBackgroundDetection = true;
	param.bEnableAdaptiveQuant = true;
	param.bEnableFrameSkip = false;
	param.bEnableLongTermReference = false;
	param.iLtrMarkPeriod = 30;
	param.uiIntraPeriod = 30; // KeyFrame Interval (1 sec)
	param.eSpsPpsIdStrategy = CONSTANT_ID;
	param.bPrefixNalAddingCtrl = false;
	param.sSpatialLayers[0].iVideoWidth = param.iPicWidth;
	param.sSpatialLayers[0].iVideoHeight = param.iPicHeight;
	param.sSpatialLayers[0].fFrameRate = param.fMaxFrameRate;
	param.sSpatialLayers[0].iSpatialBitrate = param.iTargetBitrate;
	param.sSpatialLayers[0].iMaxSpatialBitrate = param.iMaxBitrate;
	param.sSpatialLayers[0].uiProfileIdc = PRO_BASELINE;
	param.sSpatialLayers[0].uiLevelIdc = LEVEL_3_1;     // baseline & lvl 3.1 => profile-level-id=42e01f

	if(_encoder->InitializeExt(&param))
	{
		logte("H264 encoder initialize failed");
		return false;
	}

	int videoFormat = videoFormatI420;
	_encoder->SetOption (ENCODER_OPTION_DATAFORMAT, &videoFormat);

	return true;
}

void OvenCodecImplAvcodecEncAVC::SendBuffer(std::unique_ptr<const MediaFrame> frame)
{
	TranscodeEncoder::SendBuffer(std::move(frame));
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAVC::RecvBuffer(TranscodeResult *result)
{
	*result = TranscodeResult::NoData;

	if(_input_buffer.empty())
	{
		return nullptr;
	}

	while(!_input_buffer.empty())
	{
		auto frame_buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaFrame *frame = frame_buffer.get();
		OV_ASSERT2(frame != nullptr);

		const uint8_t start_code[4] = { 0, 0, 0, 1 };
		const int64_t pts = frame->GetPts();
		SFrameBSInfo fbi = { 0 };
		SSourcePicture sp = { 0 };
		size_t required_size = 0, fragments_count = 0;

		sp.iColorFormat = videoFormatI420;
		for(int i = 0; i < 3; ++i)
		{
			sp.iStride[i] = frame->GetStride(i);
			sp.pData[i] = (__u_char *)frame->GetBuffer(i);
		}
		sp.iPicWidth = frame->GetWidth();
		sp.iPicHeight = frame->GetHeight();

		if(_encoder->EncodeFrame(&sp, &fbi) != cmResultSuccess)
		{
			logte("Encode Frame Error");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(fbi.eFrameType == videoFrameTypeSkip)
		{
			continue;
		}

		for(int layer = 0; layer < fbi.iLayerNum; ++layer)
		{
			const SLayerBSInfo &layerInfo = fbi.sLayerInfo[layer];
			for(int nal = 0; nal < layerInfo.iNalCount; ++nal, ++fragments_count)
			{
				required_size += layerInfo.pNalLengthInByte[nal];
			}
		}

		if(required_size == 0)
		{
			logte("Encode Frame Error");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		auto encoded = std::make_unique<uint8_t[]>(required_size);
		auto frag_hdr = std::make_unique<FragmentationHeader>();

		if(fragments_count > MAX_FRAG_COUNT)
		{
			logte("Unexpected H264 fragments_count=%d", fragments_count);
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		frag_hdr->VerifyAndAllocateFragmentationHeader(fragments_count);
		size_t frag = 0;
		size_t encoded_length = 0;
		for(int layer = 0; layer < fbi.iLayerNum; ++layer)
		{
			const SLayerBSInfo &layerInfo = fbi.sLayerInfo[layer];
			size_t layer_len = 0;
			for(int nal = 0; nal < layerInfo.iNalCount; ++nal, ++frag)
			{
				frag_hdr->fragmentation_offset[frag] =
					encoded_length + layer_len + sizeof(start_code);
				frag_hdr->fragmentation_length[frag] =
					layerInfo.pNalLengthInByte[nal] - sizeof(start_code);
				layer_len += layerInfo.pNalLengthInByte[nal];
			}
			::memcpy(encoded.get() + encoded_length, layerInfo.pBsBuf, layer_len);
			encoded_length += layer_len;
		}

		/*
		NON-IDR(0x61) - fragments_count : 1
			00 | 00 00 00 01 61 E0 00 40 00 9C 8F 03 8F 4E 28 6F
			10 | A7 D0 D7 23 55 B8 1C 3A BA 07 8E 49 68 FF 88 B9

		SPS(0x67) + PPS(0x68) + IDR(0x65) - fragments_count : 3
			00 | 00 00 00 01 67 42 C0 1E 8C 8D 40 F0 52 40 3C 22
			10 | 11 A8 00 00 00 01 68 CE 3C 80 00 00 00 01 65 B8
		*/

		auto packet_buffer = std::make_unique<MediaPacket>(
			common::MediaType::Video,
			0,
			encoded.get(),
			encoded_length,
			(pts == 0) ? -1 : pts,
			(fbi.eFrameType == videoFrameTypeIDR) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);
		packet_buffer->_frag_hdr = std::move(frag_hdr);

		*result = TranscodeResult::DataReady;
		return std::move(packet_buffer);
	}
	return nullptr;
}