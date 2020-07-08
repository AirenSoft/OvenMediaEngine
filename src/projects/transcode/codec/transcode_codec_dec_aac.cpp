//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_dec_aac.h"
#include "base/info/application.h"

#define OV_LOG_TAG "TranscodeCodec"

std::shared_ptr<MediaFrame> OvenCodecImplAvcodecDecAAC::Dequeue(TranscodeResult *result)
{
	// Check the decoded frame is available
	int ret = ::avcodec_receive_frame(_context, _frame);

	if (ret == AVERROR(EAGAIN))
	{
		// Wait for more packet
	}
	else if (ret == AVERROR_EOF)
	{
		logte("Error receiving a packet for decoding : AVERROR_EOF");
		*result = TranscodeResult::EndOfFile;
		return nullptr;
	}
	else if (ret < 0)
	{
		logte("Error receiving a packet for decoding : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		bool need_to_change_notify = false;

		// Update codec informations if needed
		if (_change_format == false)
		{
			ret = ::avcodec_parameters_from_context(_codec_par, _context);

			if (ret == 0)
			{
				auto codec_info = ShowCodecParameters(_context, _codec_par);

				logti("[%s/%s(%u)] input stream information: %s", 
					_stream_info.GetApplicationInfo().GetName().CStr(), _stream_info.GetName().CStr(), _stream_info.GetId(), codec_info.CStr());

				_change_format = true;

				// If the format is changed, notify to another module
				need_to_change_notify = true;
			}
			else
			{
				logte("Could not obtain codec paramters from context %p", _context);
			}
		}

		_decoded_frame_num++;

		auto output_frame = std::make_shared<MediaFrame>();

		output_frame->SetMediaType(common::MediaType::Audio);
		output_frame->SetBytesPerSample(::av_get_bytes_per_sample(static_cast<AVSampleFormat>(_frame->format)));
		output_frame->SetNbSamples(_frame->nb_samples);
		output_frame->SetChannels(_frame->channels);
		output_frame->SetSampleRate(_frame->sample_rate);
		output_frame->SetFormat(_frame->format);

		// Calculate duration of frame in time_base
		float frame_duration_in_second = _frame->nb_samples * (1.0f / _frame->sample_rate);
		int frame_duration_in_timebase = static_cast<int>(frame_duration_in_second * _context->time_base.den);
		output_frame->SetDuration(frame_duration_in_timebase);

		// If the decoded audio frame does not have a PTS, Increase frame duration time in PTS of previous frame
		output_frame->SetPts(static_cast<int64_t>((_frame->pts == AV_NOPTS_VALUE) ? _last_pkt_pts+frame_duration_in_timebase : _frame->pts));
		_last_pkt_pts = output_frame->GetPts();


		auto data_length = static_cast<uint32_t>(output_frame->GetBytesPerSample() * output_frame->GetNbSamples());

		// Copy frame data into out_buf
		if (TranscodeBase::IsPlanar(output_frame->GetFormat<AVSampleFormat>()))
		{
			// If the frame is planar, the data is stored separately in the "_frame->data" array.
			for (int channel = 0; channel < _frame->channels; channel++)
			{
				output_frame->Resize(data_length, channel);
				uint8_t *output = output_frame->GetWritableBuffer(channel);
				::memcpy(output, _frame->data[channel], data_length);
			}
		}
		else
		{
			// If the frame is non-planar, it means interleaved data. So, just copy from "_frame->data[0]" into the output_frame
			output_frame->AppendBuffer(_frame->data[0], data_length * _frame->channels, 0);
		}

		::av_frame_unref(_frame);

		// Return 1, if notification is required
		*result = need_to_change_notify ? TranscodeResult::FormatChanged : TranscodeResult::DataReady;
		return std::move(output_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

void OvenCodecImplAvcodecDecAAC::Enqueue(TranscodeResult *result)
{
	if(cur_pkt == nullptr && _input_buffer.empty() == false)
	{
		auto packet = std::move(_input_buffer.front());
		_input_buffer.pop_front();

		cur_pkt = packet.get();

		if (cur_pkt != nullptr)
		{
			cur_data = cur_pkt->GetData();
			_pkt_offset = 0;
		}

		if ((cur_data == nullptr) || (cur_data->GetLength() == 0))
		{
			logte("there is no packets");
			*result = TranscodeResult::NoData;
			return;
		}
	}

	if (cur_data != nullptr)
	{
		if(cur_data->GetLength() > _pkt_offset)
		{
			int32_t parsed_size = ::av_parser_parse2(
				_parser,
				_context,
				&_pkt->data, &_pkt->size,
				cur_data->GetDataAs<uint8_t>() + _pkt_offset,
				static_cast<int32_t>(cur_data->GetLength() - _pkt_offset),
				cur_pkt->GetPts(), cur_pkt->GetPts(),
				0);

			// Failed to parsing
			if (parsed_size < 0)
			{
				logte("Error while parsing\n");
				cur_pkt = nullptr;
				cur_data = nullptr;
				_pkt_offset = 0;
				
				*result = TranscodeResult::ParseError;
				return;
			}	

			if (_pkt->size > 0)
			{
				// logte("cur_data.length(%d), parsed_size(%d) %lld/%lld", cur_data->GetLength(), parsed_size, _parser->pts, _parser->dts);
				
				_pkt->pts = _parser->pts;
				_pkt->dts = _parser->dts;

				int ret = ::avcodec_send_packet(_context, _pkt);

				if (ret == AVERROR(EAGAIN))
				{
					// Need more data
					logte("Error sending a packet for decoding : EAGAIN");
					*result = TranscodeResult::DataReady;
					return ;
				}
				else if (ret == AVERROR_EOF)
				{
					logte("Error sending a packet for decoding : AVERROR_EOF");
					*result = TranscodeResult::EndOfFile;
				}
				else if (ret == AVERROR(EINVAL))
				{
					logte("Error sending a packet for decoding : AVERROR(EINVAL)");
					*result = TranscodeResult::DataError;
					return;
				}
				else if (ret == AVERROR(ENOMEM))
				{
					logte("Error sending a packet for decoding : AVERROR(ENOMEM)");
					*result = TranscodeResult::DataError;
				}
				else if (ret < 0)
				{
					logte("Error sending a packet for decoding : ERROR(Unknown %d)", ret);
					*result = TranscodeResult::DataError;
					return;
				}
			}

			if (parsed_size > 0)
			{
				OV_ASSERT(cur_data->GetLength() >= (size_t)parsed_size, "Current data size MUST greater than parsed_size, but data size: %ld, parsed_size: %ld", cur_data->GetLength(), parsed_size);

				_pkt_offset += parsed_size;
			}


			*result = TranscodeResult::DataReady;
			return;
		}

		if(cur_data->GetLength() <= _pkt_offset)
		{
			cur_pkt = nullptr;
			cur_data = nullptr;
			_pkt_offset = 0;
		}
	}

	if(_input_buffer.size() > 0)
		*result = TranscodeResult::DataReady;
	else
		*result = TranscodeResult::NoData;
}

std::shared_ptr<MediaFrame> OvenCodecImplAvcodecDecAAC::RecvBuffer(TranscodeResult *result)
{
	Enqueue(result);
	if((int)(*result) < 0)
	{
		return nullptr;
	}

	return Dequeue(result);
}
