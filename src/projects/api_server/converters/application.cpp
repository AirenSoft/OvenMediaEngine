//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "application.h"

#include "converter_private.h"

namespace api
{
	namespace conv
	{
		static void SetRtmpProvider(Json::Value &parent_object, const char *key, const cfg::RtmpProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);
		}

		static void SetRtspPullProvider(Json::Value &parent_object, const char *key, const cfg::RtspPullProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);
		}

		static void SetRtspProvider(Json::Value &parent_object, const char *key, const cfg::RtspProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);
		}

		static void SetOvtProvider(Json::Value &parent_object, const char *key, const cfg::OvtProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);
		}

		static void SetMpegtsStreams(Json::Value &parent_object, const char *key, const cfg::mpegts::StreamMap &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &stream : config.GetStreamList())
			{
				Json::Value item;

				item["name"] = stream.GetName().CStr();
				item["port"] = stream.GetPort().GetPortString().CStr();

				object.append(item);
			}
		}

		static void SetMpegtsProvider(Json::Value &parent_object, const char *key, const cfg::MpegtsProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetMpegtsStreams(object, "streams", config.GetStreamMap(), Optional::True);
		}

		static void SetProviders(Json::Value &parent_object, const char *key, const cfg::Providers &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetRtmpProvider(object, "rtmp", config.GetRtmpProvider(), Optional::True);
			SetRtspPullProvider(object, "rtspPull", config.GetRtspPullProvider(), Optional::True);
			SetRtspProvider(object, "rtsp", config.GetRtspProvider(), Optional::True);
			SetOvtProvider(object, "ovt", config.GetOvtProvider(), Optional::True);
			SetMpegtsProvider(object, "mpegts", config.GetMpegtsProvider(), Optional::True);
		}

		static void SetPublishers(Json::Value &parent_object, const char *key, const cfg::Publishers &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);
		}

		// OutputProfile is made by combining Encodes and Streams
		static void SetOutputProfiles(Json::Value &parent_object, const char *key, const cfg::Encodes &encodes_config, const cfg::Streams &streams_config, Optional optional)
		{
			CONVERTER_RETURN_IF((encodes_config.IsParsed() == false) || (streams_config.IsParsed() == false));
			// <Encodes>
			// 	<Encode>
			// 		<Name>bypass</Name>
			// 		<Audio>
			// 			<Bypass>true</Bypass>
			// 		</Audio>
			// 		<Video>
			// 			<Bypass>true</Bypass>
			// 		</Video>
			// 	</Encode>
			// 	<Encode>
			// 		<Name>opus</Name>
			// 		<Audio>
			// 			<Codec>opus</Codec>
			// 			<Bitrate>128000</Bitrate>
			// 			<Samplerate>48000</Samplerate>
			// 			<Channel>2</Channel>
			// 		</Audio>
			// 	</Encode>
			// </Encodes>
			// <Streams>
			// 	<Stream>
			// 		<Name>bypass_stream</Name>
			// 		<OutputStreamName>${OriginStreamName}</OutputStreamName>
			// 		<Profiles>
			// 			<Profile>bypass</Profile>
			// 			<Profile>opus</Profile>
			// 		</Profiles>
			// 	</Stream>
			// </Streams>
		}

		Json::Value ConvertFromApplication(const std::shared_ptr<const ocst::Application> &application)
		{
			Json::Value response;

			const auto &app_info = application->app_info;
			const auto &app_config = app_info.GetConfig();

			SetString(response, "name", app_info.GetName().GetAppName(), Optional::False);
			SetBool(response, "dynamic", app_info.IsDynamicApp());
			SetString(response, "type", app_config.GetTypeString(), Optional::False);
			SetProviders(response, "providers", app_config.GetProviders(), Optional::True);
			SetPublishers(response, "publishers", app_config.GetPublishers(), Optional::True);
			SetOutputProfiles(response, "publishers", app_config.GetEncodes(), app_config.GetStreams(), Optional::True);

			return response;
		}
	}  // namespace conv
}  // namespace api