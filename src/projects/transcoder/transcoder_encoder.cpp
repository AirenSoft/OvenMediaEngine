//==============================================================================
//
//  Transcoder
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_encoder.h"

#include <utility>

#include "codec/encoder/encoder_aac.h"
#include "codec/encoder/encoder_avc_nv.h"
#include "codec/encoder/encoder_avc_openh264.h"
#include "codec/encoder/encoder_avc_qsv.h"
#include "codec/encoder/encoder_avc_nilogan.h"
#include "codec/encoder/encoder_avc_xma.h"
#include "codec/encoder/encoder_ffopus.h"
#include "codec/encoder/encoder_hevc_nv.h"
#include "codec/encoder/encoder_hevc_qsv.h"
#include "codec/encoder/encoder_hevc_nilogan.h"
#include "codec/encoder/encoder_hevc_xma.h"
#include "codec/encoder/encoder_jpeg.h"
#include "codec/encoder/encoder_opus.h"
#include "codec/encoder/encoder_png.h"
#include "codec/encoder/encoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define USE_LEGACY_LIBOPUS false
#define MAX_QUEUE_SIZE 5
#define ALL_GPU_ID -1
#define DEFAULT_MODULE_NAME "DEFAULT"


std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> TranscodeEncoder::GetCandidates(bool hwaccels_enable, ov::String hwaccles_modules, std::shared_ptr<MediaTrack> track)
{
	logtd("Codec(%s), HWAccels.Enable(%s), HWAccels.Modules(%s), Video.Modules(%s)",
		  GetCodecIdToString(track->GetCodecId()).CStr(),
		  hwaccels_enable ? "true" : "false",
		  hwaccles_modules.CStr(),
		  track->GetCodecModules().CStr());

	ov::String configuration = ""; 
	std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> candidate_modules = std::make_shared<std::vector<std::shared_ptr<CodecCandidate>>>();

	// If the track is not video, the default module is the only candidate.
	if (cmn::IsVideoCodec(track->GetCodecId()) == false)
	{
		candidate_modules->push_back(std::make_shared<CodecCandidate>(track->GetCodecId(), cmn::MediaCodecModuleId::DEFAULT, 0));
		return candidate_modules;
	}

	if(hwaccels_enable == true)
	{
		if(track->GetCodecModules().Trim().IsEmpty() == false)
		{
			configuration = track->GetCodecModules().Trim();
		}
		else
		{
			configuration = hwaccles_modules.Trim();
		}
	}
	else
	{
		configuration = track->GetCodecModules().Trim();
	}

	// ex) hwaccels_modules = "XMA:0,NV:0,QSV:0"
	std::vector<ov::String> desire_modules = configuration.Split(",");

	// If no modules are configured, all modules are designated as candidates.
	if (desire_modules.size() == 0 || configuration.IsEmpty() == true)
	{
		desire_modules.clear();
		
		if (hwaccels_enable == true)
		{
			desire_modules.push_back(ov::String::FormatString("%s:%d", "XMA", ALL_GPU_ID));
			desire_modules.push_back(ov::String::FormatString("%s:%d", "NV", ALL_GPU_ID));
			desire_modules.push_back(ov::String::FormatString("%s:%d", "QSV", ALL_GPU_ID));
			desire_modules.push_back(ov::String::FormatString("%s:%d", "NILOGAN", ALL_GPU_ID));
		}

		desire_modules.push_back(ov::String::FormatString("%s:%d", DEFAULT_MODULE_NAME, ALL_GPU_ID));
	}

	for(auto &desire_module : desire_modules)
	{
		// Pattern : <module_name>:<gpu_id> or <module_name>
		ov::Regex pattern_regex = ov::Regex::CompiledRegex( "(?<module_name>[^,:\\s]+[\\w]+):?(?<gpu_id>[^,]*)");

		auto matches = pattern_regex.Matches(desire_module.CStr());
		if (matches.GetError() != nullptr || matches.IsMatched() == false)
		{
			logtw("Incorrect pattern in the Modules item. module(%s)", desire_module.CStr());
			continue;;
		}

		auto named_group = matches.GetNamedGroupList();

		auto module_name 	= named_group["module_name"].GetValue();
		auto gpu_id 		= named_group["gpu_id"].GetValue().IsEmpty()?ALL_GPU_ID:ov::Converter::ToInt32(named_group["gpu_id"].GetValue());

		// Module Name(Codec Library Name) to Codec Library ID
		cmn::MediaCodecModuleId module_id = cmn::GetCodecModuleIdByName(module_name);
		if(module_id == cmn::MediaCodecModuleId::None)
		{
			logtw("Unknown codec module. name(%s)", module_name.CStr());
			continue;
		}

		if(cmn::IsSupportHWAccels(module_id) == true && hwaccels_enable == false)
		{
			logtw("HWAccels is not enabled. Ignore this codec. module(%s)", module_name.CStr());
			continue;
		}
		else  if(cmn::IsSupportHWAccels(module_id) == true && hwaccels_enable == true)
		{
			for (int i = 0; i < TranscodeGPU::GetInstance()->GetDeviceCount(module_id); i++)
			{
				if ((gpu_id == ALL_GPU_ID || gpu_id == i) && TranscodeGPU::GetInstance()->IsSupported(module_id, i) == true)
				{
					candidate_modules->push_back(std::make_shared<CodecCandidate>(track->GetCodecId(), module_id, i));
				}
			}
		}
		else
		{
			candidate_modules->push_back(std::make_shared<CodecCandidate>(track->GetCodecId(), module_id, 0));
		}
	}


	for (auto &candidate : *candidate_modules)
	{
		(void)(candidate);
		
		logtd("Candidate module: %s(%d), %s(%d):%d",
			  cmn::GetCodecIdToString(candidate->GetCodecId()).CStr(),
			  candidate->GetCodecId(),
			  cmn::GetStringFromCodecModuleId(candidate->GetModuleId()).CStr(),
			  candidate->GetModuleId(),
			  candidate->GetDeviceId());
	}

	return candidate_modules;
}

