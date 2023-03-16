//==============================================================================
//
//  TranscoderStreamInternal
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_stream_internal.h"

#include "transcoder_private.h"

TranscoderStreamInternal::TranscoderStreamInternal()
{
}

TranscoderStreamInternal::~TranscoderStreamInternal()
{
}

ov::String TranscoderStreamInternal::GetIdentifiedForVideoProfile(const uint32_t track_id, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("In_T%d_Out_Pbypass", track_id);
	}

	auto unique_profile_name = ov::String::FormatString("In_T%d_Out_C%s-%d-%.02f-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());

	if(profile.GetPreset().IsEmpty() == false)
	{
		unique_profile_name +=  ov::String::FormatString("-Pr%s", profile.GetPreset().CStr());
	}

	if(profile.GetProfile().IsEmpty() == false)
	{
		unique_profile_name +=  ov::String::FormatString("-Pf%s", profile.GetProfile().CStr());
	}

	return unique_profile_name;
}

ov::String TranscoderStreamInternal::GetIdentifiedForImageProfile(const uint32_t track_id, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	return ov::String::FormatString("In_T%d_Out_C%s-%.02f-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetFramerate(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscoderStreamInternal::GetIdentifiedForAudioProfile(const uint32_t track_id, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("In_T%d_Out_Pbypass", track_id);
	}

	return ov::String::FormatString("In_T%d_Out_C%s-%d-%d-%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetSamplerate(),
									profile.GetChannel());
}

ov::String TranscoderStreamInternal::GetIdentifiedForDataProfile(const uint32_t track_id)
{
	return ov::String::FormatString("In_T%d_Out_Cbypass", track_id);
}

cmn::Timebase TranscoderStreamInternal::GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id)
{
	cmn::Timebase timebase(1, 1000);

	switch (codec_id)
	{
		case cmn::MediaCodecId::H264:
		case cmn::MediaCodecId::H265:
		case cmn::MediaCodecId::Vp8:
		case cmn::MediaCodecId::Vp9:
		case cmn::MediaCodecId::Flv:
		case cmn::MediaCodecId::Jpeg:
		case cmn::MediaCodecId::Png:
			timebase.SetNum(1);
			timebase.SetDen(90000);
			break;
		case cmn::MediaCodecId::Aac:
		case cmn::MediaCodecId::Mp3:
		case cmn::MediaCodecId::Opus:
			timebase.SetNum(1);
			timebase.SetDen(48000);
			break;
		default:
			break;
	}

	return timebase;
}

MediaTrackId TranscoderStreamInternal::NewTrackId()
{
	return _last_track_index++;
}

