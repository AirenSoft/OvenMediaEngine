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

#define OV_LOG_TAG "MediaFilter.Resampler"

MediaFilterResampler::MediaFilterResampler()
{
	::avfilter_register_all();

	_frame = ::av_frame_alloc();

	_outputs = ::avfilter_inout_alloc();
	_inputs = ::avfilter_inout_alloc();

	OV_ASSERT2(_frame != nullptr);
	OV_ASSERT2(_inputs != nullptr);
	OV_ASSERT2(_outputs != nullptr);
}

MediaFilterResampler::~MediaFilterResampler()
{
	Stop();

	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);

	OV_SAFE_FUNC(_inputs, nullptr, ::avfilter_inout_free, &);
	OV_SAFE_FUNC(_outputs, nullptr, ::avfilter_inout_free, &);

	OV_SAFE_FUNC(_filter_graph, nullptr, ::avfilter_graph_free, &);

	_input_buffer.clear();
	_output_buffer.clear();
}

bool MediaFilterResampler::Configure(const std::shared_ptr<MediaTrack> &input_media_track, const std::shared_ptr<TranscodeContext> &input_context, const std::shared_ptr<TranscodeContext> &output_context)
{
	int ret;

	const AVFilter *abuffersrc = ::avfilter_get_by_name("abuffer");
	const AVFilter *abuffersink = ::avfilter_get_by_name("abuffersink");

	_filter_graph = ::avfilter_graph_alloc();

	if ((_filter_graph == nullptr) || (_inputs == nullptr) || (_outputs == nullptr))
	{
		logte("Could not allocate variables for filter graph: %p, %p, %p", _filter_graph, _inputs, _outputs);
		return false;
	}

	AVRational input_timebase = TimebaseToAVRational(input_context->GetTimeBase());
	AVRational output_timebase = TimebaseToAVRational(output_context->GetTimeBase());

	_scale = ::av_q2d(::av_div_q(input_timebase, output_timebase));

	if (::isnan(_scale))
	{
		logte("Invalid timebase: input: %d/%d, output: %d/%d",
			  input_timebase.num, input_timebase.den,
			  output_timebase.num, output_timebase.den);

		return false;
	}

	// Prepare filters
	//
	// Filter graph:
	//     [abuffer] -> [aresample] -> [asetnsamples] -> [aformat] -> [asettb] -> [abuffersink]

	// Prepare the input filter

	// "abuffer" filter
	ov::String input_args = ov::String::FormatString(
		// "time_base=%s:sample_rate=%d:sample_fmt=%s:channel_layout=0x%x",
		"time_base=%s:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
		input_context->GetTimeBase().GetStringExpr().CStr(),
		input_context->GetAudioSampleRate(),
		input_context->GetAudioSample().GetName(),
		input_context->GetAudioChannel().GetName());
		// input_context->GetAudioChannel().GetLayout().GetName());

	ret = ::avfilter_graph_create_filter(&_buffersrc_ctx, abuffersrc, "in", input_args, nullptr, _filter_graph);
	if (ret < 0)
	{
		logte("Could not create audio buffer source filter for resampling: %d", ret);
		return false;
	}

	// Prepare output filters
	std::vector<ov::String> filters = {
		// "asettb" filter options
		ov::String::FormatString("asettb=%s", output_context->GetTimeBase().GetStringExpr().CStr()),
		// "aresample" filter options
		// Restore Missing Samples
		ov::String::FormatString("aresample=async=1000", output_context->GetAudioSampleRate()),
		// Change the number of samples
		ov::String::FormatString("aresample=%d", output_context->GetAudioSampleRate()),
		// "aformat" filter options
		ov::String::FormatString("aformat=sample_fmts=%s:channel_layouts=%s", output_context->GetAudioSample().GetName(), output_context->GetAudioChannel().GetName()),
		// "asetnsamples" filter options
		ov::String::FormatString("asetnsamples=n=1024")
	};

	ov::String output_filters = ov::String::Join(filters, ",");

	ret = ::avfilter_graph_create_filter(&_buffersink_ctx, abuffersink, "out", nullptr, nullptr, _filter_graph);
	if (ret < 0)
	{
		logte("Could not create audio buffer sink filter for resampling: %d", ret);
		return false;
	}

	_outputs->name = ::av_strdup("in");
	_outputs->filter_ctx = _buffersrc_ctx;
	_outputs->pad_idx = 0;
	_outputs->next = nullptr;

	_inputs->name = ::av_strdup("out");
	_inputs->filter_ctx = _buffersink_ctx;
	_inputs->pad_idx = 0;
	_inputs->next = nullptr;

	if ((_outputs->name == nullptr) || (_inputs->name == nullptr))
	{
		return false;
	}

	// logte("%s", input_args.CStr());
	// logte("%s", output_filters.CStr());

	ret = ::avfilter_graph_parse_ptr(_filter_graph, output_filters, &_inputs, &_outputs, nullptr);
	if (ret < 0)
	{
		logte("Could not parse filter string for resampling: %d (%s)", ret, output_filters.CStr());
		return false;
	}

	ret = ::avfilter_graph_config(_filter_graph, nullptr);
	if (ret < 0)
	{
		logte("Could not validate filter graph for resampling: %d", ret);
		return false;
	}

	logtd("Resampler is enabled for track #%u using parameters. input: %s / outputs: %s", input_media_track->GetId(), input_args.CStr(), output_filters.CStr());

	_input_context = input_context;
	_output_context = output_context;

	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&MediaFilterResampler::ThreadFilter, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;

		logte("Failed to start transcode resample filter thread.");
	}

	return true;
}