#define CASE_CREATE_CODEC_IFNEED(MODULE_ID, CLS) \
	case cmn::MediaCodecModuleId::MODULE_ID: \
		encoder = std::make_shared<CLS>(info); \
		if (encoder == nullptr) \
		{ \
			break; \
		} \
		track->SetCodecDeviceId(candidate->GetDeviceId()); \
		if (encoder->Configure(track) == true) \
		{ \
			goto done; \
		} \
		if (encoder != nullptr) { encoder->Stop(); encoder = nullptr; } \
		break; \

std::shared_ptr<TranscodeEncoder> TranscodeEncoder::Create(
	int32_t encoder_id,
	const info::Stream &info,
	std::shared_ptr<MediaTrack> track,
	std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> candidates,
	CompleteHandler complete_handler)
{
	std::shared_ptr<TranscodeEncoder> encoder = nullptr;
	std::shared_ptr<CodecCandidate> cur_candidate = nullptr;

	for (auto &candidate : *candidates)
	{
		cur_candidate = candidate;

		if (candidate->GetCodecId() == cmn::MediaCodecId::H264)
		{
			switch (candidate->GetModuleId())
			{
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderAVCxOpenH264);
				CASE_CREATE_CODEC_IFNEED(OPENH264, EncoderAVCxOpenH264);
				CASE_CREATE_CODEC_IFNEED(QSV, EncoderAVCxQSV);
				CASE_CREATE_CODEC_IFNEED(NILOGAN, EncoderAVCxNILOGAN);
				CASE_CREATE_CODEC_IFNEED(XMA, EncoderAVCxXMA);
				CASE_CREATE_CODEC_IFNEED(NVENC, EncoderAVCxNV);
				default:
					break;
			}
			break;
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::H265)
		{
			switch (candidate->GetModuleId())
			{
				CASE_CREATE_CODEC_IFNEED(QSV, EncoderHEVCxQSV);
				CASE_CREATE_CODEC_IFNEED(NILOGAN, EncoderHEVCxNILOGAN);
				CASE_CREATE_CODEC_IFNEED(XMA, EncoderHEVCxXMA);
				CASE_CREATE_CODEC_IFNEED(NVENC, EncoderHEVCxNV);
				default:
					break;
			};
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Vp8)
		{
			switch (candidate->GetModuleId())
			{
				default:
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderVP8);
				CASE_CREATE_CODEC_IFNEED(LIBVPX, EncoderVP8);

					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			switch (candidate->GetModuleId())
			{
				default:
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderAAC);
				CASE_CREATE_CODEC_IFNEED(FDKAAC, EncoderAAC);
					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Opus)
		{
			switch (candidate->GetModuleId())
			{
#if USE_LEGACY_LIBOPUS
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderOPUS);
#else
				default:
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderFFOPUS);
				CASE_CREATE_CODEC_IFNEED(LIBOPUS, EncoderFFOPUS);
#endif

					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Jpeg)
		{
			switch (candidate->GetModuleId())
			{
				default:
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderJPEG);

					break;
			}
			break;
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Png)
		{
			switch (candidate->GetModuleId())
			{
				default:
				CASE_CREATE_CODEC_IFNEED(DEFAULT, EncoderPNG);
					break;
			}
			break;
		}
		else
		{
			OV_ASSERT(false, "Not supported codec: %d", track->GetCodecId());
		}

		// If the decoder is not created, try the next candidate.
		encoder = nullptr;
	}