std::shared_ptr<MediaTrack> TranscoderStreamInternal::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
	{
		return nullptr;
	}

	bool is_parsed;
	profile.IsBypass(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetBypassByConfig(profile.IsBypass());
	}

	profile.GetBitrateString(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetBitrateByConfig(profile.GetBitrate());
	}

	profile.GetWidth(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetWidthByConfig(profile.GetWidth());
	}

	profile.GetHeight(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetHeightByConfig(profile.GetHeight());
	}

	profile.GetFramerate(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetFrameRateByConfig(profile.GetFramerate());
	}

	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId());
	output_track->SetVariantName(profile.GetName());
	output_track->SetPublicName(input_track->GetPublicName());
	output_track->SetLanguage(input_track->GetLanguage());
	output_track->SetOriginBitstream(input_track->GetOriginBitstream());

	bool need_bypass = IsMatchesBypassCondition(input_track, profile);
	if (need_bypass == true)
	{
		output_track->SetBypass(true);
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetCodecLibraryId(input_track->GetCodecLibraryId());
		output_track->SetWidth(input_track->GetWidth());
		output_track->SetHeight(input_track->GetHeight());
		output_track->SetTimeBase(input_track->GetTimeBase());
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
		output_track->SetCodecLibraryId(cmn::GetCodecLibraryIdByName(profile.GetCodec()));
		output_track->SetWidth(profile.GetWidth());
		output_track->SetHeight(profile.GetHeight());
		output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));
		output_track->SetPreset(profile.GetPreset());
		output_track->SetThreadCount(profile.GetThreadCount());
		output_track->SetKeyFrameInterval(profile.GetKeyFrameInterval());
		output_track->SetBFrames(profile.GetBFrames());
		output_track->SetProfile(profile.GetProfile());
	}

	if(output_track->GetFrameRateByConfig() == 0)
	{
		output_track->SetFrameRateByMeasured(input_track->GetFrameRate());
	}

	if(output_track->GetBitrateByConfig() == 0)
	{
		output_track->SetBitrateByMeasured(input_track->GetBitrate());
	}

	if (cmn::IsVideoCodec(output_track->GetCodecId()) == false)
	{
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscoderStreamInternal::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
	{
		return nullptr;
	}

	bool is_parsed;
	profile.IsBypass(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetBypassByConfig(profile.IsBypass());
	}

	profile.GetBitrateString(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetBitrateByConfig(profile.GetBitrate());
	}

	output_track->SetMediaType(cmn::MediaType::Audio);
	output_track->SetId(NewTrackId());
	output_track->SetVariantName(profile.GetName());
	output_track->SetPublicName(input_track->GetPublicName());
	output_track->SetLanguage(input_track->GetLanguage());
	output_track->SetOriginBitstream(input_track->GetOriginBitstream());

	bool need_bypass = IsMatchesBypassCondition(input_track, profile);
	if (need_bypass == true)
	{
		output_track->SetBypass(true);
		output_track->SetCodecId(input_track->GetCodecId());
		output_track->SetCodecLibraryId(input_track->GetCodecLibraryId());
		output_track->SetChannel(input_track->GetChannel());
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
		output_track->SetTimeBase(input_track->GetTimeBase());
		output_track->SetSampleRate(input_track->GetSampleRate());
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
		output_track->SetCodecLibraryId(cmn::GetCodecLibraryIdByName(profile.GetCodec()));
		output_track->GetChannel().SetLayout(profile.GetChannel() == 1 ? cmn::AudioChannel::Layout::LayoutMono : cmn::AudioChannel::Layout::LayoutStereo);
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());	// The sample format will change by the decoder event.
		output_track->SetSampleRate(profile.GetSamplerate());

		// Samplerate
		if (output_track->GetCodecId() == cmn::MediaCodecId::Opus)
		{
			if (output_track->GetSampleRate() != 48000)
			{
				output_track->SetSampleRate(48000);
				logtw("OPUS codec only supports 48000Hz samplerate. change the samplerate to 48000Hz");
			}
		}

		// Timebase
		if (output_track->GetSampleRate() == 0)
		{
			output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));
		}
		else
		{
			output_track->SetTimeBase(1, output_track->GetSampleRate());
		}
	}

	// Bitrate
	if(output_track->GetBitrateByConfig() == 0)
	{
		output_track->SetBitrateByMeasured(input_track->GetBitrate());
	}

	if (cmn::IsAudioCodec(output_track->GetCodecId()) == false)
	{
		logtw("Encoding codec set is not a audio codec");
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscoderStreamInternal::CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
	{
		return nullptr;
	}

	bool is_parsed;
	profile.GetWidth(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetWidthByConfig(profile.GetWidth());
	}

	profile.GetHeight(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetHeightByConfig(profile.GetHeight());
	}

	profile.GetFramerate(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetFrameRateByConfig(profile.GetFramerate());
	}

	output_track->SetPublicName(input_track->GetPublicName());
	output_track->SetLanguage(input_track->GetLanguage());
	output_track->SetVariantName(profile.GetName());
	output_track->SetOriginBitstream(input_track->GetOriginBitstream());
	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId());
	output_track->SetBypass(false);
	output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
	output_track->SetCodecLibraryId(cmn::GetCodecLibraryIdByName(profile.GetCodec()));
	output_track->SetBitrateByConfig(0);
	output_track->SetWidth(profile.GetWidth());
	output_track->SetHeight(profile.GetHeight());
	output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));

	if(output_track->GetFrameRateByConfig() == 0)
	{
		output_track->SetFrameRateByMeasured(input_track->GetFrameRate());
	}

	if (cmn::IsImageCodec(output_track->GetCodecId()) == false)
	{
		return nullptr;
	}

	return output_track;
}

std::shared_ptr<MediaTrack> TranscoderStreamInternal::CreateOutputTrackDataType(const std::shared_ptr<MediaTrack> &input_track)
{
	auto output_track = std::make_shared<MediaTrack>();
	if (output_track == nullptr)
	{
		return nullptr;
	}

	output_track->SetMediaType(cmn::MediaType::Data);
	output_track->SetId(NewTrackId());
	output_track->SetVariantName("");
	output_track->SetPublicName(input_track->GetPublicName());
	output_track->SetLanguage(input_track->GetLanguage());
	output_track->SetBypass(true);
	output_track->SetCodecId(input_track->GetCodecId());
	output_track->SetCodecLibraryId(input_track->GetCodecLibraryId());
	output_track->SetOriginBitstream(input_track->GetOriginBitstream());
	output_track->SetWidth(input_track->GetWidth());
	output_track->SetHeight(input_track->GetHeight());
	output_track->SetFrameRateByMeasured(input_track->GetFrameRate());
	output_track->SetTimeBase(input_track->GetTimeBase());

	return output_track;
}


