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

		static void SetTracks(Json::Value &parent_object, const char *key, const std::map<int32_t, std::shared_ptr<MediaTrack>> &tracks, Optional optional)
		{
			CONVERTER_RETURN_IF(false);

			for (auto &item : tracks)
			{
				auto &track = item.second;

				// temp
				object.append(track->GetInfoString().CStr());
			}
		}

		static void SetInputStream(Json::Value &parent_object, const char *key, const std::shared_ptr<const mon::StreamMetrics> &stream, Optional optional)
		{
			CONVERTER_RETURN_IF(stream == nullptr);

			SetString(object, "sourceType", ::StringFromStreamSourceType(stream->GetSourceType()), Optional::False);
			SetString(object, "sourceUrl", stream->GetMediaSource(), Optional::True);
			// TODO(dimiden): Complete this function
			SetConnection(object, "connection", stream.get(), Optional::True);
			SetTracks(object, "tracks", stream->GetTracks(), Optional::False);
			SetTimestamp(object, "createdTime", stream->GetCreatedTime());
		}

		Json::Value ConvertFromStream(const std::shared_ptr<const mon::StreamMetrics> &stream, std::vector<std::shared_ptr<mon::StreamMetrics>> output_streams)
		{
			Json::Value response = Json::objectValue;

			SetString(response, "name", stream->GetName(), Optional::False);
			SetInputStream(response, "input", stream, Optional::False);

			return response;
		}
	}  // namespace conv
}  // namespace api