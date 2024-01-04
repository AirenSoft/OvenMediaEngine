//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "filter_rescaler.h"

#include <base/ovlibrary/ovlibrary.h>

#include "../transcoder_gpu.h"
#include "../transcoder_private.h"

#define MAX_QUEUE_SIZE 300
#define FILTER_FLAG_HWFRAME_AWARE  (1 << 0)

FilterRescaler::FilterRescaler()
{
	_frame = ::av_frame_alloc();

	_inputs = ::avfilter_inout_alloc();
	_outputs = ::avfilter_inout_alloc();
	
	_buffersrc = ::avfilter_get_by_name("buffer");
	_buffersink = ::avfilter_get_by_name("buffersink");

	_filter_graph = ::avfilter_graph_alloc();

	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);

	OV_ASSERT2(_frame != nullptr);
	OV_ASSERT2(_inputs != nullptr);
	OV_ASSERT2(_outputs != nullptr);
	OV_ASSERT2(_buffersrc != nullptr);
	OV_ASSERT2(_buffersink != nullptr);
	OV_ASSERT2(_filter_graph != nullptr);

	// Limit the number of filter threads to 4. I think 4 thread is usually enough for video filtering processing.
	_filter_graph->nb_threads = 4;
}

FilterRescaler::~FilterRescaler()
{
	Stop();

	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);
	OV_SAFE_FUNC(_inputs, nullptr, ::avfilter_inout_free, &);
	OV_SAFE_FUNC(_outputs, nullptr, ::avfilter_inout_free, &);
	OV_SAFE_FUNC(_filter_graph, nullptr, ::avfilter_graph_free, &);

	_buffersrc= nullptr;
	_buffersink = nullptr;
	
	_input_buffer.Clear();
}

bool FilterRescaler::InitializeSourceFilter()
{
	std::vector<ov::String> src_params;

	src_params.push_back(ov::String::FormatString("video_size=%dx%d", _input_track->GetWidth(), _input_track->GetHeight()));
	src_params.push_back(ov::String::FormatString("pix_fmt=%s", ::av_get_pix_fmt_name((AVPixelFormat)_src_pixfmt)));
	src_params.push_back(ov::String::FormatString("time_base=%s", _input_track->GetTimeBase().GetStringExpr().CStr()));
	src_params.push_back(ov::String::FormatString("pixel_aspect=%d/%d", 1, 1));
	src_params.push_back(ov::String::FormatString("frame_rate=%d/%d", 30, 1));

	_src_args = ov::String::Join(src_params, ":");


	int ret = ::avfilter_graph_create_filter(&_buffersrc_ctx, _buffersrc, "in", _src_args, nullptr, _filter_graph);
	if (ret < 0)
	{
		logte("Could not create video buffer source filter for rescaling: %d", ret);
		return false;
	}

	_outputs->name = ::av_strdup("in");
	_outputs->filter_ctx = _buffersrc_ctx;
	_outputs->pad_idx = 0;
	_outputs->next = nullptr;

	return true;
}

bool FilterRescaler::InitializeSinkFilter()
{
	int ret = ::avfilter_graph_create_filter(&_buffersink_ctx, _buffersink, "out", nullptr, nullptr, _filter_graph);
	if (ret < 0)
	{
		logte("Could not create video buffer sink filter for rescaling: %d", ret);
		return false;
	}

	_inputs->name = ::av_strdup("out");
	_inputs->filter_ctx = _buffersink_ctx;
	_inputs->pad_idx = 0;
	_inputs->next = nullptr;

	return true;
}

