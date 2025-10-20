//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_codecs.h"

#include "transcoder_gpu.h"
#include "transcoder_private.h"

namespace tc
{
	TranscodeCodecs::TranscodeCodecs()
	{
		// TODO(Keukhan) : Currently, codec and hardware information are defined manually, but in the future,
		// it should be improved so that they are automatically registered based on the available codecs.

		// --------------------------------------------------------------------------
		// Video Codecs
		// --------------------------------------------------------------------------
		Register(CodecInfo("FFmpeg Video Codecs", cmn::MediaType::Video, cmn::MediaCodecModuleId::DEFAULT, 0, "-",
						   {cmn::MediaCodecId::H264, cmn::MediaCodecId::H265, cmn::MediaCodecId::Vp8},
						   true, false, false));

		Register(CodecInfo("Open Source H.264 Codec", cmn::MediaType::Video, cmn::MediaCodecModuleId::OPENH264, 0, "-",
						   {cmn::MediaCodecId::H264},
						   false, true, false));

#ifdef THIRDP_LIBX264_ENABLED
		Register(CodecInfo("x264 H.264 Codec", cmn::MediaType::Video, cmn::MediaCodecModuleId::X264, 0, "-",
						   {cmn::MediaCodecId::H264},
						   false, true, false));
#endif

		Register(CodecInfo("WebM VP8/VP9 Codec SDK", cmn::MediaType::Video, cmn::MediaCodecModuleId::LIBVPX, 0, "-",
						   {cmn::MediaCodecId::Vp8},
						   false, true, false));

		// --------------------------------------------------------------------------
		// Audio Codecs
		// --------------------------------------------------------------------------
		Register(CodecInfo("FFmpeg Audio Codecs", cmn::MediaType::Audio, cmn::MediaCodecModuleId::DEFAULT, 0, "-",
						   {cmn::MediaCodecId::Aac, cmn::MediaCodecId::Opus},
						   true, false, false));

		Register(CodecInfo("Fraunhofer FDK AAC", cmn::MediaType::Audio, cmn::MediaCodecModuleId::FDKAAC, 0, "-",
						   {cmn::MediaCodecId::Aac},
						   false, true, false));

		Register(CodecInfo("Opus Interactive Audio Codec", cmn::MediaType::Audio, cmn::MediaCodecModuleId::LIBOPUS, 0, "-",
						   {cmn::MediaCodecId::Opus},
						   false, true, false));

		// --------------------------------------------------------------------------
		// Image Codecs
		// --------------------------------------------------------------------------
		Register(CodecInfo("FFmpeg Image Codec", cmn::MediaType::Video, cmn::MediaCodecModuleId::DEFAULT, 0, "-",
						   {cmn::MediaCodecId::Jpeg, cmn::MediaCodecId::Png, cmn::MediaCodecId::Webp},
						   false, true, false));

		// --------------------------------------------------------------------------
		// Video Codecs - Hardware Accelerated
		// --------------------------------------------------------------------------
#ifdef HWACCELS_NVIDIA_ENABLED
		{
			auto module_id = cmn::MediaCodecModuleId::NVENC;
			for (int32_t i = 0; i < TranscodeGPU::GetInstance()->GetDeviceCount(module_id); i++)
			{
				ov::String device_name = TranscodeGPU::GetInstance()->GetDeviceDisplayName(module_id, i);
				ov::String bus_id	   = TranscodeGPU::GetInstance()->GetDeviceBusId(module_id, i);

				Register(CodecInfo(device_name, cmn::MediaType::Video, module_id, i, bus_id,
								   {cmn::MediaCodecId::H264, cmn::MediaCodecId::H265},
								   true, true, true));

				Register(CodecInfo(device_name, cmn::MediaType::Audio, module_id, i, bus_id,
								   {cmn::MediaCodecId::Whisper},
								   false, true, true));
			}
		}
#endif

#ifdef HWACCELS_XMA_ENABLED
		{
			auto module_id = cmn::MediaCodecModuleId::XMA;
			for (int32_t i = 0; i < TranscodeGPU::GetInstance()->GetDeviceCount(module_id); i++)
			{
				ov::String device_name = TranscodeGPU::GetInstance()->GetDeviceDisplayName(module_id, i);
				ov::String bus_id	   = TranscodeGPU::GetInstance()->GetDeviceBusId(module_id, i);

				Register(CodecInfo(device_name, cmn::MediaType::Video, module_id, i, bus_id,
								   {cmn::MediaCodecId::H264, cmn::MediaCodecId::H265},
								   true, true, true));
			}
		}
#endif
	}

