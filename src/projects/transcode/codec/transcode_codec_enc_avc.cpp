//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_avc.h"

#define OV_LOG_TAG "TranscodeCodec"

bool OvenCodecImplAvcodecEncAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	AVCodec *codec = avcodec_find_encoder(GetCodecID());
	if(!codec)
	{
		logte("Codec not found");
		return false;
	}

	_context = avcodec_alloc_context3(codec);
	if(!_context)
	{
		logte("Could not allocate video codec context");
		return false;
	}

    _context->bit_rate = _transcode_context->GetBitrate();
	_context->rc_max_rate = _context->bit_rate;
	_context->rc_buffer_size = static_cast<int>(_context->bit_rate * 2);
	_context->sample_aspect_ratio = (AVRational){ 1, 1 };
	_context->time_base = (AVRational){
		_transcode_context->GetTimeBase().GetNum(),
        static_cast<int>(_transcode_context->GetFrameRate())
	};
    _context->framerate = av_d2q(_transcode_context->GetFrameRate(), AV_TIME_BASE);
    _context->gop_size = _transcode_context->GetGOP();
    _context->max_b_frames = 0;
	_context->pix_fmt = AV_PIX_FMT_YUV420P;
	_context->width = _transcode_context->GetVideoWidth();
	_context->height = _transcode_context->GetVideoHeight();
	_context->thread_count = 4;

    av_opt_set(_context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(_context->priv_data, "tune", "zerolatency", 0);
    av_opt_set(_context->priv_data, "x264opts", "no-mbtree:sliced-threads:sync-lookahead=0", 0);

	// open codec
	if(avcodec_open2(_context, codec, nullptr) < 0)
	{
		logte("Could not open codec");
		return false;
	}

	return true;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAVC::RecvBuffer(TranscodeResult *result)
{
	int ret = avcodec_receive_packet(_context, _pkt);

    if(ret == AVERROR(EAGAIN))
    {
        // Packet is enqueued to encoder's buffer
    }
    else if(ret == AVERROR_EOF)
    {
        logte("Error receiving a packet for decoding : AVERROR_EOF");

        *result = TranscodeResult::DataError;
        return nullptr;
    }
    else if(ret < 0)
    {
        logte("Error receiving a packet for encoding : %d", ret);

        *result = TranscodeResult::DataError;
        return nullptr;
    }
    else
    {
        auto packet_buffer = std::make_unique<MediaPacket>(
                common::MediaType::Video,
                0,
                _pkt->data,
                _pkt->size,
                _pkt->dts,
                (_pkt->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag
        );
        packet_buffer->_frag_hdr = MakeFragmentHeader();

        av_packet_unref(_pkt);

        *result = TranscodeResult::DataReady;
        return std::move(packet_buffer);
    }

	while(_input_buffer.size() > 0)
	{
		auto buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaFrame *frame = buffer.get();
		OV_ASSERT2(frame != nullptr);

		_frame->format = frame->GetFormat();
		_frame->width = frame->GetWidth();
		_frame->height = frame->GetHeight();
		_frame->pts = frame->GetPts();

		_frame->linesize[0] = frame->GetStride(0);
		_frame->linesize[1] = frame->GetStride(1);
		_frame->linesize[2] = frame->GetStride(2);

		if(av_frame_get_buffer(_frame, 32) < 0)
		{
			logte("Could not allocate the video frame data");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		ret = avcodec_send_frame(_context, _frame);

		if(ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
		}

		av_frame_unref(_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

std::unique_ptr<FragmentationHeader> OvenCodecImplAvcodecEncAVC::MakeFragmentHeader()
{
	auto fragment_header = std::make_unique<FragmentationHeader>();

	int nal_pattern_size = 4;
	int sps_start_index = -1;
	int sps_end_index = -1;
	int pps_start_index = -1;
	int pps_end_index = -1;
	int fragment_count = 0;
	uint32_t current_index = 0;

	while ((current_index + nal_pattern_size) < _pkt->size)
	{
		if (_pkt->data[current_index] == 0 && _pkt->data[current_index + 1] == 0 &&
			_pkt->data[current_index + 2] == 0 && _pkt->data[current_index + 3] == 1)
		{
			// Pattern 0x00 0x00 0x00 0x01
			nal_pattern_size = 4;
		}
		else if (_pkt->data[current_index] == 0 && _pkt->data[current_index + 1] == 0 &&
				 _pkt->data[current_index + 2] == 1)
		{
			// Pattern 0x00 0x00 0x01
			nal_pattern_size = 3;

		}
		else
		{
			current_index++;
			continue;
		}

        fragment_count++;

		if (sps_start_index == -1)
		{
			sps_start_index = current_index + nal_pattern_size;
			current_index += nal_pattern_size;

            if(_pkt->flags != AV_PKT_FLAG_KEY)
            {
                break;
            }
		}
		else if (sps_end_index == -1)
		{
			sps_end_index = current_index - 1;
			pps_start_index = current_index + nal_pattern_size;
			current_index += nal_pattern_size;
		}
		else // (pps_end_index == -1)
		{
			pps_end_index = current_index - 1;
			break;
		}
	}

    fragment_header->VerifyAndAllocateFragmentationHeader(fragment_count);

    if (_pkt->flags == AV_PKT_FLAG_KEY) // KeyFrame
	{
	    // SPS + PPS + IDR
		fragment_header->fragmentation_offset[0] = sps_start_index;
		fragment_header->fragmentation_offset[1] = pps_start_index;
		fragment_header->fragmentation_offset[2] = (pps_end_index + 1) + nal_pattern_size;

		fragment_header->fragmentation_length[0] = sps_end_index - (sps_start_index - 1);
		fragment_header->fragmentation_length[1] = pps_end_index - (pps_start_index - 1);
		fragment_header->fragmentation_length[2] = _pkt->size - (pps_end_index + nal_pattern_size);
	}
	else
	{
        // NON-IDR
		fragment_header->fragmentation_offset[0] = sps_start_index ;
		fragment_header->fragmentation_length[0] = _pkt->size - (sps_start_index - 1);
	}

	return std::move(fragment_header);
}