bool FilterRescaler::InitializeFilterDescription()
{
	std::vector<ov::String> filters;

	// 1. Framerate
	if (_output_track->GetFrameRateByConfig() > 0.0f)
	{
		filters.push_back(ov::String::FormatString("fps=fps=%.2f:round=near", _output_track->GetFrameRateByConfig()));
	}

	// 2. Timebase
	filters.push_back(ov::String::FormatString("settb=%s", _output_track->GetTimeBase().GetStringExpr().CStr()));

	// 3. Scaler
	auto input_module_id = _input_track->GetCodecModuleId();
	auto input_device_id = _input_track->GetCodecDeviceId();
	auto output_module_id = _output_track->GetCodecModuleId();
	auto output_device_id = _output_track->GetCodecDeviceId();
	
	// Scaler is performed on the same device as the encoder(output module)
	ov::String desc = "";

	if (output_module_id == cmn::MediaCodecModuleId::DEFAULT ||
		output_module_id == cmn::MediaCodecModuleId::BEAMR ||
		output_module_id == cmn::MediaCodecModuleId::QSV ||
		output_module_id == cmn::MediaCodecModuleId::LIBVPX ||
		// Until now, Logan VPU processes in CPU memory like SW-based modules. Performance needs to be improved in the future
		output_module_id == cmn::MediaCodecModuleId::NILOGAN  
		/* || output_module_id == cmn::MediaCodecModuleId::libx26x */)
	{
		switch (input_module_id)
		{
			case cmn::MediaCodecModuleId::NVENC:  
			{
				// Use the av_hwframe_transfer_data function to enable copying from GPU memory to CPU memory.
				_use_hwframe_transfer = true;

				// Change the pixel format of the source filter to the SW pixel format supported by the hardware.
				auto hw_device_ctx = TranscodeGPU::GetInstance()->GetDeviceContext(input_module_id, input_device_id);
				if (hw_device_ctx == nullptr)
				{
					logte("Could not get hw device context for %s(%d)", cmn::GetStringFromCodecModuleId(input_module_id).CStr(), input_device_id);
					return false;
				}
				auto constraints = av_hwdevice_get_hwframe_constraints(hw_device_ctx, nullptr);
				_src_pixfmt = *(constraints->valid_sw_formats);
				desc = ov::String::FormatString("");
			}
			break;
			case cmn::MediaCodecModuleId::XMA: 
			{
				// Copy the frames in Xilinx Device memory to the CPU memory using the xvbm_convert filter.
				desc = ov::String::FormatString("xvbm_convert,");
			}
			break;
			default: 
				logtw("Unsupported input module: %s", cmn::GetStringFromCodecModuleId(input_module_id).CStr());
			case cmn::MediaCodecModuleId::QSV:  	// CPU memory using 'gpu_copy=on'
			case cmn::MediaCodecModuleId::NILOGAN: 	// CPU memory using 'out=sw'
			case cmn::MediaCodecModuleId::DEFAULT:  // CPU memory 
			{
				desc = ov::String::FormatString("");
			}
		}
		// Scaler description of defulat module
		desc += ov::String::FormatString("scale=%dx%d:flags=bilinear", _output_track->GetWidth(), _output_track->GetHeight());
	}
	else if (output_module_id == cmn::MediaCodecModuleId::NVENC)
	{
		switch (input_module_id)
		{
			case cmn::MediaCodecModuleId::NVENC: { 	// Zero Copy
				//TODO: Exception handling required if Device ID is different. Find out if memory copy between GPUs is possible
				desc = ov::String::FormatString("");
			}
			break;
			case cmn::MediaCodecModuleId::XMA: {
				desc = ov::String::FormatString("xvbm_convert,hwupload_cuda=device=%d,",output_device_id);
			}
			break;
			default: 
				logtw("Unsupported input module: %s", cmn::GetStringFromCodecModuleId(input_module_id).CStr());
			case cmn::MediaCodecModuleId::QSV: 		// CPU memory using 'gpu_copy=on'
			case cmn::MediaCodecModuleId::NILOGAN: 	// CPU memory using 'out=sw'
			case cmn::MediaCodecModuleId::DEFAULT: 	// CPU memory 
			{
				desc = ov::String::FormatString("hwupload_cuda=device=%d,", output_device_id);
			}
		}
		desc += ov::String::FormatString("scale_npp=%d:%d", _output_track->GetWidth(), _output_track->GetHeight());
		// TODO: scale_npp filter requires hardware device/frames context 
	}
	else if (output_module_id == cmn::MediaCodecModuleId::XMA)
	{
		ov::String scaler = "";

		switch (input_module_id)
		{
			case cmn::MediaCodecModuleId::XMA: {  // Zero Copy
				//TODO: Exception handling required if Device ID is different. Find out if memory copy between GPUs is possible
				desc = ov::String::FormatString("");
			}
			break;
			case cmn::MediaCodecModuleId::NVENC: {
				// Use the av_hwframe_transfer_data function to enable copying from GPU memory to CPU memory.
				_use_hwframe_transfer = true;

				// Change the pixel format of the source filter to the SW pixel format supported by the hardware.
				auto hw_device_ctx = TranscodeGPU::GetInstance()->GetDeviceContext(input_module_id, input_device_id);
				if (hw_device_ctx == nullptr)
				{
					logte("Could not get hw device context for %s(%d)", cmn::GetStringFromCodecModuleId(input_module_id).CStr(), input_device_id);
					return false;
				}
				auto constraints = av_hwdevice_get_hwframe_constraints(hw_device_ctx, nullptr);
				_src_pixfmt = *(constraints->valid_sw_formats);
				desc = ov::String::FormatString("xvbm_convert,");
			}
			break;
			default:
				logtw("Unsupported input module: %s", cmn::GetStringFromCodecModuleId(input_module_id).CStr());
			case cmn::MediaCodecModuleId::QSV:		// CPU memory using 'gpu_copy=on'
			case cmn::MediaCodecModuleId::NILOGAN:	// CPU memory using 'out=sw'
			case cmn::MediaCodecModuleId::DEFAULT:	// CPU memory
			{
				desc = ov::String::FormatString("xvbm_convert,");
			}
		}

		desc += ov::String::FormatString("multiscale_xma=lxlnx_hwdev=%d:outputs=1:out_1_width=%d:out_1_height=%d:out_1_rate=full",
			_output_track->GetCodecDeviceId(), _output_track->GetWidth(), _output_track->GetHeight());
	}
	else
	{
		logtw("Unsupported output module id: %d", output_module_id);
		return false;
	}

	filters.push_back(desc);

	// 4. Pixel Format
	filters.push_back(ov::String::FormatString("format=%s", ::av_get_pix_fmt_name((AVPixelFormat)_output_track->GetColorspace())));

	if(filters.size() == 0)
	{
		filters.push_back("null");
	}

	_filter_desc = ov::String::Join(filters, ",");

	return true;
}