	ov::String TranscodeCodecs::GetInfoString() const
	{
		ov::String info;
		for (const auto &codec : _codecs)
		{
			info += codec->GetInfoString() + "\n";
		}
		return info;
	}

	bool TranscodeCodecs::Activate(bool coder_type, cmn::MediaCodecId codec_id, cmn::MediaCodecModuleId module_id, cmn::DeviceId device_id)
	{
		auto codec = GetCodec(coder_type, codec_id, module_id, device_id);
		if (codec == nullptr)
		{
			logtw("Cannot find the codec to activate. coder_type(%s), codec_id(%s), module_id(%s), device_id(%d)",
				  coder_type ? "Encoder" : "Decoder",
				  cmn::GetCodecIdString(codec_id),
				  cmn::GetCodecModuleIdString(module_id),
				  device_id);

			return false;
		}

		(coder_type == true) ? codec->GetMetrics().IncreaseActiveEncoder() : codec->GetMetrics().IncreaseActiveDecoder();

		// logtw("\n%s", tc::TranscodeCodecs::GetInstance()->GetInfoString().CStr());

		return true;
	}

	bool TranscodeCodecs::Deactivate(bool coder_type, cmn::MediaCodecId codec_id, cmn::MediaCodecModuleId module_id, cmn::DeviceId device_id)
	{
		auto codec = GetCodec(coder_type, codec_id, module_id, device_id);
		if (codec == nullptr)
		{
			logtw("Cannot find the codec to deactivate. coder_type(%s), codec_id(%s), module_id(%s), device_id(%d)",
				  coder_type ? "Encoder" : "Decoder",
				  cmn::GetCodecIdString(codec_id),
				  cmn::GetCodecModuleIdString(module_id),
				  device_id);

			return false;
		}

		(coder_type == true) ? codec->GetMetrics().DecreaseActiveEncoder() : codec->GetMetrics().DecreaseActiveDecoder();

		// logtw("\n%s", tc::TranscodeCodecs::GetInstance()->GetInfoString().CStr());

		return true;
	}

	ov::String CodecInfo::GetInfoString() const
	{
		ov::String supported_codecs;
		for (const auto &codec : _supported_codecs)
		{
			if (supported_codecs.IsEmpty() == false)
			{
				supported_codecs.Append(",");
			}
			supported_codecs.Append(cmn::GetCodecIdString(codec));
		}

		ov::String metrics = ov::String::FormatString("dec%2d,enc%2d",
													  _metrics._active_decoder,
													  _metrics._active_encoder);

		return ov::String::FormatString("Name:%11s, DisplayName: %36s, Type: %s, Module: %8s, Id: %d, Decoder: %3s, Encoder: %3s, HWAccel: %3s, BusID: %16s, Supported: [%13s], Metrics: [%s]",
										GetName().CStr(),
										_display_name.CStr(),
										cmn::GetMediaTypeString(_media_type),
										cmn::GetCodecModuleIdString(_module_id),
										_device_id,
										_is_decoder ? "Yes" : "No",
										_is_encoder ? "Yes" : "No",
										_is_hwaccel ? "Yes" : "No",
										_bus_id.CStr(),
										supported_codecs.CStr(),
										metrics.CStr());
	}
}  // namespace tc
