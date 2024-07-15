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

ov::String TranscoderStreamInternal::ProfileToSerialize(const uint32_t track_id, const cfg::vhost::app::oprf::VideoProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("I=T%d,O=bypass", track_id);
	}

	auto unique_profile_name = ov::String::FormatString("I=%d,O=%s:%d:%.02f:%d:%d:%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetFramerate(),
									profile.GetSkipFrames(),
									profile.GetWidth(),
									profile.GetHeight());

	if(profile.GetPreset().IsEmpty() == false)
	{
		unique_profile_name +=  ov::String::FormatString(":%s", profile.GetPreset().CStr());
	}

	if(profile.GetProfile().IsEmpty() == false)
	{
		unique_profile_name +=  ov::String::FormatString(":%s", profile.GetProfile().CStr());
	}

	return unique_profile_name;
}

ov::String TranscoderStreamInternal::ProfileToSerialize(const uint32_t track_id, const cfg::vhost::app::oprf::ImageProfile &profile)
{
	return ov::String::FormatString("I=%d,O=%s:%.02f:%d:%d:%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetFramerate(),
									profile.GetSkipFrames(),
									profile.GetWidth(),
									profile.GetHeight());
}

ov::String TranscoderStreamInternal::ProfileToSerialize(const uint32_t track_id, const cfg::vhost::app::oprf::AudioProfile &profile)
{
	if (profile.IsBypass() == true)
	{
		return ov::String::FormatString("I=%d,O=bypass", track_id);
	}

	return ov::String::FormatString("I=%d,O=%s:%d:%d:%d",
									track_id,
									profile.GetCodec().CStr(),
									profile.GetBitrate(),
									profile.GetSamplerate(),
									profile.GetChannel());
}

