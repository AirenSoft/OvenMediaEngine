//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../transcode_context.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
}

#include <cstdint>
#include <vector>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include <base/ovlibrary/ovlibrary.h>
#include <base/media_route/media_buffer.h>
#include <base/media_route/media_type.h>

enum class TranscodeResult : int32_t
{
	// An error occurred while process the packet/frame
	DataError = -4,
	// An error occurred while parse the packet using av_parser_parse2()
	ParseError = -3,
	// There is no data to process
	NoData = -2,
	// 파일의 끝
	EndOfFile = -1,

	// 프레임 디코딩 완료, 아직 디코딩할 데이터가 있음
	DataReady = 0,
	// 포맷이 변경됨
	FormatChanged = 1,
};

template<typename InputType, typename OutputType>
class TranscodeBase
{
public:
	virtual ~TranscodeBase() = default;

	virtual AVCodecID GetCodecID() const noexcept = 0;

	virtual bool Configure(std::shared_ptr<TranscodeContext> context) = 0;

	virtual void SendBuffer(std::unique_ptr<const InputType> buf) = 0;
	virtual std::unique_ptr<OutputType> RecvBuffer(TranscodeResult *result) = 0;

protected:
	static bool IsPlanar(AVSampleFormat format)
	{
		switch(format)
		{
			case AV_SAMPLE_FMT_U8:
			case AV_SAMPLE_FMT_S16:
			case AV_SAMPLE_FMT_S32:
			case AV_SAMPLE_FMT_FLT:
			case AV_SAMPLE_FMT_DBL:
			case AV_SAMPLE_FMT_S64:
				return false;

			case AV_SAMPLE_FMT_U8P:
			case AV_SAMPLE_FMT_S16P:
			case AV_SAMPLE_FMT_S32P:
			case AV_SAMPLE_FMT_FLTP:
			case AV_SAMPLE_FMT_DBLP:
			case AV_SAMPLE_FMT_S64P:
				return true;

			default:
				return false;
		}
	}

	std::vector<std::unique_ptr<const InputType>> _input_buffer;
	std::vector<std::unique_ptr<OutputType>> _output_buffer;
};

