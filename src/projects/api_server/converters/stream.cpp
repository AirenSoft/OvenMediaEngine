//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application.h"
#include "common.h"

namespace api
{
	namespace conv
	{
		static void SetConnection(Json::Value &parent_object, const char *key, const void *temp, Optional optional)
		{
		}

		static void SetTimebase(Json::Value &parent_object, const char *key, const cmn::Timebase &timebase, Optional optional)
		{
			CONVERTER_RETURN_IF(timebase.GetDen() > 0, Json::objectValue);

			SetInt(object, "num", timebase.GetNum());
			SetInt(object, "den", timebase.GetDen());
		}

		static void SetVideoTrack(Json::Value &parent_object, const char *key, const std::shared_ptr<MediaTrack> &track, Optional optional)
		{
			CONVERTER_RETURN_IF(track == nullptr, Json::objectValue);

			SetBool(object, "bypass", track->IsBypass());

			if (track->IsBypass() == false)
			{
				SetString(object, "codec", ::StringFromMediaCodecId(track->GetCodecId()), Optional::False);
				SetInt(object, "width", track->GetWidth());
				SetInt(object, "height", track->GetHeight());
				SetString(object, "bitrate", ov::Converter::ToString(track->GetBitrate()), Optional::False);
				SetFloat(object, "framerate", track->GetFrameRate());
				SetTimebase(object, "timebase", track->GetTimeBase(), Optional::False);
			}
		}

		static void SetAudioChannel(Json::Value &parent_object, const char *key, const cmn::AudioChannel &channel, Optional optional)
		{
			CONVERTER_RETURN_IF(channel.GetLayout() == cmn::AudioChannel::Layout::LayoutUnknown, Json::objectValue);

			SetString(object, "layout", channel.GetName(), Optional::False);
			SetInt(object, "count", channel.GetCounts());
		}

		static void SetAudioTrack(Json::Value &parent_object, const char *key, const std::shared_ptr<MediaTrack> &track, Optional optional)
		{
			CONVERTER_RETURN_IF(track == nullptr, Json::objectValue);

			SetBool(object, "bypass", track->IsBypass());

			if (track->IsBypass() == false)
			{
				SetString(object, "codec", ::StringFromMediaCodecId(track->GetCodecId()), Optional::False);
				SetInt(object, "samplerate", track->GetSampleRate());
				// SetAudioChannel(object, "channel", track->GetChannel(), Optional::False);
				SetInt(object, "channel", track->GetChannel().GetCounts());
				SetString(object, "bitrate", ov::Converter::ToString(track->GetBitrate()), Optional::False);
				SetTimebase(object, "timebase", track->GetTimeBase(), Optional::False);
			}
		}

		static void SetTracks(Json::Value &parent_object, const char *key, const std::map<int32_t, std::shared_ptr<MediaTrack>> &tracks, Optional optional)
		{
			CONVERTER_RETURN_IF(false, Json::arrayValue);

			for (auto &item : tracks)
			{
				auto &track = item.second;

				Json::Value track_value;

				SetString(track_value, "type", ::StringFromMediaType(track->GetMediaType()), Optional::False);

				switch (track->GetMediaType())
				{
					case cmn::MediaType::Video:
						SetVideoTrack(track_value, "video", track, Optional::False);
						break;
					case cmn::MediaType::Audio:
						SetAudioTrack(track_value, "audio", track, Optional::False);
						break;

					case cmn::MediaType::Unknown:
						[[fallthrough]];
					case cmn::MediaType::Data:
						[[fallthrough]];
					case cmn::MediaType::Subtitle:
						[[fallthrough]];
					case cmn::MediaType::Attachment:
						[[fallthrough]];
					case cmn::MediaType::Nb:
						break;
				}

				object.append(track_value);
			}
		}

		static void SetInputStream(Json::Value &parent_object, const char *key, const std::shared_ptr<const mon::StreamMetrics> &stream, Optional optional)
		{
			auto common_metrics = std::static_pointer_cast<const mon::CommonMetrics>(stream);

			CONVERTER_RETURN_IF(common_metrics == nullptr, Json::objectValue);

			SetString(object, "sourceType", ::StringFromStreamSourceType(stream->GetSourceType()), Optional::False);
			SetString(object, "sourceUrl", stream->GetMediaSource(), Optional::True);
			// TODO(dimiden): Complete this function
			SetConnection(object, "connection", stream.get(), Optional::True);
			SetTracks(object, "tracks", stream->GetTracks(), Optional::False);
			SetTimestamp(object, "createdTime", common_metrics->GetCreatedTime());
		}

		static void SetOutputStreams(Json::Value &parent_object, const char *key, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams, Optional optional)
		{
			CONVERTER_RETURN_IF(false, Json::arrayValue);

			for (auto &output_stream : output_streams)
			{
				Json::Value output_value;

				SetString(output_value, "name", output_stream->GetName(), Optional::False);
				SetTracks(output_value, "tracks", output_stream->GetTracks(), Optional::False);

				object.append(output_value);
			}
		}

		Json::Value JsonFromStream(const std::shared_ptr<const mon::StreamMetrics> &stream, const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			Json::Value response(Json::ValueType::objectValue);

			SetString(response, "name", stream->GetName(), Optional::False);
			SetInputStream(response, "input", stream, Optional::False);
			SetOutputStreams(response, "outputs", output_streams, Optional::False);

			return std::move(response);
		}
	}  // namespace conv
}  // namespace api