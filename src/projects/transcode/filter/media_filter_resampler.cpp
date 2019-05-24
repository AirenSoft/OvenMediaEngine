//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "media_filter_resampler.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "MediaFilter"

MediaFilterResampler::MediaFilterResampler() :
	_frame(nullptr),
	_buffersink_ctx(nullptr),
	_buffersrc_ctx(nullptr),
	_filter_graph(nullptr),
	_outputs(nullptr),
	_inputs(nullptr)
{
	avfilter_register_all();

	_frame = av_frame_alloc();

	_outputs = avfilter_inout_alloc();

	_inputs = avfilter_inout_alloc();
}

MediaFilterResampler::~MediaFilterResampler()
{
	if(_frame)
	{
		av_frame_free(&_frame);
	}

	if(_outputs)
	{
		avfilter_inout_free(&_outputs);
	}

	if(_inputs)
	{
		avfilter_inout_free(&_inputs);
	}

	if(_filter_graph)
	{
		avfilter_graph_free(&_filter_graph);
	}
}

bool MediaFilterResampler::Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context)
{
	int ret;
	const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");
	const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");

	_filter_graph = avfilter_graph_alloc();
	if(!_outputs || !_inputs || !_filter_graph)
	{
		logte("cannot allocated filter graph");
		return false;
	}

	ov::String input_formats;
	input_formats.Format(
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%x",
		input_media_track->GetTimeBase().GetNum(),                  // Timebase 1
		input_media_track->GetTimeBase().GetDen(),                  // Timebase 1000
		input_media_track->GetSampleRate(),                         // SampleRate
		input_media_track->GetSample().GetName(),                   // SampleFormat
		input_media_track->GetChannel().GetLayout()                 // SampleLayout
	);

	ret = avfilter_graph_create_filter(&_buffersrc_ctx, abuffersrc, "in", input_formats.CStr(), nullptr, _filter_graph);
	if(ret < 0)
	{
		logte("Cannot create buffer source");
		return false;
	}

	ov::String output_filter_descr;

	output_filter_descr.Format(
		"aresample=%d,asetnsamples=n=1024,aformat=sample_fmts=%s:channel_layouts=%s,asettb=expr=%f",
		//"aresample=%d,aformat=sample_fmts=%s:channel_layouts=%s,asettb=expr=%f",
		context->GetAudioSampleRate(),
		context->GetAudioSample().GetName(),
		context->GetAudioChannel().GetName(),
		context->GetTimeBase().GetExpr()
	);

	logtd("resample. track[%u] %s -> %s", input_media_track->GetId(), input_formats.CStr(), output_filter_descr.CStr());

	static const enum AVSampleFormat out_sample_fmts[] = {
		(AVSampleFormat)context->GetAudioSample().GetFormat(),
		(AVSampleFormat)-1
	};
	static const int64_t out_channel_layouts[] = {
		(int64_t)context->GetAudioChannel().GetLayout(),
		-1
	};
	static const int out_sample_rates[] = {
		context->GetAudioSampleRate(),
		-1
	};

	// logtd("out_sample_fmts : %d", context->_audio_sample.GetFormat());
	// logtd("out_channel_layouts : %x", context->_audio_channel.GetLayout());
	// logtd("out_sample_rates : %d", context->GetAudioSampleRate());

	ret = avfilter_graph_create_filter(&_buffersink_ctx, abuffersink, "out", nullptr, nullptr, _filter_graph);
	if(ret < 0)
	{
		logte("Cannot create audio buffer sink");
		return false;
	}

	ret = av_opt_set_int_list(_buffersink_ctx, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN);
	if(ret < 0)
	{
		logte("Cannot set output sample format");
		return false;
	}

	ret = av_opt_set_int_list(_buffersink_ctx, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
	if(ret < 0)
	{
		logte("Cannot set output channel layout");
		return false;
	}

	ret = av_opt_set_int_list(_buffersink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
	if(ret < 0)
	{
		logte("Cannot set output sample rate");
		return false;
	}

	// 필터 연결
	_outputs->name = av_strdup("in");
	_outputs->filter_ctx = _buffersrc_ctx;
	_outputs->pad_idx = 0;
	_outputs->next = nullptr;


	// 버퍼 싱크의 임력 설정
	_inputs->name = av_strdup("out");
	_inputs->filter_ctx = _buffersink_ctx;
	_inputs->pad_idx = 0;
	_inputs->next = nullptr;

	if((ret = avfilter_graph_parse_ptr(_filter_graph, output_filter_descr.CStr(), &_inputs, &_outputs, nullptr)) < 0)
	{
		logte("Cannot create filter graph");
		return false;
	}

	if((ret = avfilter_graph_config(_filter_graph, nullptr)) < 0)
	{
		logte("Cannot validation filter graph");
		return false;
	}

	return true;
}

int32_t MediaFilterResampler::SendBuffer(std::unique_ptr<MediaFrame> buffer)
{
	logtp("Data before resampling:\n%s", ov::Dump(buffer->GetBuffer(0), buffer->GetBufferSize(0), 32).CStr());

	_input_buffer.push_back(std::move(buffer));

	return 0;
}

std::unique_ptr<MediaFrame> MediaFilterResampler::RecvBuffer(TranscodeResult *result)
{
	int ret = av_buffersink_get_frame(_buffersink_ctx, _frame);

	if(ret == AVERROR(EAGAIN))
	{
		// printf("eagain : %d\r\n", ret);
	}
	else if(ret == AVERROR_EOF)
	{
		logte("eof : %d", ret);
		*result = TranscodeResult::EndOfFile;
		return nullptr;
	}
	else if(ret < 0)
	{
		logte("error : %d", ret);
		*result = TranscodeResult::DataError;
		return nullptr;
	}
	else
	{
		auto output_frame = std::make_unique<MediaFrame>();

		output_frame->SetFormat(_frame->format);
		output_frame->SetBytesPerSample(av_get_bytes_per_sample((AVSampleFormat)_frame->format));
		output_frame->SetNbSamples(_frame->nb_samples);
		output_frame->SetChannels(_frame->channels);
		output_frame->SetSampleRate(_frame->sample_rate);
		output_frame->SetChannelLayout((common::AudioChannel::Layout)_frame->channel_layout);

		output_frame->SetPts((_frame->pts == AV_NOPTS_VALUE) ? -1L : _frame->pts);

		auto data_length = static_cast<uint32_t>(output_frame->GetBytesPerSample() * output_frame->GetNbSamples());

		// Copy frame data into out_buf
		if(IsPlanar(static_cast<AVSampleFormat>(_frame->format)))
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

		logtp("Resampled data:\n%s", ov::Dump(_frame->data[0], _frame->linesize[0], 32).CStr());

		av_frame_unref(_frame);

		// Notify가 필요한 경우에 1을 반환, 아닌 경우에는 일반적인 경우로 0을 반환
		*result = TranscodeResult::DataReady;
		return std::move(output_frame);
	}

	while(_input_buffer.empty() == false)
	{
		MediaFrame *frame = _input_buffer[0].get();

		logtp("Dequeued data for resampling:\n%s", ov::Dump(frame->GetBuffer(0), frame->GetBufferSize(0), 32).CStr());

		// ** 오디오 프레임에 들어갈 필수 항목
		_frame->nb_samples = frame->GetNbSamples();
		_frame->format = frame->GetFormat();
		_frame->channel_layout = static_cast<uint64_t>(frame->GetChannelLayout());
		_frame->channels = frame->GetChannels();
		_frame->sample_rate = frame->GetSampleRate();
		_frame->pts = frame->GetPts();

		// AVFrame 버퍼의 메모리 할당
		if(av_frame_get_buffer(_frame, 0) < 0)
		{
			logte("Could not allocate the audio frame data");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");

			*result = TranscodeResult::DataError;
			return nullptr;
		}

		// Copy data into frame
		if(IsPlanar(frame->GetFormat<AVSampleFormat>()))
		{
			// If the frame is planar, the data should stored separately in the "_frame->data" array.
			_frame->linesize[0] = 0;

			for(int channel = 0; channel < _frame->channels; channel++)
			{
				size_t data_length = frame->GetBufferSize(channel);

				::memcpy(_frame->data[channel], frame->GetBuffer(channel), data_length);
				_frame->linesize[0] += data_length;
			}
		}
		else
		{
			// If the frame is non-planar, Just copy interleaved data to "_frame->data[0]"
			::memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		}

		// Copy packet data into frame
		if(av_buffersrc_add_frame_flags(_buffersrc_ctx, _frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
		{
			logte("Error while feeding the audio filtergraph. frame.format(%d), buffer.pts(%lld) buffer.linesize(%d), buf.size(%d)", _frame->format, _frame->pts, _frame->linesize[0], _input_buffer.size());
		}

		// 처리가 완료된 패킷은 큐에서 삭제함.
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		av_frame_unref(_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

bool MediaFilterResampler::IsPlanar(AVSampleFormat format)
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
