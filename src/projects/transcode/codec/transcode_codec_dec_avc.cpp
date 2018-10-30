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

#define DEBUG_PREVIEW 0

bool OvenCodecImplAvcodecDecAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	if(WelsCreateDecoder(&_decoder))
	{
		logte("Unable to create H264 decoder");
		return false;
	}

	SDecodingParam param = { nullptr };
	param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
	param.eEcActiveIdc = ERROR_CON_DISABLE;

#if !OPENH264_VER_AT_LEAST(1, 6)
	param.eOutputColorFormat = videoFormatI420;
#endif

	if(_decoder->Initialize(&param))
	{
		logte("H264 decoder initialize failed");
		WelsDestroyDecoder(_decoder);
		return false;
	}
	_format_changed = true;

	return true;
}

std::unique_ptr<MediaFrame> OvenCodecImplAvcodecDecAVC::RecvBuffer(TranscodeResult *result)
{
	int64_t pts;
	SBufferInfo info;
	uint8_t* ptrs[3];

	if(_input_buffer.empty())
	{
		*result = TranscodeResult::NoData;
		return nullptr;
	}

	while(!_input_buffer.empty())
	{
		auto packet_buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaPacket *packet = packet_buffer.get();
		OV_ASSERT2(packet != nullptr);

		auto &data = packet->GetData();
		pts = packet->GetPts();

		::memset(&info, 0, sizeof(SBufferInfo));

		// DecodeFrameNoDelay = DecodeFrame2(src, len, ...) + DecodeFrame2(NULL, 0, ...)
		DECODING_STATE state = _decoder->DecodeFrameNoDelay(
			static_cast<const u_char *>(data->GetData()),
			static_cast<const int>(data->GetLength()),
			ptrs,
			&info
		);

		if (state != dsErrorFree)
		{
			// Todo RequestIDR or something like that.
			logte("DecodeFrameNoDelay failed, state=%d,len=%d", state, data->GetLength());

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(info.iBufferStatus != 1)
		{
			// 0 : one frame is not ready
			// 1 : one frame is ready
			continue;
		}

		auto frame_buffer = std::make_unique<MediaFrame>();

		frame_buffer->SetWidth(info.UsrData.sSystemBuffer.iWidth);
		frame_buffer->SetHeight(info.UsrData.sSystemBuffer.iHeight);
		frame_buffer->SetPts((pts == 0) ? -1 : pts);

		frame_buffer->SetStride(info.UsrData.sSystemBuffer.iStride[0], 0);
		frame_buffer->SetStride(info.UsrData.sSystemBuffer.iStride[1], 1);
		frame_buffer->SetStride(info.UsrData.sSystemBuffer.iStride[1], 2);

		frame_buffer->SetBuffer(ptrs[0], frame_buffer->GetStride(0) * frame_buffer->GetHeight(), 0);
		frame_buffer->SetBuffer(ptrs[1], frame_buffer->GetStride(1) * frame_buffer->GetHeight() / 2, 1);
		frame_buffer->SetBuffer(ptrs[2], frame_buffer->GetStride(2) * frame_buffer->GetHeight() / 2, 2);

		*result = TranscodeResult::DataReady;

		if(_format_changed)
		{
			*result = TranscodeResult::FormatChanged;
			_format_changed = false;
		}
		return std::move(frame_buffer);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

