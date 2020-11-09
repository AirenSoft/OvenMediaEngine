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
		static void SetRtmpProvider(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::RtmpProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetRtspPullProvider(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::RtspPullProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetRtspProvider(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::RtspProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetOvtProvider(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::OvtProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetMpegtsStreams(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::mpegts::StreamMap &config, Optional optional)
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

		static void SetMpegtsProvider(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::MpegtsProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;

			SetMpegtsStreams(object, "streams", config.GetStreamMap(), Optional::True);
		}

		static void SetProviders(Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::Providers &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetRtmpProvider(object, "rtmp", config.GetRtmpProvider(), Optional::True);
			SetRtspPullProvider(object, "rtspPull", config.GetRtspPullProvider(), Optional::True);
			SetRtspProvider(object, "rtsp", config.GetRtspProvider(), Optional::True);
			SetOvtProvider(object, "ovt", config.GetOvtProvider(), Optional::True);
			SetMpegtsProvider(object, "mpegts", config.GetMpegtsProvider(), Optional::True);
		}

		static void SetRtmpPushPublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::RtmpPushPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetCrossDomains(Json::Value &parent_object, const char *key, const cfg::cmn::CrossDomain &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &url : config.GetUrls())
			{
				object.append(url.GetUrl().CStr());
			}
		}

		static void SetHlsPublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::HlsPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentCount", config.GetSegmentCount());
			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(object, "crossDomains", config.GetCrossDomain(), Optional::True);
		}

		static void SetDashPublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::DashPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentCount", config.GetSegmentCount());
			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(object, "crossDomains", config.GetCrossDomain(), Optional::True);
		}

		static void SetLlDashPublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::LlDashPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(object, "crossDomains", config.GetCrossDomain(), Optional::True);
		}

		static void SetWebrtcPublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::WebrtcPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetTimeInterval(object, "timeout", config.GetTimeout());
		}

		static void SetOvtPublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::OvtPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetFilePublisher(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::FilePublisher &config, Optional optional)
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

		static void SetPublishers(Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::Publishers &config, Optional optional)
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
		static void SetOutputProfiles(Json::Value &parent_object, const char *key, const cfg::vhost::app::enc::Encodes &encodes_config, const cfg::vhost::app::stream::Streams &streams_config, Optional optional)
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

		Json::Value JsonFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application)
		{
			Json::Value response(Json::ValueType::objectValue);

			SetString(response, "name", application->GetName().GetAppName(), Optional::False);
			SetBool(response, "dynamic", application->IsDynamicApp());
			SetString(response, "type", application->GetConfig().GetTypeString(), Optional::False);
			SetProviders(response, "providers", application->GetConfig().GetProviders(), Optional::True);
			SetPublishers(response, "publishers", application->GetConfig().GetPublishers(), Optional::True);

			return std::move(response);
		}

		void MakeUpperCase(const ov::String &path, const Json::Value &input, Json::Value *output)
		{
			switch (input.type())
			{
				case Json::ValueType::nullValue:
				case Json::ValueType::intValue:
				case Json::ValueType::uintValue:
				case Json::ValueType::realValue:
				case Json::ValueType::stringValue:
				case Json::ValueType::booleanValue:
				case Json::ValueType::arrayValue:
					*output = input;
					break;

				case Json::ValueType::objectValue:
					if (input.size() == 0)
					{
						*output = Json::objectValue;
					}
					else
					{
						for (auto item = input.begin(); item != input.end(); ++item)
						{
							const auto &input_name = item.name();
							ov::String name = input_name.c_str();

							if ((path == "application.providers") || (path == "application.publishers"))
							{
								// All settings names in providers or publishers are in capital letters except WebRTC
								if (name == "webrtc")
								{
									name = "WebRTC";
								}
								else
								{
									name.MakeUpper();
								}
							}
							else
							{
								// Change the first letter to uppercase
								if (name.GetLength() > 0)
								{
									auto buffer = name.GetBuffer();
									buffer[0] = ::toupper(buffer[0]);
								}
							}

							ov::String name_with_path;
							name_with_path.Format("%s.%s", path.CStr(), input_name.c_str());

							MakeUpperCase(name_with_path, *item, &(output->operator[](name)));
						}
					}

					break;
			}
		}

		std::shared_ptr<ov::Error> ApplicationFromJson(const Json::Value &json_value, cfg::vhost::app::Application *application)
		{
			Json::Value value;

			MakeUpperCase("application", json_value, &value);

			return application->Parse("", value, "Application");
		}
	}  // namespace conv
}  // namespace api