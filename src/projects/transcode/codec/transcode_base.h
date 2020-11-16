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
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <cstdint>
#include <vector>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include <base/ovlibrary/ovlibrary.h>
#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>

enum class TranscodeResult : int32_t
{
	// An error occurred while process the packet/frame
	DataError = -4,
	// An error occurred while parse the packet using av_parser_parse2()
	ParseError = -3,
	// There is no data to process
	NoData = -2,
	// End of file
	EndOfFile = -1,
	// Decode/Encoder Complete and Reamin more data
	DataReady = 0,
	// Change Output Format
	FormatChanged = 1,
};

template<typename InputType, typename OutputType>
class TranscodeBase
{
public:
	virtual ~TranscodeBase() = default;

	virtual AVCodecID GetCodecID() const noexcept = 0;

	virtual bool Configure(std::shared_ptr<TranscodeContext> context) = 0;

	virtual void SendBuffer(std::shared_ptr<const InputType> buf) = 0;
	virtual std::shared_ptr<OutputType> RecvBuffer(TranscodeResult *result) = 0;

	static AVRational TimebaseToAVRational(const cmn::Timebase &timebase)
	{
		return (AVRational){
			.num = timebase.GetNum(),
			.den = timebase.GetDen()
		};
	}

	uint32_t GetInputBufferSize()
	{
		return _input_buffer.size();
	}

	uint32_t GetOutputBufferSize()
	{
		return _output_buffer.size();
	}

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

	std::deque<std::shared_ptr<const InputType>> _input_buffer;
	std::deque<std::shared_ptr<OutputType>> _output_buffer;
};