bool FilterRescaler::Configure(const std::shared_ptr<MediaTrack> &input_track, const std::shared_ptr<MediaTrack> &output_track)
{
	SetState(State::CREATED);

	_input_track = input_track;
	_output_track = output_track;
	_src_width = _input_track->GetWidth();
	_src_height = _input_track->GetHeight();
	_src_pixfmt = _input_track->GetColorspace();

	if (InitializeFilterDescription() == false)
	{
		SetState(State::ERROR);
		
		return false;
	}

	if (InitializeSourceFilter() == false)
	{
		SetState(State::ERROR);
		
		return false;
	}

	if (InitializeSinkFilter() == false)
	{
		SetState(State::ERROR);

		return false;
	}

	logti("Rescaler parameters. track(#%u -> #%u), module(%s:%d -> %s:%d). desc:(src:%s -> output:%s)",
		  _input_track->GetId(),
		  _output_track->GetId(),
		  GetStringFromCodecModuleId(_input_track->GetCodecModuleId()).CStr(),
		  _input_track->GetCodecDeviceId(),
		  GetStringFromCodecModuleId(_output_track->GetCodecModuleId()).CStr(),
		  _output_track->GetCodecDeviceId(),
		  _src_args.CStr(),
		  _filter_desc.CStr());


	if ((::avfilter_graph_parse_ptr(_filter_graph, _filter_desc, &_inputs, &_outputs, nullptr)) < 0)
	{
		logte("Could not parse filter string for rescaling: %s", _filter_desc.CStr());
		SetState(State::ERROR);
		
		return false;
	}

	if (SetHWContextToFilterIfNeed() == false)
	{
		logte("Could not set hw context to filters");
		SetState(State::ERROR);

		return false;
	}

	if (::avfilter_graph_config(_filter_graph, nullptr) < 0)
	{
		logte("Could not validate filter graph for rescaling");
		SetState(State::ERROR);

		return false;
	}

	return true;
}

bool FilterRescaler::Start()
{
	// Generates a thread that reads and encodes frames in the input_buffer queue and places them in the output queue.
	try
	{
		_kill_flag = false;

		_thread_work = std::thread(&FilterRescaler::WorkerThread, this);
		pthread_setname_np(_thread_work.native_handle(), "Rescaler");
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;
		SetState(State::ERROR);

		logte("Failed to start rescaling filter thread");

		return false;
	}

	return true;
}

