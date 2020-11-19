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
		static void SetRtmpProvider(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::RtmpProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetRtspPullProvider(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::RtspPullProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetRtspProvider(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::RtspProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetOvtProvider(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::OvtProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetMpegtsStreams(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::mpegts::StreamMap &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			auto app = mon::Monitoring::GetInstance()->GetApplicationMetrics(app_info);

			if (app == nullptr)
			{
				OV_ASSERT2(false);
				return;
			}

			// auto &stream_map = app->GetStreamMap();

			object = Json::arrayValue;

			for (auto &stream : config.GetStreamList())
			{
				Json::Value item;

				SetString(item, "name", stream.GetName(), Optional::False);
				// TODO(dimiden): use stream_map
				SetString(item, "port", stream.GetPort().GetPortString(), Optional::False);

				object.append(item);
			}
		}

		static void SetMpegtsProvider(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::MpegtsProvider &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;

			SetMpegtsStreams(app_info, object, "streams", config.GetStreamMap(), Optional::True);
		}

		static void SetProviders(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pvd::Providers &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetRtmpProvider(app_info, object, "rtmp", config.GetRtmpProvider(), Optional::True);
			SetRtspPullProvider(app_info, object, "rtspPull", config.GetRtspPullProvider(), Optional::True);
			SetRtspProvider(app_info, object, "rtsp", config.GetRtspProvider(), Optional::True);
			SetOvtProvider(app_info, object, "ovt", config.GetOvtProvider(), Optional::True);
			SetMpegtsProvider(app_info, object, "mpegts", config.GetMpegtsProvider(), Optional::True);
		}

		static void SetRtmpPushPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::RtmpPushPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetCrossDomains(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::cmn::CrossDomains &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &url : config.GetUrls())
			{
				object.append(url.GetUrl().CStr());
			}
		}

		static void SetHlsPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::HlsPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentCount", config.GetSegmentCount());
			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(app_info, object, "crossDomains", config.GetCrossDomains(), Optional::True);
		}

		static void SetDashPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::DashPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentCount", config.GetSegmentCount());
			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(app_info, object, "crossDomains", config.GetCrossDomains(), Optional::True);
		}

		static void SetLlDashPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::LlDashPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "segmentDuration", config.GetSegmentDuration());
			SetCrossDomains(app_info, object, "crossDomains", config.GetCrossDomains(), Optional::True);
		}

		static void SetWebrtcPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::WebrtcPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetTimeInterval(object, "timeout", config.GetTimeout());
		}

		static void SetOvtPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::OvtPublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			object = Json::objectValue;
		}

		static void SetFilePublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::FilePublisher &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetString(object, "filePath", config.GetFilePath(), Optional::True);
			SetString(object, "fileInfoPath", config.GetFileInfoPath(), Optional::True);
		}

		// TODO(soulk): Uncomment below function after implement cfg::ThumbnailPublisher
		// static void SetThumbnailPublisher(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::ThumbnailPublisher &config, Optional optional)
		// {
		// 	CONVERTER_RETURN_IF(config.IsParsed() == false);
		//
		// 	SetString(object, "filePath", config.GetFilePath(), Optional::True);
		// }

		static void SetPublishers(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::pub::Publishers &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetInt(object, "sessionLoadBalancingThreadCount", config.GetSessionLoadBalancingThreadCount());
			SetRtmpPushPublisher(app_info, object, "rtmpPush", config.GetRtmpPushPublisher(), Optional::True);
			SetHlsPublisher(app_info, object, "hls", config.GetHlsPublisher(), Optional::True);
			SetDashPublisher(app_info, object, "dash", config.GetDashPublisher(), Optional::True);
			SetLlDashPublisher(app_info, object, "llDash", config.GetLlDashPublisher(), Optional::True);
			SetWebrtcPublisher(app_info, object, "webrtc", config.GetWebrtcPublisher(), Optional::True);
			SetOvtPublisher(app_info, object, "ovt", config.GetOvtPublisher(), Optional::True);
			SetFilePublisher(app_info, object, "file", config.GetFilePublisher(), Optional::True);
			// TODO(soulk): Uncomment this line after implement cfg::ThumbnailPublisher
			// SetThumbnailPublisher(app_info, object, "thumbnail", config.GetThumbnailPublisher(), Optional::True);
		}

		static void SetEncodes(Json::Value &parent_object, const char *key, const cfg::vhost::app::oprf::Encodes &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			Json::Value audios(Json::ValueType::arrayValue);
			Json::Value videos(Json::ValueType::arrayValue);

			for (auto &profile : config.GetAudioProfileList())
			{
				Json::Value audio;

				if (profile.IsBypass())
				{
					SetBool(audio, "bypass", profile.IsBypass());
				}
				else
				{
					SetBool(audio, "active", profile.IsActive());
					SetString(audio, "codec", profile.GetCodec(), Optional::False);
					SetString(audio, "bitrate", profile.GetBitrateString(), Optional::False);
					SetInt(audio, "samplerate", profile.GetSamplerate());
					SetInt(audio, "channel", profile.GetChannel());
				}

				audios.append(audio);
			}

			for (auto &profile : config.GetVideoProfileList())
			{
				Json::Value video;

				if (profile.IsBypass())
				{
					SetBool(video, "bypass", profile.IsBypass());
				}
				else
				{
					SetBool(video, "active", profile.IsActive());
					SetString(video, "codec", profile.GetCodec(), Optional::False);
					SetString(video, "scale", profile.GetScale(), Optional::False);
					SetInt(video, "width", profile.GetWidth());
					SetInt(video, "height", profile.GetHeight());
					SetString(video, "bitrate", profile.GetBitrateString(), Optional::False);
					SetFloat(video, "framerate", profile.GetFramerate());
				}

				videos.append(video);
			}

			object["audios"] = audios;
			object["videos"] = videos;
		}

		Json::Value JsonFromOutputProfile(const cfg::vhost::app::oprf::OutputProfile &output_profile)
		{
			Json::Value item;

			SetString(item, "name", output_profile.GetName(), Optional::False);
			SetString(item, "outputStreamName", output_profile.GetOutputStreamName(), Optional::False);
			SetEncodes(item, "encodes", output_profile.GetEncodes(), Optional::True);

			return std::move(item);
		}

		static void SetOutputProfiles(const info::Application &app_info, Json::Value &parent_object, const char *key, const cfg::vhost::app::oprf::OutputProfiles &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &output_profile : config.GetOutputProfileList())
			{
				object.append(JsonFromOutputProfile(output_profile));
			}
		}

		Json::Value JsonFromApplication(const std::shared_ptr<const mon::ApplicationMetrics> &application)
		{
			Json::Value response(Json::ValueType::objectValue);

			auto &config = application->GetConfig();
			const auto &app_info = std::static_pointer_cast<const info::Application>(application);

			SetString(response, "name", application->GetName().GetAppName(), Optional::False);
			SetBool(response, "dynamic", application->IsDynamicApp());
			SetString(response, "type", config.GetTypeString(), Optional::False);
			SetOutputProfiles(*app_info, response, "outputProfiles", config.GetOutputProfiles(), Optional::True);
			SetProviders(*app_info, response, "providers", config.GetProviders(), Optional::True);
			SetPublishers(*app_info, response, "publishers", config.GetPublishers(), Optional::True);

			return std::move(response);
		}

		void MakeUpperCase(const ov::String &path, const Json::Value &input, Json::Value *output)
		{
			switch (input.type())
			{
				case Json::ValueType::nullValue:
					[[fallthrough]];
				case Json::ValueType::intValue:
					[[fallthrough]];
				case Json::ValueType::uintValue:
					[[fallthrough]];
				case Json::ValueType::realValue:
					[[fallthrough]];
				case Json::ValueType::stringValue:
					[[fallthrough]];
				case Json::ValueType::booleanValue:
					*output = input;
					break;

				case Json::ValueType::arrayValue:
					*output = Json::arrayValue;

					for (auto item : input)
					{
						Json::Value converted;

						MakeUpperCase(path, item, &converted);

						output->append(converted);
					}
					break;

				case Json::ValueType::objectValue:
					*output = Json::objectValue;

					for (auto item = input.begin(); item != input.end(); ++item)
					{
						const auto &input_name = item.name();
						ov::String name = input_name.c_str();

						bool converted = false;

						if ((path == "application.providers") || (path == "application.publishers"))
						{
							// Some settings names in providers or publishers are in capital letters
							if (name == "webrtc")
							{
								name = "WebRTC";
								converted = true;
							}
							else if (name == "threadCount")
							{
								name = "ThreadCount";
								converted = true;
							}
							else if (name == "rtspPull")
							{
								name = "RTSPPull";
								converted = true;
							}
							else
							{
								name.MakeUpper();
								converted = true;
							}
						}
						else if ((path == "application.providers.mpegts"))
						{
							if (name == "streams")
							{
								name = "StreamMap";
								converted = true;
							}
						}
						else if ((path == "application.outputProfiles.encodes"))
						{
							if (name == "audios")
							{
								name = "Audio";
								converted = true;
							}
							else if (name == "videos")
							{
								name = "Video";
								converted = true;
							}
						}

						if (converted == false)
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