ov::String TranscoderStreamInternal::ProfileToSerialize(const uint32_t track_id)
{
	return ov::String::FormatString("I=%d,O=bypass", track_id);
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

std::shared_ptr<MediaTrack> TranscoderStreamInternal::CreateOutputTrack(
	const std::shared_ptr<MediaTrack> &input_track, 
	const cfg::vhost::app::oprf::VideoProfile &profile
	)
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

	profile.GetKeyFrameInterval(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetKeyFrameIntervalByConfig(profile.GetKeyFrameInterval());
	}

	profile.GetKeyFrameIntervalType(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetKeyFrameIntervalTypeByConfig(cmn::GetKeyFrameIntervalTypeByName(profile.GetKeyFrameIntervalType()));
	}

	profile.GetSkipFrames(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetSkipFramesByConfig(profile.GetSkipFrames());
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
		output_track->SetCodecModules(input_track->GetCodecModules());
		output_track->SetCodecModuleId(input_track->GetCodecModuleId());
		output_track->SetWidth(input_track->GetWidth());
		output_track->SetHeight(input_track->GetHeight());
		output_track->SetTimeBase(input_track->GetTimeBase());
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
		output_track->SetCodecModules(profile.GetModules());
		output_track->SetWidth(profile.GetWidth());
		output_track->SetHeight(profile.GetHeight());
		output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));
		output_track->SetPreset(profile.GetPreset());
		output_track->SetThreadCount(profile.GetThreadCount());
		output_track->SetBFrames(profile.GetBFrames());
		output_track->SetProfile(profile.GetProfile());
	}

	//  If the framerate is not set, it is set to the same value as the input.
	if(output_track->GetFrameRateByConfig() == 0)
	{
		output_track->SetFrameRateByConfig(input_track->GetFrameRateByConfig());
		output_track->SetFrameRateByMeasured(input_track->GetFrameRateByMeasured());
	}

	//  If the bitrate is not set, it is set to the same value as the input.
	if(output_track->GetBitrateByConfig() == 0)
	{
		output_track->SetBitrateByConfig(input_track->GetBitrateByConfig());
		output_track->SetBitrateByMeasured(input_track->GetBitrateByMeasured());
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
		output_track->SetCodecModules(input_track->GetCodecModules());
		output_track->SetCodecModuleId(input_track->GetCodecModuleId());
		output_track->SetChannel(input_track->GetChannel());
		output_track->GetSample().SetFormat(input_track->GetSample().GetFormat());
		output_track->SetTimeBase(input_track->GetTimeBase());
		output_track->SetSampleRate(input_track->GetSampleRate());
	}
	else
	{
		output_track->SetBypass(false);
		output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
		output_track->SetCodecModules(profile.GetModules());
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

	//  If the bitrate is not set, it is set to the same value as the input.
	if(output_track->GetBitrateByConfig() == 0)
	{
		output_track->SetBitrateByConfig(input_track->GetBitrateByConfig());
		output_track->SetBitrateByMeasured(input_track->GetBitrateByMeasured());
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

	profile.GetSkipFrames(&is_parsed);
	if (is_parsed == true)
	{
		output_track->SetSkipFramesByConfig(profile.GetSkipFrames());
	}

	output_track->SetPublicName(input_track->GetPublicName());
	output_track->SetLanguage(input_track->GetLanguage());
	output_track->SetVariantName(profile.GetName());
	output_track->SetOriginBitstream(input_track->GetOriginBitstream());

	output_track->SetMediaType(cmn::MediaType::Video);
	output_track->SetId(NewTrackId());
	output_track->SetBypass(false);
	output_track->SetCodecId(cmn::GetCodecIdByName(profile.GetCodec()));
	output_track->SetCodecModules(profile.GetModules());
	output_track->SetWidth(profile.GetWidth());
	output_track->SetHeight(profile.GetHeight());
	output_track->SetTimeBase(GetDefaultTimebaseByCodecId(output_track->GetCodecId()));

	// Github Issue : #1417
	// Set any value for quick validation of the output track. 
	// If the validation of OutputTrack is delayed, the Stream Prepare event occurs late in Publisher.
	// The bitrate of an image doesn’t mean much anyway.
	output_track->SetBitrateByConfig(0);
	output_track->SetBitrateByMeasured(1000000);
	
	//  If the framerate is not set, it is set to the same value as the input.
	if(output_track->GetFrameRateByConfig() == 0)
	{
		output_track->SetFrameRateByMeasured(input_track->GetFrameRate());
	}
	else
	{
		output_track->SetFrameRateByMeasured(output_track->GetFrameRateByConfig());
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
	output_track->SetCodecModules("");
	output_track->SetCodecModuleId(input_track->GetCodecModuleId());
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

double TranscoderStreamInternal::GetProperFramerate(const std::shared_ptr<MediaTrack> &ref_track)
{
	double default_framerate = 30.0f;

	if (ref_track->GetFrameRateByConfig() > 0.0f)
	{
		double defined_framerate = ref_track->GetFrameRateByConfig();
		logti("Output framerate is not set. set the estimated framerate from framerate of input track. %.2ffps",
			  defined_framerate);
		
		return defined_framerate;
	}
	else if (ref_track->GetFrameRateByMeasured() > 0.0f)
	{
		double measured_framerate = ref_track->GetFrameRateByMeasured();
		double recommended_framerate = MeasurementToRecommendFramerate(measured_framerate);

		logti("Output framerate is not set. set the recommended framerate from measured framerate of input track. %.2f -> %.2f",
			  measured_framerate, recommended_framerate);

		return recommended_framerate;
	}

	logti("Output framerate is not set. set the default framerate. %.2ffps", default_framerate);

	return default_framerate;
}

double TranscoderStreamInternal::MeasurementToRecommendFramerate(double framerate)
{
	double start_framerate = ::ceil(framerate);
	if(start_framerate < 5.0f)
		start_framerate = 5.0f;
	
	// It is greater than the measured frame rate and is set to a value that is divisible by an integer in the timebase(90Hz).
	// In chunk-based protocols, the chunk length is made stable.
	double recommend_framerate = start_framerate;

	while (true)
	{
		if ((int)(90000 % (int64_t)recommend_framerate) == 0)
		{
			break;
		}

		recommend_framerate++;
	}

	return ::floor(recommend_framerate);
}

void TranscoderStreamInternal::UpdateOutputTrackPassthrough(const std::shared_ptr<MediaTrack> &output_track, MediaFrame *buffer)
{
	if (output_track->GetMediaType() == cmn::MediaType::Video)
	{
		output_track->SetWidth(buffer->GetWidth());
		output_track->SetHeight(buffer->GetHeight());
		output_track->SetColorspace(buffer->GetFormat());
	}
	else if (output_track->GetMediaType() == cmn::MediaType::Audio)
	{
		output_track->SetSampleRate(buffer->GetSampleRate());
		output_track->GetSample().SetFormat(buffer->GetFormat<cmn::AudioSample::Format>());
		output_track->SetChannel(buffer->GetChannels());
	}
}

void TranscoderStreamInternal::UpdateOutputTrackTranscode(const std::shared_ptr<MediaTrack> &output_track, const std::shared_ptr<MediaTrack> &input_track, MediaFrame *buffer)
{
	if (output_track->GetMediaType() == cmn::MediaType::Video)
	{
		float aspect_ratio = (float)buffer->GetWidth() / (float)buffer->GetHeight();

		// Keep the original video resolution
		if (output_track->GetWidth() == 0 && output_track->GetHeight() == 0)
		{
			output_track->SetWidth(buffer->GetWidth());
			output_track->SetHeight(buffer->GetHeight());
		}
		// Width is automatically calculated as the original video ratio
		else if (output_track->GetWidth() == 0 && output_track->GetHeight() != 0)
		{
			int32_t width = (int32_t)((float)output_track->GetHeight() * aspect_ratio);
			width = (width % 2 == 0) ? width : width + 1;
			output_track->SetWidth(width);
		}
		// Heigh is automatically calculated as the original video ratio
		else if (output_track->GetWidth() != 0 && output_track->GetHeight() == 0)
		{
			int32_t height = (int32_t)((float)output_track->GetWidth() / aspect_ratio);
			height = (height % 2 == 0) ? height : height + 1;
			output_track->SetHeight(height);
		}

		// Set framerate of the output track
		if (output_track->GetFrameRate() == 0.0f)
		{
			auto framerate = GetProperFramerate(input_track);
			output_track->SetEstimateFrameRate(framerate);
		}

		// To be compatible with all hardware. The encoding resolution must be a multiple of 4
		// In particular, Xilinx Media Accelerator must have a resolution specified in multiples of 4.
		if (output_track->GetWidth() % 4 != 0)
		{
			int32_t new_width = (output_track->GetWidth() / 4 + 1) * 4;

			logtd("The width of the output track is not a multiple of 4. change the width to %d -> %d", output_track->GetWidth(), new_width);

			output_track->SetWidth(new_width);
		}

		if (output_track->GetHeight() % 4 != 0)
		{
			int32_t new_height = (output_track->GetHeight() / 4 + 1) * 4;

			logtd("The height of the output track is not a multiple of 4. change the height to %d -> %d", output_track->GetHeight(), new_height);

			output_track->SetHeight(new_height);
		}
	}
	else if (output_track->GetMediaType() == cmn::MediaType::Audio)
	{
		if (output_track->GetSampleRate() == 0)
		{
			output_track->SetSampleRate(buffer->GetSampleRate());
			output_track->SetTimeBase(1, buffer->GetSampleRate());
		}

		if (output_track->GetChannel().GetLayout() == cmn::AudioChannel::Layout::LayoutUnknown)
		{
			output_track->SetChannel(buffer->GetChannels());
		}
	}
}

bool TranscoderStreamInternal::StoreInputTrackSnapshot(std::shared_ptr<info::Stream> stream)
{
	_input_track_snapshot.clear();
	
	for (auto &[track_id, track] : stream->GetTracks())
	{
		auto clone = track->Clone();
		_input_track_snapshot[track_id] = clone;
	}	

	return true;
}

std::map<int32_t, std::shared_ptr<MediaTrack>>& TranscoderStreamInternal::GetInputTrackSnapshot()
{
	return _input_track_snapshot;
}

bool TranscoderStreamInternal::IsEqualCountAndMediaTypeOfMediaTracks(std::map<int32_t, std::shared_ptr<MediaTrack>> a, std::map<int32_t, std::shared_ptr<MediaTrack>> b)
{
	if (a.size() != b.size())
	{
		return false;
	}

	for (auto &[track_id, track] : a)
	{
		if (b.find(track_id) == b.end())
		{
			return false;
		}

		auto track_b = b[track_id];

		if(track->GetMediaType() != track_b->GetMediaType())
		{
			return false;
		}
	}

	return true;
}