void FilterRescaler::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();

	if (_thread_work.joinable())
	{
		_thread_work.join();
	}

	SetState(State::STOPPED);
}

void FilterRescaler::WorkerThread()
{
	logtd("Start rescaling filter thread");
	int ret;


	SetState(State::STARTED);

	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
		{
			continue;
		}

		auto media_frame = std::move(obj.value());

		auto av_frame = ffmpeg::Conv::ToAVFrame(cmn::MediaType::Video, media_frame);
		if (!av_frame)
		{
			logte("Could not allocate the video frame data");

			SetState(State::ERROR);

			break;
		}

		AVFrame *alter_av_frame = nullptr;

		if (_use_hwframe_transfer == true && av_frame->hw_frames_ctx != nullptr)
		{
			// GPU Memory -> Host Memory
			alter_av_frame = ::av_frame_alloc();
			if ((ret = ::av_hwframe_transfer_data(alter_av_frame, av_frame, 0)) < 0)
			{
				logte("Error transferring the data to system memory\n");
				continue;
			}
			alter_av_frame->pts = av_frame->pts;
		}

		// Send to filtergraph
		ret = ::av_buffersrc_write_frame(_buffersrc_ctx, alter_av_frame != nullptr ? alter_av_frame : av_frame);
		if (ret < 0)
		{
			logte("An error occurred while feeding to filtergraph: format: %d, pts: %lld, linesize: %d, queue.size: %d", av_frame->format, av_frame->pts, av_frame->linesize[0], _input_buffer.Size());

			continue;
		}

		if(alter_av_frame != nullptr)
		{
			av_frame_free(&alter_av_frame);
		}

		while (!_kill_flag)
		{
			// Receive from filtergraph
			ret = ::av_buffersink_get_frame(_buffersink_ctx, _frame);
			if (ret == AVERROR(EAGAIN))
			{
				break;
			}
			else if (ret == AVERROR_EOF)
			{
				logte("Error receiving filtered frame. error(EOF)");

				SetState(State::ERROR);

				break;
			}
			else if (ret < 0)
			{
				logte("Error receiving filtered frame. error(%d)", ret);

				SetState(State::ERROR);

				break;
			}
			else
			{
				_frame->pict_type = AV_PICTURE_TYPE_NONE;
				auto output_frame = ffmpeg::Conv::ToMediaFrame(cmn::MediaType::Video, _frame);
				::av_frame_unref(_frame);
				if (output_frame == nullptr)
				{
					continue;
				}

				if (_complete_handler != nullptr && _kill_flag == false)
				{
					_complete_handler(std::move(output_frame));
				}
			}
		}
	}
}

bool FilterRescaler::SetHWContextToFilterIfNeed()
{
	auto hw_device_ctx =  TranscodeGPU::GetInstance()->GetDeviceContext(cmn::MediaCodecModuleId::NVENC, _output_track->GetCodecDeviceId());

	// Assign the hw device context and hw frames context to the filter graph
	for (uint32_t i = 0 ; i < _filter_graph->nb_filters ; i++)
	{
		auto filter = _filter_graph->filters[i];

		if( (filter->filter->flags_internal & FILTER_FLAG_HWFRAME_AWARE) == 0 || (filter->inputs == nullptr))
		{
			continue;
		}

		bool matched = false;
		if( strstr(filter->name, "scale_cuda") != nullptr || strstr(filter->name, "scale_npp") != nullptr)
		{
			matched = true;
		}

		if (matched == true)
		{
			logtd("set hardware device/frames context to filter. name(%s)", filter->name);

			if (ffmpeg::Conv::SetHwDeviceCtxOfAVFilterContext(filter, hw_device_ctx) == false)
			{
				logte("Could not set hw device context for %s", filter->name);
				return false;
			}

			for (uint32_t j = 0; j < filter->nb_inputs; j++)
			{
				auto input = filter->inputs[j];
				if (input == nullptr)
				{
					continue;
				}

				if (ffmpeg::Conv::SetHWFramesCtxOfAVFilterLink(input, hw_device_ctx, _output_track->GetWidth(), _output_track->GetHeight()) == false)
				{
					logte("Could not set hw frames context for %s", filter->name);
					return false;
				}
			}
		}
	}

	return true;
}
