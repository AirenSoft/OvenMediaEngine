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
}


int32_t MediaFilterResampler::Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context)
{
	int ret;
	const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");
	const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");

	_filter_graph = avfilter_graph_alloc();
	if(!_outputs || !_inputs || !_filter_graph)
	{
		logte("cannot allocated filter graph");
		return 1;
	}

	ov::String input_formats;
	input_formats.Format("time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%x", input_media_track->GetTimeBase().GetNum()            // Timebase 1
		, input_media_track->GetTimeBase().GetDen()            // Timebase 1000
		, input_media_track->GetSampleRate()                    // SampleRate
		, input_media_track->GetSample().GetName()            // SampleFormat
		, input_media_track->GetChannel().GetLayout());        // SampleLayout

	ret = avfilter_graph_create_filter(&_buffersrc_ctx, abuffersrc, "in", input_formats.CStr(), NULL, _filter_graph);
	if(ret < 0)
	{
		logte("Cannot create buffer source\n");
		return 1;
	}

	ov::String output_filter_descr;
	output_filter_descr.Format("aresample=%d,aformat=sample_fmts=%s:channel_layouts=%s,asettb=expr=%f", context->GetAudioSampleRate(), context->_audio_sample.GetName(), context->_audio_channel.GetName(), (float)context->GetVideoTimeBase().GetExpr()
	);

	logtd("resample. track[%u] %s -> %s", input_media_track->GetId(), input_formats.CStr(), output_filter_descr.CStr());


	static const enum AVSampleFormat out_sample_fmts[] = {
		(AVSampleFormat)context->_audio_sample.GetFormat(),
		(AVSampleFormat)-1
	};
	static const int64_t out_channel_layouts[] = {
		(int64_t)context->_audio_channel.GetLayout(),
		-1
	};
	static const int out_sample_rates[] = {
		context->GetAudioSampleRate(),
		-1
	};

	// logtd("out_sample_fmts : %d", context->_audio_sample.GetFormat());
	// logtd("out_channel_layouts : %x", context->_audio_channel.GetLayout());
	// logtd("out_sample_rates : %d", context->GetAudioSampleRate());

	ret = avfilter_graph_create_filter(&_buffersink_ctx, abuffersink, "out", NULL, NULL, _filter_graph);
	if(ret < 0)
	{
		logte("Cannot create audio buffer sink\n");
		return 1;
	}

	ret = av_opt_set_int_list(_buffersink_ctx, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN);
	if(ret < 0)
	{
		logte("Cannot set output sample format\n");
		return 1;
	}

	ret = av_opt_set_int_list(_buffersink_ctx, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
	if(ret < 0)
	{
		logte("Cannot set output channel layout\n");
		return 1;
	}

	ret = av_opt_set_int_list(_buffersink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
	if(ret < 0)
	{
		logte("Cannot set output sample rate\n");
		return 1;
	}

	// 필터 연결
	_outputs->name = av_strdup("in");
	_outputs->filter_ctx = _buffersrc_ctx;
	_outputs->pad_idx = 0;
	_outputs->next = NULL;


	// 버퍼 싱크의 임력 설정
	_inputs->name = av_strdup("out");
	_inputs->filter_ctx = _buffersink_ctx;
	_inputs->pad_idx = 0;
	_inputs->next = NULL;


	if((ret = avfilter_graph_parse_ptr(_filter_graph, output_filter_descr.CStr(), &_inputs, &_outputs, NULL)) < 0)
	{
		logte("Cannot create filter graph\n");
		return 1;
	}

	if((ret = avfilter_graph_config(_filter_graph, NULL)) < 0)
	{
		logte("Cannot validation filter graph\n");
		return 1;
	}

	return 0;
}

int32_t MediaFilterResampler::SendBuffer(std::unique_ptr<MediaBuffer> buf)
{

	_pkt_buf.push_back(std::move(buf));

	return 0;
}

std::pair<int32_t, std::unique_ptr<MediaBuffer>> MediaFilterResampler::RecvBuffer()
{
	int ret = av_buffersink_get_frame(_buffersink_ctx, _frame);
	if(ret == AVERROR(EAGAIN))
	{
		// printf("eagain : %d\r\n", ret);
	}
	else if(ret == AVERROR_EOF)
	{
		logte("eof : %d\r\n", ret);
		return std::make_pair(-1, nullptr);
	}
	else if(ret < 0)
	{
		logte("error : %d\r\n", ret);
		return std::make_pair(-1, nullptr);
	}
	else
	{
		auto out_buf = std::make_unique<MediaBuffer>();

		out_buf->SetFormat(_frame->format);
		out_buf->SetBytesPerSample(av_get_bytes_per_sample((AVSampleFormat)_frame->format));
		out_buf->SetNbSamples(_frame->nb_samples);
		out_buf->SetChannels(_frame->channels);
		out_buf->SetSampleRate(_frame->sample_rate);
		out_buf->SetChannelLayout((MediaCommonType::AudioChannel::Layout)_frame->channel_layout);

		out_buf->SetPts((_frame->pts == AV_NOPTS_VALUE) ? -1.0f : _frame->pts);

		uint32_t data_length = (uint32_t)(out_buf->GetBytesPerSample() * out_buf->GetNbSamples() * out_buf->GetChannels());

		// 메모리를 미리 할당함
		out_buf->Resize(data_length);

		uint8_t *buf_ptr = out_buf->GetBuffer();

		for(int i = 0; i < out_buf->GetNbSamples(); i++)
		{
			for(int ch = 0; ch < out_buf->GetChannels(); ch++)
			{
				memcpy(buf_ptr + ch + out_buf->GetBytesPerSample() * i, _frame->data[ch] + out_buf->GetBytesPerSample() * i, static_cast<size_t>(out_buf->GetBytesPerSample()));
			}
		}

		av_frame_unref(_frame);

		// Notify가 필요한 경우에 1을 반환, 아닌 경우에는 일반적인 경우로 0을 반환
		return std::make_pair(0, std::move(out_buf));
	}


	while(_pkt_buf.size() > 0)
	{
		MediaBuffer *cur_pkt = _pkt_buf[0].get();

		// ** 오디오 프레임에 들어갈 필수 항목
		_frame->nb_samples = cur_pkt->GetNbSamples();
		_frame->format = cur_pkt->GetFormat();
		_frame->channel_layout = static_cast<uint64_t>(cur_pkt->GetChannelLayout());
		_frame->channels = cur_pkt->GetChannels();
		_frame->sample_rate = cur_pkt->GetSampleRate();
		_frame->pts = cur_pkt->GetPts();

#if 0
		logtd("frame.nb_samples(%d)", _frame->nb_samples);
		logtd("frame.format(%d)", _frame->format);
		logtd("frame.channel_layout(%d)", _frame->channel_layout);
		logtd("frame.channels(%d)", _frame->channels);
		logtd("frame.sample_rate(%d)", _frame->sample_rate);
#endif

		// AVFrame 버퍼의 메모리 할당
		if(av_frame_get_buffer(_frame, 0) < 0)
		{
			logte("Could not allocate the audio frame data\n");

			return std::make_pair(-1, nullptr);
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable\n");

			return std::make_pair(-1, nullptr);
		}

		if(av_buffersrc_add_frame_flags(_buffersrc_ctx, _frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
		{
			logte("Error while feeding the audio filtergraph. frame.format(%d), buffer.pts(%.0f) buffer.linesize(%d), buf.size(%d)\n", _frame->format, (double)_frame->pts, _frame->linesize[0], _pkt_buf.size());
		}

		// 처리가 완료된 패킷은 큐에서 삭제함.
		_pkt_buf.erase(_pkt_buf.begin(), _pkt_buf.begin() + 1);

		av_frame_unref(_frame);
	}

	return std::make_pair(-1, nullptr);
}