done:
	if (encoder)
	{
		track->SetCodecModuleId(cur_candidate->GetModuleId());
		
		encoder->SetEncoderId(encoder_id);
		encoder->SetCompleteHandler(complete_handler);

		logti("The encoder has been created successfully. track(#%d), codec(%s), module(%s:%d)",
			track->GetId(),
			cmn::GetCodecIdToString(track->GetCodecId()).CStr(),
			cmn::GetStringFromCodecModuleId(track->GetCodecModuleId()).CStr(),
			track->GetCodecDeviceId());		
	}

	return encoder;
}

TranscodeEncoder::TranscodeEncoder(info::Stream stream_info) : 
	_stream_info(stream_info)
{
	_packet = ::av_packet_alloc();
	_frame = ::av_frame_alloc();
	_codec_par = ::avcodec_parameters_alloc();
}

TranscodeEncoder::~TranscodeEncoder()
{
	Stop();

	if (_codec_context != nullptr && _codec_context->codec != nullptr)
	{
		if (_codec_context->codec->capabilities & AV_CODEC_CAP_ENCODER_FLUSH)
		{
			::avcodec_flush_buffers(_codec_context);
		}
	}

	if (_codec_context != nullptr)
	{
		OV_SAFE_FUNC(_codec_context, nullptr, ::avcodec_free_context, &);
	}
	
	OV_SAFE_FUNC(_frame, nullptr, ::av_frame_free, &);
	OV_SAFE_FUNC(_packet, nullptr, ::av_packet_free, &);
	OV_SAFE_FUNC(_codec_par, nullptr, ::avcodec_parameters_free, &);

	_input_buffer.Clear();
}


cmn::Timebase TranscodeEncoder::GetTimebase() const
{
	return _track->GetTimeBase();
}

void TranscodeEncoder::SetEncoderId(int32_t encoder_id)
{
	_encoder_id = encoder_id;
}

bool TranscodeEncoder::Configure(std::shared_ptr<MediaTrack> output_track)
{
	_track = output_track;	
	_track->SetOriginBitstream(GetBitstreamFormat());

	auto name = ov::String::FormatString("encoder_%s_%d", ::avcodec_get_name(GetCodecID()), _track->GetId());
	auto urn = std::make_shared<info::ManagedQueue::URN>(
		_stream_info.GetApplicationInfo().GetVHostAppName(),
		_stream_info.GetName(),
		"trs",
		name);
	_input_buffer.SetUrn(urn);
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);

	// This is used to prevent the from creating frames from rescaler/resampler filter. 
	// Because of hardware resource limitations.
	_input_buffer.SetExceedWaitEnable(true);

	// SkipMessage is enabled due to the high possibility of queue overflow due to insufficient video encoding performance.
	// Users will not experience any inconvenience even if the video is intermittently missing.
	// However, it is sensitive when the audio cuts out.
	// if(_track->GetMediaType() == cmn::MediaType::Video)
	// {
	// 	_input_buffer.SetSkipMessageEnable(true);
	// }

	return (_track != nullptr);
}