bool TranscoderStreamInternal::IsMatchesBypassCondition(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	bool is_parsed = false;
	uint32_t if_count;
	ov::String condition;

	if (profile.IsBypass() == true)
	{
		return true;
	}

	auto if_match = profile.GetBypassIfMatch(&is_parsed);
	if (is_parsed == false)
	{
		return false;
	}
	if_count = 0;

	condition = if_match.GetCodec(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && (cmn::GetCodecIdByName(profile.GetCodec().CStr()) != input_track->GetCodecId()))
		{
			return false;
		}

		if_count++;
	}

	condition = if_match.GetWidth(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && (input_track->GetWidth() != profile.GetWidth()))
		{
			return false;
		}
		else if ((condition == "LTE") && input_track->GetWidth() > profile.GetWidth())
		{
			return false;
		}
		else if (condition == "GTE" && (input_track->GetWidth() < profile.GetWidth()))
		{
			return false;
		}

		if_count++;
	}

	condition = if_match.GetHeight(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && (input_track->GetHeight() != profile.GetHeight()))
		{
			return false;
		}
		else if ((condition == "LTE") && input_track->GetHeight() > profile.GetHeight())
		{
			return false;
		}
		else if (condition == "GTE" && (input_track->GetHeight() < profile.GetHeight()))
		{
			return false;
		}

		if_count++;
	}

	condition = if_match.GetFramerate(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && (input_track->GetFrameRate() != profile.GetFramerate()))
		{
			return false;
		}
		else if ((condition == "LTE") && input_track->GetFrameRate() > profile.GetFramerate())
		{
			return false;
		}
		else if (condition == "GTE" && (input_track->GetFrameRate() < profile.GetFramerate()))
		{
			return false;
		}

		if_count++;
	}

	condition = if_match.GetSAR(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if (condition == "EQ") 
		{
			float track_sar = (float)input_track->GetWidth() / (float)input_track->GetHeight();
			float profile_sar = (float)profile.GetWidth() / (float)profile.GetHeight();

			if (track_sar != profile_sar)
			{
				return false;
			}
		}

		if_count++;
	}	

	return (if_count > 0) ? true : false;
}

bool TranscoderStreamInternal::IsMatchesBypassCondition(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	bool is_parsed = false;
	uint32_t if_count;

	ov::String condition;

	if (profile.IsBypass() == true)
	{
		return true;
	}

	auto if_match = profile.GetBypassIfMatch(&is_parsed);
	if (is_parsed == false)
	{
		return false;
	}
	if_count = 0;

	condition = if_match.GetCodec(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && (cmn::GetCodecIdByName(profile.GetCodec().CStr()) != input_track->GetCodecId()))
		{
			if (cmn::GetCodecIdByName(profile.GetCodec().CStr()) != input_track->GetCodecId())
			{
				return false;
			}
		}

		if_count++;
	}

	condition = if_match.GetSamplerate(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && (input_track->GetSampleRate() != profile.GetSamplerate()))
		{
			return false;
		}
		else if ((condition == "LTE") && (input_track->GetSampleRate() > profile.GetSamplerate()))
		{
			return false;
		}
		else if ((condition == "GTE") && (input_track->GetSampleRate() < profile.GetSamplerate()))
		{
			return false;
		}

		if_count++;
	}

	condition = if_match.GetChannel(&is_parsed).UpperCaseString();
	if (is_parsed == true)
	{
		if ((condition == "EQ") && ((const int)input_track->GetChannel().GetCounts() != profile.GetChannel()))
		{
			return false;
		}
		else if ((condition == "LTE") && ((const int)input_track->GetChannel().GetCounts() > profile.GetChannel()))
		{
			return false;
		}
		else if ((condition == "GTE") && ((const int)input_track->GetChannel().GetCounts() < profile.GetChannel()))
		{
			return false;
		}

		if_count++;
	}

	return (if_count > 0) ? true : false;
}