void MediaFilterResampler::Stop()
{
	_kill_flag = true;

	_queue_event.Notify();

	if (_thread_work.joinable())
	{
		_thread_work.join();
		logtd("Terminated transcode resample filter thread.");
	}
}


void MediaFilterResampler::ThreadFilter()
{
	logtd("Start transcode resampler filter thread.");

	while (!_kill_flag)
	{
		_queue_event.Wait();

		std::unique_lock<std::mutex> mlock(_mutex);

		if (_input_buffer.empty())
		{
			continue;
		}

		auto frame = std::move(_input_buffer.front());
		_input_buffer.pop_front();

		mlock.unlock();

		// logtd("format(%d), channels(%d), samples(%d)", frame->GetFormat(), frame->GetChannels(), frame->GetNbSamples());
		///logtp("Dequeued data for resampling: %lld\n%s", frame->GetPts(), ov::Dump(frame->GetBuffer(0), frame->GetBufferSize(0), 32).CStr());

		_frame->format = frame->GetFormat();
		_frame->nb_samples = frame->GetNbSamples();
		_frame->channel_layout = static_cast<uint64_t>(frame->GetChannelLayout());
		_frame->channels = frame->GetChannels();
		_frame->sample_rate = frame->GetSampleRate();
		_frame->pts = frame->GetPts();
		_frame->pkt_duration = frame->GetDuration();

		int ret = ::av_frame_get_buffer(_frame, 0);
		if (ret < 0)
		{
			logte("Could not allocate the audio frame data");

			// *result = TranscodeResult::DataError;
			// return nullptr;
			break;
		}

		ret = ::av_frame_make_writable(_frame);
		if (ret < 0)
		{
			logte("Could not make writable frame");

			// *result = TranscodeResult::DataError;
			// return nullptr;
			 break;
		}

		// Copy data into frame
		if (IsPlanar(frame->GetFormat<AVSampleFormat>()))
		{
			// If the frame is planar, the data should stored separately in the "_frame->data" array.
			_frame->linesize[0] = 0;

			for (int channel = 0; channel < _frame->channels; channel++)
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
		if (::av_buffersrc_add_frame_flags(_buffersrc_ctx, _frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
		{
			logte("An error occurred while feeding the audio filtergraph: pts: %lld, linesize: %d, srate: %d, layout: %d, channels: %d, format: %d, rq: %d",  _frame->pts, _frame->linesize[0], _frame->sample_rate, _frame->channel_layout, _frame->channels, _frame->format, _input_buffer.size());
		}

		::av_frame_unref(_frame);


		while (true)
		{
			int ret = ::av_buffersink_get_frame(_buffersink_ctx, _frame);

			if (ret == AVERROR(EAGAIN))
			{
				// Wait for more packet
				break;
			}
			else if (ret == AVERROR_EOF)
			{
				logte("Error receiving a packet for decoding : AVERROR_EOF");
				// *result = TranscodeResult::EndOfFile;
				// return nullptr;
				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				// *result = TranscodeResult::DataError;
				// return nullptr;
			
				break;
			}
			else
			{
				auto output_frame = std::make_shared<MediaFrame>();

				output_frame->SetFormat(_frame->format);
				output_frame->SetBytesPerSample(::av_get_bytes_per_sample((AVSampleFormat)_frame->format));
				output_frame->SetNbSamples(_frame->nb_samples);
				output_frame->SetChannels(_frame->channels);
				output_frame->SetSampleRate(_frame->sample_rate);
				output_frame->SetChannelLayout((cmn::AudioChannel::Layout)_frame->channel_layout);
				output_frame->SetPts((_frame->pts == AV_NOPTS_VALUE) ? -1L : _frame->pts);
				output_frame->SetDuration(_frame->pkt_duration);

				auto data_length = static_cast<uint32_t>(output_frame->GetBytesPerSample() * output_frame->GetNbSamples());

				// Copy frame data into out_buf
				if (IsPlanar(static_cast<AVSampleFormat>(_frame->format)))
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

				//logtp("Resampled data: %lld\n%s", output_frame->GetPts(), ov::Dump(_frame->data[0], _frame->linesize[0], 32).CStr());

				::av_frame_unref(_frame);

				// *result = TranscodeResult::DataReady;
				// return std::move(output_frame);
				std::unique_lock<std::mutex> mlock(_mutex);

				_output_buffer.push_back(std::move(output_frame));
			}
		}
	}
}

int32_t MediaFilterResampler::SendBuffer(std::shared_ptr<MediaFrame> buffer)
{
	std::unique_lock<std::mutex> mlock(_mutex);

	_input_buffer.push_back(std::move(buffer));
	
	mlock.unlock();
	
	_queue_event.Notify();;

	return 0;
}

std::shared_ptr<MediaFrame> MediaFilterResampler::RecvBuffer(TranscodeResult *result)
{
	std::unique_lock<std::mutex> mlock(_mutex);
	if(!_output_buffer.empty())
	{
		*result = TranscodeResult::DataReady;

		auto frame = std::move(_output_buffer.front());
		_output_buffer.pop_front();

		return std::move(frame);
	}

	*result = TranscodeResult::NoData;

	return nullptr;
}

bool MediaFilterResampler::IsPlanar(AVSampleFormat format)
{
	switch (format)
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
