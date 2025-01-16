//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../transcoder_context.h"


#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>
#include <base/ovlibrary/ovlibrary.h>
#include <modules/ffmpeg/ffmpeg_conv.h>
#include <modules/managed_queue/managed_queue.h>

#include <algorithm>
#include <stdint.h>
#include <memory>
#include <thread>
#include <vector>


enum class TranscodeResult : int32_t
{
	Again = -5,
	// An error occurred while process the packet/frame
	DataError = -4,
	// An error occurred while parse the packet using av_parser_parse2()
	ParseError = -3,
	// There is no data to process
	NoData = -2,
	// End of file
	EndOfFile = -1,
	// Decode/Encoder Complete and Remain more data
	DataReady = 0,
	// Change Output Format
	FormatChanged = 1,
};

class CodecCandidate
{
public:
	CodecCandidate(cmn::MediaCodecId codec_id, cmn::MediaCodecModuleId module_id, int32_t device_id)
		: _codec_id(codec_id),
		  _module_id(module_id),
		  _device_id(device_id)
	{
	}

	cmn::MediaCodecModuleId GetModuleId() const
	{
		return _module_id;
	}

	int32_t GetDeviceId() const
	{
		return _device_id;
	}

	cmn::MediaCodecId GetCodecId() const
	{
		return _codec_id;
	}

private:
	cmn::MediaCodecId _codec_id;
	cmn::MediaCodecModuleId _module_id;
	int32_t _device_id;
};


template <typename InputType, typename OutputType>
class TranscodeBase
{
public:
	virtual ~TranscodeBase() = default;

	virtual AVCodecID GetCodecID() const noexcept = 0;

	virtual bool Configure(std::shared_ptr<MediaTrack> track) = 0;

	virtual void SendBuffer(std::shared_ptr<const InputType> buf) = 0;

	int32_t GetBufferSize() const
	{
		return _input_buffer.Size();
	}
	
protected:
	ov::ManagedQueue<std::shared_ptr<const InputType>> _input_buffer;
};
