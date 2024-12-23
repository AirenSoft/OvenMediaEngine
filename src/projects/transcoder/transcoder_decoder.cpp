//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_decoder.h"

#include "codec/decoder/decoder_aac.h"
#include "codec/decoder/decoder_avc.h"
#include "codec/decoder/decoder_avc_nilogan.h"
#include "codec/decoder/decoder_avc_nv.h"
#include "codec/decoder/decoder_avc_qsv.h"
#include "codec/decoder/decoder_avc_xma.h"
#include "codec/decoder/decoder_hevc.h"
#include "codec/decoder/decoder_hevc_nilogan.h"
#include "codec/decoder/decoder_hevc_nv.h"
#include "codec/decoder/decoder_hevc_qsv.h"
#include "codec/decoder/decoder_hevc_xma.h"
#include "codec/decoder/decoder_mp3.h"
#include "codec/decoder/decoder_opus.h"
#include "codec/decoder/decoder_vp8.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

#define MAX_QUEUE_SIZE 500
#define ALL_GPU_ID -1
#define DEFAULT_MODULE_NAME "DEFAULT"

std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> TranscodeDecoder::GetCandidates(bool hwaccels_enable, ov::String hwaccles_modules, std::shared_ptr<MediaTrack> track)
{
	logtd("Codec(%s), HWAccels.Enable(%s), HWAccels.Modules(%s)",
		  GetCodecIdToString(track->GetCodecId()).CStr(), hwaccels_enable ? "true" : "false", hwaccles_modules.CStr());

	ov::String configuration = "";
	std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> candidate_modules = std::make_shared<std::vector<std::shared_ptr<CodecCandidate>>>();

	if (hwaccels_enable == true)
	{
		configuration = hwaccles_modules.Trim();
	}
	else
	{
		configuration = "";
	}

	// If the track is not video, the default module is the only candidate.
	if (cmn::IsVideoCodec(track->GetCodecId()) == false)
	{
		candidate_modules->push_back(std::make_shared<CodecCandidate>(track->GetCodecId(), cmn::MediaCodecModuleId::DEFAULT, 0));
		return candidate_modules;
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

	for (auto &desire_module : desire_modules)
	{
		// Pattern : <module_name>:<gpu_id> or <module_name>
		ov::Regex pattern_regex = ov::Regex::CompiledRegex("(?<module_name>[^,:\\s]+[\\w]+):?(?<gpu_id>[^,]*)");

		auto matches = pattern_regex.Matches(desire_module.CStr());
		if (matches.GetError() != nullptr || matches.IsMatched() == false)
		{
			logtw("Incorrect pattern in the Modules item. module(%s)", desire_module.CStr());

			continue;
			;
		}
		auto named_group = matches.GetNamedGroupList();

		auto module_name = named_group["module_name"].GetValue();
		auto gpu_id = named_group["gpu_id"].GetValue().IsEmpty() ? ALL_GPU_ID : ov::Converter::ToInt32(named_group["gpu_id"].GetValue());

		// If Unknown module name, skip.
		cmn::MediaCodecModuleId module_id = cmn::GetCodecModuleIdByName(module_name);
		if (module_id == cmn::MediaCodecModuleId::None)
		{
			logtw("Unknown codec module. name(%s)", module_name.CStr());
			continue;
		}

		// If hardware usage is enabled, check if the module is supported.
		if (hwaccels_enable == true)
		{
			for (int i = 0; i < TranscodeGPU::GetInstance()->GetDeviceCount(module_id); i++)
			{
				if ((gpu_id == ALL_GPU_ID || gpu_id == i) && TranscodeGPU::GetInstance()->IsSupported(module_id, i) == true)
				{
					candidate_modules->push_back(std::make_shared<CodecCandidate>(track->GetCodecId(), module_id, i));
				}
			}
		}

		//
		if (module_id == cmn::MediaCodecModuleId::DEFAULT)
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

#define CASE_CREATE_CODEC_IFNEED(MODULE_ID, CLS)           \
	case cmn::MediaCodecModuleId::MODULE_ID:               \
		decoder = std::make_shared<CLS>(*info);            \
		if (decoder == nullptr)                            \
		{                                                  \
			break;                                         \
		}                                                  \
		track->SetCodecDeviceId(candidate->GetDeviceId()); \
		if (decoder->Configure(track) == true)             \
		{                                                  \
			goto done;                                     \
		}                                                  \
		if (decoder != nullptr)                            \
		{                                                  \
			decoder->Stop();                               \
			decoder = nullptr;                             \
		}                                                  \
		break;

std::shared_ptr<TranscodeDecoder> TranscodeDecoder::Create(
	int32_t decoder_id,
	std::shared_ptr<info::Stream> info,
	std::shared_ptr<MediaTrack> track,
	std::shared_ptr<std::vector<std::shared_ptr<CodecCandidate>>> candidates,
	CompleteHandler complete_handler)
{
	std::shared_ptr<TranscodeDecoder> decoder = nullptr;
	std::shared_ptr<CodecCandidate> cur_candidate = nullptr;

	for (auto &candidate : *candidates)
	{
		cur_candidate = candidate;

		if (candidate->GetCodecId() == cmn::MediaCodecId::H264)
		{
			switch (candidate->GetModuleId())
			{
				CASE_CREATE_CODEC_IFNEED(DEFAULT, DecoderAVC)
				CASE_CREATE_CODEC_IFNEED(QSV, DecoderAVCxQSV)
				CASE_CREATE_CODEC_IFNEED(NILOGAN, DecoderAVCxNILOGAN)
				CASE_CREATE_CODEC_IFNEED(NVENC, DecoderAVCxNV)
				CASE_CREATE_CODEC_IFNEED(XMA, DecoderAVCxXMA)
				default:
					break;
			}
			break;
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::H265)
		{
			switch (candidate->GetModuleId())
			{
				CASE_CREATE_CODEC_IFNEED(DEFAULT, DecoderHEVC)
				CASE_CREATE_CODEC_IFNEED(QSV, DecoderHEVCxQSV)
				CASE_CREATE_CODEC_IFNEED(NILOGAN, DecoderHEVCxNILOGAN)
				CASE_CREATE_CODEC_IFNEED(NVENC, DecoderHEVCxNV)
				CASE_CREATE_CODEC_IFNEED(XMA, DecoderHEVCxXMA)
				default:
					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Vp8)
		{
			switch (candidate->GetModuleId())
			{
				default:
					CASE_CREATE_CODEC_IFNEED(DEFAULT, DecoderVP8)

					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			switch (candidate->GetModuleId())
			{
				default:
					CASE_CREATE_CODEC_IFNEED(DEFAULT, DecoderAAC)
					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Opus)
		{
			switch (candidate->GetModuleId())
			{
				default:
					CASE_CREATE_CODEC_IFNEED(DEFAULT, DecoderOPUS)
					break;
			}
		}
		else if (candidate->GetCodecId() == cmn::MediaCodecId::Mp3)
		{
			switch (candidate->GetModuleId())
			{
				default:
					CASE_CREATE_CODEC_IFNEED(DEFAULT, DecoderMP3)
					break;
			}
		}
		else
		{
			OV_ASSERT(false, "Not supported codec: %d", track->GetCodecId());
		}

		// If the decoder is not created, try the next candidate.
		decoder = nullptr;
	}

done:
	if (decoder != nullptr)
	{
		track->SetCodecModuleId(cur_candidate->GetModuleId());
		decoder->SetDecoderId(decoder_id);
		decoder->SetCompleteHandler(complete_handler);

		logti("The decoder has been created. track(#%d) codec(%s), module(%s:%d)",
			  track->GetId(),
			  cmn::GetCodecIdToString(track->GetCodecId()).CStr(),
			  cmn::GetStringFromCodecModuleId(track->GetCodecModuleId()).CStr(),
			  track->GetCodecDeviceId());
	}

	return decoder;
}

TranscodeDecoder::TranscodeDecoder(info::Stream stream_info)
	: _stream_info(stream_info)
{
	_pkt = ::av_packet_alloc();
	_frame = ::av_frame_alloc();
	_codec_par = avcodec_parameters_alloc();
}

TranscodeDecoder::~TranscodeDecoder()
{
	if (_context != nullptr && _context->codec != nullptr)
	{
		if (_context->codec->capabilities & AV_CODEC_CAP_ENCODER_FLUSH)
		{
			::avcodec_flush_buffers(_context);
		}
	}

	if (_context != nullptr)
	{
		::avcodec_free_context(&_context);
	}

	if (_codec_par != nullptr)
	{
		::avcodec_parameters_free(&_codec_par);
	}

	if (_frame != nullptr)
	{
		::av_frame_free(&_frame);
	}

	if (_pkt != nullptr)
	{
		::av_packet_free(&_pkt);
	}

	if (_parser != nullptr)
	{
		::av_parser_close(_parser);
	}

	_input_buffer.Clear();
}

std::shared_ptr<MediaTrack> &TranscodeDecoder::GetRefTrack()
{
	return _track;
}

cmn::Timebase TranscodeDecoder::GetTimebase()
{
	return GetRefTrack()->GetTimeBase();
}

void TranscodeDecoder::SetDecoderId(int32_t decoder_id)
{
	_decoder_id = decoder_id;
}

bool TranscodeDecoder::Configure(std::shared_ptr<MediaTrack> track)
{
	if (track == nullptr)
	{
		return false;
	}
	_track = track;

	auto name = ov::String::FormatString("dec_%s_%d", ::avcodec_get_name(GetCodecID()), _track->GetId());
	auto urn = std::make_shared<info::ManagedQueue::URN>(
		_stream_info.GetApplicationInfo().GetVHostAppName(),
		_stream_info.GetName(),
		"trs",
		name);
	_input_buffer.SetUrn(urn);
	_input_buffer.SetThreshold(MAX_QUEUE_SIZE);

	_kill_flag = false;

	try
	{
		_codec_thread = std::thread(&TranscodeDecoder::CodecThread, this);
		pthread_setname_np(_codec_thread.native_handle(), ov::String::FormatString("DEC-%s-t%u", avcodec_get_name(GetCodecID()), _track->GetId()).CStr());

		// Initialize the codec and wait for completion.
		if (_codec_init_event.Get() == false)
		{
			_kill_flag = true;
			return false;
		}
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;
		return false;
	}

	return true;
}

void TranscodeDecoder::Stop()
{
	_kill_flag = true;

	_input_buffer.Stop();

	if (_codec_thread.joinable())
	{
		_codec_thread.join();

		logtd(ov::String::FormatString("decoder %s thread has ended", avcodec_get_name(GetCodecID())).CStr());
	}
}

void TranscodeDecoder::SendBuffer(std::shared_ptr<const MediaPacket> packet)
{
	_input_buffer.Enqueue(std::move(packet));
}

void TranscodeDecoder::SetCompleteHandler(CompleteHandler complete_handler)
{
	_complete_handler = std::move(complete_handler);
}

void TranscodeDecoder::Complete(TranscodeResult result, std::shared_ptr<MediaFrame> frame)
{
	// Invoke callback function when encoding/decoding is completed.
	if (_complete_handler)
	{
		frame->SetTrackId(_decoder_id);
		_complete_handler(result, _decoder_id, std::move(frame));
	}
}
