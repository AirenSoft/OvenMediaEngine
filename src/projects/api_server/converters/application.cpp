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
		static void SetRtmpProvider(Json::Value &parent_object, const char *key, const cfg::RtmpProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetRtspPullProvider(Json::Value &parent_object, const char *key, const cfg::RtspPullProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetRtspProvider(Json::Value &parent_object, const char *key, const cfg::RtspProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetOvtProvider(Json::Value &parent_object, const char *key, const cfg::OvtProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetMpegtsStreams(Json::Value &parent_object, const char *key, const cfg::mpegts::StreamMap &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;

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

			object = Json::objectValue;

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

		static void SetRtmpPushPublisher(Json::Value &parent_object, const char *key, const cfg::RtmpPushPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetCrossDomains(Json::Value &parent_object, const char *key, const cfg::CrossDomain &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &url : config.GetUrls())
			{
				object.append(url.GetUrl().CStr());
			}
		}

		static void SetHlsPublisher(Json::Value &parent_object, const char *key, const cfg::HlsPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentCount", config.GetSegmentCount());
			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(object, "crossDomains", config.GetCrossDomain(), Optional::True);
		}

		static void SetDashPublisher(Json::Value &parent_object, const char *key, const cfg::DashPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentCount", config.GetSegmentCount());
			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(object, "crossDomains", config.GetCrossDomain(), Optional::True);
		}

		static void SetLlDashPublisher(Json::Value &parent_object, const char *key, const cfg::LlDashPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(object, "crossDomains", config.GetCrossDomain(), Optional::True);
		}

		static void SetWebrtcPublisher(Json::Value &parent_object, const char *key, const cfg::WebrtcPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetTimeInterval(object, "timeout", config.GetTimeout());
		}

		static void SetOvtPublisher(Json::Value &parent_object, const char *key, const cfg::OvtPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetFilePublisher(Json::Value &parent_object, const char *key, const cfg::FilePublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetString(object, "filePath", config.GetFilePath(), Optional::True);
			SetString(object, "fileInfoPath", config.GetFileInfoPath(), Optional::True);
		}

		// TODO(soulk): Uncomment below function after implement cfg::ThumbnailPublisher
		// static void SetThumbnailPublisher(Json::Value &parent_object, const char *key, const cfg::ThumbnailPublisher &config, Optional optional)
		// {
		// 	CONVERTER_RETURN_IF(config.IsParsed() == false);
		//
		// 	SetString(object, "filePath", config.GetFilePath(), Optional::True);
		// }

		static void SetPublishers(Json::Value &parent_object, const char *key, const cfg::Publishers &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "threadCount", config.GetThreadCount());
			SetRtmpPushPublisher(object, "rtmpPush", config.GetRtmpPushPublisher(), Optional::True);
			SetHlsPublisher(object, "hls", config.GetHlsPublisher(), Optional::True);
			SetDashPublisher(object, "dash", config.GetDashPublisher(), Optional::True);
			SetLlDashPublisher(object, "llDash", config.GetLlDashPublisher(), Optional::True);
			SetWebrtcPublisher(object, "webrtc", config.GetWebrtcPublisher(), Optional::True);
			SetOvtPublisher(object, "ovt", config.GetOvtPublisher(), Optional::True);
			SetFilePublisher(object, "file", config.GetFilePublisher(), Optional::True);
			// TODO(soulk): Uncomment this line after implement cfg::ThumbnailPublisher
			// SetThumbnailPublisher(object, "thumbnail", config.GetThumbnailPublisher(), Optional::True);
		}

		// OutputProfile is made by combining Encodes and Streams
		static void SetOutputProfiles(Json::Value &parent_object, const char *key, const cfg::Encodes &encodes_config, const cfg::Streams &streams_config, Optional optional)
		{
			CONVERTER_RETURN_IF((encodes_config.IsParsed() == false) || (streams_config.IsParsed() == false));

			// to Encodes and Streams:
			//
			// 	<Encodes>
			// 		<Encode>
			// 			<Name>bypass</Name>
			// 			<Audio>
			// 				<Bypass>true</Bypass>
			// 			</Audio>
			// 			<Video>
			// 				<Bypass>true</Bypass>
			// 			</Video>
			// 		</Encode>
			// 	</Encodes>
			// 	<Streams>
			// 		<Stream>
			// 			<Name>bypass_stream</Name>
			// 			<OutputStreamName>${OriginStreamName}</OutputStreamName>
			// 			<Profiles>
			// 				<Profile>bypass</Profile>
			// 				<Profile>opus</Profile>
			// 			</Profiles>
			// 		</Stream>
			// 	</Streams>
			//
			// from OutputProfile:
			//	 {
			//	 	"name": "bypass_stream",
			//	 	"outputStreamName": "${OriginStreamName}",
			//	 	"encodes": [
			//	 		{
			//	 			"name": "bypass",
			//	 			"audio": {
			//	 				"bypass": true
			//	 			},
			//	 			"video": {
			//	 				"bypass": true
			//	 			}
			//	 		}
			//	 	]
			//	 }
		}

		Json::Value ConvertFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application)
		{
			Json::Value response = Json::objectValue;

			SetString(response, "name", application->GetName().GetAppName(), Optional::False);
			SetBool(response, "dynamic", application->IsDynamicApp());
			SetString(response, "type", application->GetConfig().GetTypeString(), Optional::False);
			SetProviders(response, "providers", application->GetConfig().GetProviders(), Optional::True);
			SetPublishers(response, "publishers", application->GetConfig().GetPublishers(), Optional::True);

			return response;
		}
	}  // namespace conv
}  // namespace api