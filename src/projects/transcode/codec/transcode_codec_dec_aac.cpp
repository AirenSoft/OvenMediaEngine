//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_dec_aac.h"

#define OV_LOG_TAG "TranscodeCodec"

std::unique_ptr<MediaFrame> OvenCodecImplAvcodecDecAAC::RecvBuffer(TranscodeResult *result)
{
    // Check the decoded frame is available
	int ret = avcodec_receive_frame(_context, _frame);

	if(ret == AVERROR(EAGAIN))
	{
		// 패킷을 넣음
	}
	else if(ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");
		*result = TranscodeResult::EndOfFile;
		return nullptr;
	}
	else if(ret < 0)
	{
		logte("Error receiving a packet for decoding : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
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
				logti("Codec parameters: codec_type(%d), codec_id(%d), codec_tag(%d), extra(%d), format(%d), bit_rate(%d), bits_per_coded_sample(%d), bits_per_raw_sample(%d), profile(%d), level(%d), sample_aspect_ratio(%d/%d) width(%d), height(%d) field_order(%d) color_range(%d) color_primaries(%d) color_trc(%d) color_space(%d) chroma_location(%d), channel_layout(%ld) channels(%d) sample_rate(%d) block_align(%d) frame_size(%d)",
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

				_change_format = true;
				// 포맷이 변경이 되면, 변경된 정보를 알려줘야함. 

				need_to_change_notify = true;
			}
		}

		_decoded_frame_num++;

		auto output_frame = std::make_unique<MediaFrame>();

		output_frame->SetBytesPerSample(av_get_bytes_per_sample(static_cast<AVSampleFormat>(_frame->format)));
		output_frame->SetNbSamples(_frame->nb_samples);
		output_frame->SetChannels(_frame->channels);
		output_frame->SetSampleRate(_frame->sample_rate);
		output_frame->SetFormat(_frame->format);
		output_frame->SetPts(static_cast<int64_t>((_frame->pts == AV_NOPTS_VALUE) ? -1LL : _frame->pts));

		auto data_length = static_cast<uint32_t>(output_frame->GetBytesPerSample() * output_frame->GetNbSamples());

		// Copy frame data into out_buf
		if(TranscodeBase::IsPlanar(output_frame->GetFormat<AVSampleFormat>()))
		{
			// If the frame is planar, the data is stored separately in the "_frame->data" array.
			for(int channel = 0; channel < _frame->channels; channel++)
			{
				output_frame->Resize(data_length, channel);
				uint8_t *output = output_frame->GetBuffer(channel);
				::memcpy(output, _frame->data[channel], data_length);
			}
		}
		else
		{
			// If the frame is non-planar, it means interleaved data. So, just copy from "_frame->data[0]" into the output_frame
			output_frame->AppendBuffer(_frame->data[0], data_length * _frame->channels, 0);
		}

		av_frame_unref(_frame);

		// Return 1, if notification is required
		*result = need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady;
		return std::move(output_frame);
	}

	///////////////////////////////////////////////////
	// 인코딩 요청
	///////////////////////////////////////////////////
	off_t offset = 0;

	while(_input_buffer.empty() == false)
	{
		auto packet = std::move(_input_buffer[0]);
        _input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

        const MediaPacket *cur_pkt = packet.get();

		std::shared_ptr<const ov::Data> cur_data = nullptr;

		if(cur_pkt != nullptr)
		{
			cur_data = cur_pkt->GetData();
		}

		if((cur_data == nullptr) || (cur_data->GetLength() == 0))
		{
			continue;
		}

		logtp("Decoding AAC packet\n%s", cur_data->Dump(32).CStr());

		int parsed_size = av_parser_parse2(
			_parser,
			_context,
			&_pkt->data, &_pkt->size,
			cur_data->GetDataAs<uint8_t>() + offset,
			static_cast<int>(cur_data->GetLength() - offset),
			cur_pkt->GetPts(), cur_pkt->GetPts(),
			0);

		if(parsed_size < 0)
		{
			logte("Error while parsing\n");
			*result = TranscodeResult::ParseError;
			return nullptr;
		}

		if(_pkt->size > 0)
		{
			_pkt->pts = _parser->pts;
			_pkt->dts = _parser->dts;

			ret = avcodec_send_packet(_context, _pkt);

			if(ret == AVERROR(EAGAIN))
			{
                // Need more data
			}
			else if(ret == AVERROR_EOF)
			{
				logte("Error sending a packet for decoding : AVERROR_EOF");
			}
			else if(ret == AVERROR(EINVAL))
			{
				logte("Error sending a packet for decoding : AVERROR(EINVAL)");
                *result = TranscodeResult::DataError;
                return nullptr;
			}
			else if(ret == AVERROR(ENOMEM))
			{
				logte("Error sending a packet for decoding : AVERROR(ENOMEM)");
			}
			else if(ret < 0)
			{
				logte("Error sending a packet for decoding : ERROR(Unknown %d)", ret);
                *result = TranscodeResult::DataError;
                return nullptr;
			}
		}

		if(parsed_size > 0)
		{
			OV_ASSERT(cur_data->GetLength() >= (unsigned int)parsed_size, "Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld", cur_data->GetLength(), parsed_size);

			offset += parsed_size;

			if(cur_data->GetLength() <= (unsigned int)parsed_size)
			{
				offset = 0;
			}
		}
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