std::shared_ptr<MediaTrack> &TranscodeEncoder::GetRefTrack()
{
	return _track;
}

void TranscodeEncoder::SendBuffer(std::shared_ptr<const MediaFrame> frame)
{
	if (_input_buffer.IsExceedWaitEnable() == true)
	{
		_input_buffer.Enqueue(std::move(frame), false, 1000);
	}
	else
	{
		_input_buffer.Enqueue(std::move(frame));
	}
}

void TranscodeEncoder::SetCompleteHandler(CompleteHandler complete_handler)
{
	_complete_handler = move(complete_handler);
}

void TranscodeEncoder::Complete(std::shared_ptr<MediaPacket> packet)
{
	if (_complete_handler)
	{
		_complete_handler(_encoder_id, std::move(packet));
	}
}

void TranscodeEncoder::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();

	if (_codec_thread.joinable())
	{
		_codec_thread.join();
		logtd(ov::String::FormatString("encoder %s thread has ended", avcodec_get_name(GetCodecID())).CStr());
	}
}

void TranscodeEncoder::CodecThread()
{
	if ((GetRefTrack()->GetMediaType() == cmn::MediaType::Video) &&
		(GetRefTrack()->GetKeyFrameIntervalTypeByConfig() == cmn::KeyFrameIntervalType::TIME))
	{
		auto timebase_timescale = GetRefTrack()->GetTimeBase().GetTimescale();
		auto key_frame_interval = GetRefTrack()->GetKeyFrameInterval();
		_force_keyframe_by_time_interval = static_cast<int64_t>(timebase_timescale * (double)key_frame_interval / 1000);

		// Insert keyframe in first frame
		_accumulate_frame_duration = -1;
	}

	while (!_kill_flag)
	{
		auto obj = _input_buffer.Dequeue();
		if (obj.has_value() == false)
			continue;

		auto media_frame = std::move(obj.value());

		///////////////////////////////////////////////////
		// Request frame encoding to codec
		///////////////////////////////////////////////////
		auto av_frame = ffmpeg::Conv::ToAVFrame(GetRefTrack()->GetMediaType(), media_frame);
		if (!av_frame)
		{
			logte("Could not allocate the video frame data");
			break;
		}

		// Force inserts keyframes based on accumulated frame duration.
		if (GetRefTrack()->GetMediaType() == cmn::MediaType::Video)
		{
			av_frame->pict_type = AV_PICTURE_TYPE_NONE;
			if (_force_keyframe_by_time_interval > 0)
			{
				if (_accumulate_frame_duration >= _force_keyframe_by_time_interval ||
					_accumulate_frame_duration == -1) // First Frame
				{
					av_frame->pict_type = AV_PICTURE_TYPE_I;
					_accumulate_frame_duration = 0;
				}
				_accumulate_frame_duration += media_frame->GetDuration();
			}
		}

		int ret = ::avcodec_send_frame(_codec_context, av_frame);
		if (ret < 0)
		{
			logte("Error sending a frame for encoding : %d", ret);
		}

		///////////////////////////////////////////////////
		// The encoded packet is taken from the codec.
		///////////////////////////////////////////////////
		while (!_kill_flag)
		{
			// Check frame is available
			int ret = ::avcodec_receive_packet(_codec_context, _packet);
			if (ret == AVERROR(EAGAIN))
			{
				// More packets are needed for encoding.
				break;
			}
			else if (ret == AVERROR_EOF && ret < 0)
			{
				logte("Error receiving a packet for decoding : %d", ret);
				break;
			}
			else
			{
				auto media_packet = ffmpeg::Conv::ToMediaPacket(_packet, GetRefTrack()->GetMediaType(), _bitstream_format, _packet_type);
				if (media_packet == nullptr)
				{
					logte("Could not allocate the media packet");
					break;
				}

				if(GetRefTrack()->GetMediaType() == cmn::MediaType::Audio)
				{
					// TODO : If the pts value are under zero, the dash packetizer does not work.
					if (media_packet->GetPts() < 0)
					{
						continue;
					}
				}

				::av_packet_unref(_packet);

				Complete(std::move(media_packet));
			}
		}
	}
}

