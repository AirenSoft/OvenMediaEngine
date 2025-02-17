//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "stream_actions_controller.h"
#include "../../../../../api_private.h"

#include <base/provider/application.h>
#include <modules/data_format/id3v2/id3v2.h>
#include <modules/data_format/id3v2/frames/id3v2_frames.h>
#include <modules/data_format/cue_event/cue_event.h>
#include <modules/data_format/amf_event/amf_event.h>
#include <modules/bitstream/h264/h264_sei.h>

namespace api
{
	namespace v1
	{
		void StreamActionsController::PrepareHandlers()
		{
			RegisterGet(R"()", &StreamActionsController::OnGetDummyAction);

			RegisterPost(R"((hlsDumps))", &StreamActionsController::OnPostHLSDumps);
			RegisterPost(R"((startHlsDump))", &StreamActionsController::OnPostStartHLSDump);
			RegisterPost(R"((stopHlsDump))", &StreamActionsController::OnPostStopHLSDump);
			RegisterPost(R"((sendEvent))", &StreamActionsController::OnPostSendEvent);
			RegisterPost(R"((sendEvents))", &StreamActionsController::OnPostSendEvents);

			RegisterPost(R"((concludeHlsLive))", &StreamActionsController::OnPostConcludeHlsLive);
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:hlsDumps
		// {
		//  "outputStreamName": "stream",
		//  "id": "dump_id"
		// }
		ApiResponse StreamActionsController::OnPostHLSDumps(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
									const std::shared_ptr<mon::HostMetrics> &vhost,
									const std::shared_ptr<mon::ApplicationMetrics> &app,
									const std::shared_ptr<mon::StreamMetrics> &stream,
									const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			auto dump_info = ::serdes::DumpInfoFromJson(request_body);
			if (dump_info == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}

			if (dump_info->GetStreamName().IsEmpty() == true)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "outputStreamName is required");
			}

			auto output_stream_it = std::find_if(output_streams.begin(), output_streams.end(), [&](const auto &output_stream) {
				return output_stream->GetName() == dump_info->GetStreamName();
			});

			if (output_stream_it == output_streams.end())
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find output stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto output_stream = *output_stream_it;
			auto llhls_stream = GetOutputStream<LLHlsStream>(output_stream, PublisherType::LLHls);
			if (llhls_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find LLHLS stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			Json::Value response = Json::Value(Json::arrayValue);
			std::vector<std::shared_ptr<const mdl::Dump>> dumps;

			if (dump_info->GetId().IsEmpty() == true)
			{
				dumps = llhls_stream->GetDumpInfoList();
			}
			else
			{
				auto info = llhls_stream->GetDumpInfo(dump_info->GetId());
				if (info != nullptr)
				{
					dumps.push_back(info);
				}
			}

			for (auto &dump : dumps)
			{
				response.append(::serdes::JsonFromDumpInfo(dump));
			}
			
			return response;
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:startHlsDump
		// {
		// 	"outputStreamName": "stream",
		// 	"id": "dump_id",
		// 	"outputPath": "/tmp/",
		// 	"playlist" : ["llhls.m3u8", "abr.m3u8"],
		// 	"userData":{
		// 		"fileName" : "s3.info",
		// 		"fileData" : "s3://id:password@bucket_name/obj_name"
		//	}
		// }
		ApiResponse StreamActionsController::OnPostStartHLSDump(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app,
										const std::shared_ptr<mon::StreamMetrics> &stream,
										const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			auto dump_info = ::serdes::DumpInfoFromJson(request_body);
			if (dump_info == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}

			if (dump_info->GetId().IsEmpty() == true || 
				dump_info->GetStreamName().IsEmpty() == true ||
				dump_info->GetOutputPath().IsEmpty() == true)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "id, outputStreamName and outputPath are required");
			}

			auto output_stream_it = std::find_if(output_streams.begin(), output_streams.end(), [&](const auto &output_stream) {
				return output_stream->GetName() == dump_info->GetStreamName();
			});

			if (output_stream_it == output_streams.end())
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find output stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto output_stream = *output_stream_it;
			auto llhls_stream = GetOutputStream<LLHlsStream>(output_stream, PublisherType::LLHls);
			if (llhls_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find LLHLS stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto [result, reason] = llhls_stream->StartDump(dump_info);
			if (result == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError,
									  "Could not start dump: [%s/%s/%s] (%s)",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr(), reason.CStr());
			}

			return {http::StatusCode::OK};
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:stopHlsDump
		// {
		//  "outputStreamName": "stream",
		//  "id": "dump_id"
		// }
		ApiResponse StreamActionsController::OnPostStopHLSDump(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app,
										const std::shared_ptr<mon::StreamMetrics> &stream,
										const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			auto dump_info = ::serdes::DumpInfoFromJson(request_body);
			if (dump_info == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Could not parse json context: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}

			if (dump_info->GetStreamName().IsEmpty() == true)
			{
				throw http::HttpError(http::StatusCode::BadRequest,
									  "outputStreamName is required");
			}

			auto output_stream_it = std::find_if(output_streams.begin(), output_streams.end(), [&](const auto &output_stream) {
				return output_stream->GetName() == dump_info->GetStreamName();
			});

			if (output_stream_it == output_streams.end())
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find output stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto output_stream = *output_stream_it;
			auto llhls_stream = GetOutputStream<LLHlsStream>(output_stream, PublisherType::LLHls);
			if (llhls_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not find LLHLS stream: [%s/%s/%s]",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr());
			}

			auto [result, reason] = llhls_stream->StopDump(dump_info);
			if (result == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError,
									  "Could not stop dump: [%s/%s/%s] (%s)",
									  vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), dump_info->GetStreamName().CStr(), reason.CStr());
			}

			return {http::StatusCode::OK};
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:injectHLSEvent
		ApiResponse StreamActionsController::OnPostSendEvents(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app,
										const std::shared_ptr<mon::StreamMetrics> &stream,
										const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			if (request_body.isArray() == false || request_body.size() == 0)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "events(array) is required");
			}

			MultipleStatus status_codes;
			Json::Value response_value(Json::ValueType::arrayValue);
			for (const auto &request_event : request_body)
			{
				try
				{
					OnPostSendEvent(client, request_event, vhost, app, stream, output_streams);
					status_codes.AddStatusCode(http::StatusCode::OK);

					Json::Value response;
					response["statusCode"] = static_cast<int>(http::StatusCode::OK);
					response["message"] = StringFromStatusCode(http::StatusCode::OK);
					response_value.append(response);
				}
				catch (const http::HttpError &error)
				{
					status_codes.AddStatusCode(error.GetStatusCode());
					response_value.append(::serdes::JsonFromError(error));
				}
			}

			return {status_codes, std::move(response_value)};
		}

		// POST /v1/vhosts/<vhost_name>/apps/<app_name>/streams/<stream_name>:injectHLSEvent
		ApiResponse StreamActionsController::OnPostSendEvent(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
										const std::shared_ptr<mon::HostMetrics> &vhost,
										const std::shared_ptr<mon::ApplicationMetrics> &app,
										const std::shared_ptr<mon::StreamMetrics> &stream,
										const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			// Validate request body

			// ID3 Event
			// {
			//   "eventFormat": "id3v2",
			//   "eventType": "video",
			//	 "urgent": false,
			//   "events":[
			//       {
			//         "frameType": "TXXX",
			//         "info": "AirenSoft",
			//         "data": "OvenMediaEngine"
			//       },
			//       {
			//         "frameType": "TXXX",
			//         "info": "AirenSoft",
			//         "data": "OvenMediaEngine"
			//       },
			//		 {
			//         "frameType": "PRIV",
			//         "info": "AirenSoft",
			//         "data": "OvenMediaEngine"
			//       }
			//   ]
			// }

			// Cue Event
			// {
			// 	"eventFormat": "cue",
			// 	"events":[
			// 		{
			// 			"cueType": "out", // out | in
			// 			"duration": 60500 // milliseconds, only available when cueType is out
			// 		}
			// 	]
			// }

			// SEI Event
			// {
			// 	"eventFormat": "sei",
			// 	"eventType" : "video",
			// 	"urgent" : true,
			// 	"events": [
			// 		{
			// 			"seiType" : "UserDataUnregistered",
			// 			"data": "Hello! Digitia1212n!"  
			// 		}
			// 	]
			// }


			if (request_body.isMember("eventFormat") == false || request_body["eventFormat"].isString() == false ||
				request_body.isMember("events") == false || request_body["events"].isArray() == false || request_body["events"].size() == 0)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "eventFormat(string) and events(array) are required");
			}

			cmn::BitstreamFormat event_format = cmn::BitstreamFormat::Unknown;
			ov::String event_format_string = request_body["eventFormat"].asString().c_str();
			std::shared_ptr<ov::Data> events_data = nullptr;

			if (event_format_string.UpperCaseString() == "ID3V2")
			{
				event_format = cmn::BitstreamFormat::ID3v2;
				events_data = MakeID3Data(request_body["events"]);
			}
			else if (event_format_string.UpperCaseString() == "CUE")
			{
				event_format = cmn::BitstreamFormat::CUE;
				events_data = MakeCueData(request_body["events"]);
			}
			else
			{
				throw http::HttpError(http::StatusCode::BadRequest, "eventFormat is not supported: [%s]", event_format_string.CStr());
			}

			if (events_data == nullptr)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "Could not make events data");
			}

			// Event Type (Optional)
			cmn::PacketType event_type = cmn::PacketType::EVENT;
			if (request_body.isMember("eventType") == true)
			{
				auto event_type_json = request_body["eventType"];
				if (event_type_json.isString() == false)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "eventType must be string");
				}

				ov::String event_type_string = event_type_json.asString().c_str();
				if (event_type_string.UpperCaseString() == "VIDEO")
				{
					event_type = cmn::PacketType::VIDEO_EVENT;
				}
				else if (event_type_string.UpperCaseString() == "AUDIO")
				{
					event_type = cmn::PacketType::AUDIO_EVENT;
				}
				else if (event_type_string.UpperCaseString() == "EVENT")
				{
					event_type = cmn::PacketType::EVENT;
				}
				else
				{
					throw http::HttpError(http::StatusCode::BadRequest, "eventType is not supported: [%s]", event_type_string.CStr());
				}
			}

			// Urgent (Optional)
			bool urgent = false;
			if (request_body.isMember("urgent") == true && request_body["urgent"].isBool() == true)
			{
				urgent = request_body["urgent"].asBool();
			}

			auto source_stream = GetSourceStream(stream);
			if (source_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									"Could not find stream: [%s/%s/%s]",
									vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}

			if (source_stream->SendDataFrame(-1, event_format, event_type, events_data, urgent) == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError,
									"Internal Server Error - Could not inject event: [%s/%s/%s]",
									vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}
			return {http::StatusCode::OK};
		}

		ApiResponse StreamActionsController::OnPostConcludeHlsLive(const std::shared_ptr<http::svr::HttpExchange> &client, const Json::Value &request_body,
											   const std::shared_ptr<mon::HostMetrics> &vhost,
											   const std::shared_ptr<mon::ApplicationMetrics> &app,
											   const std::shared_ptr<mon::StreamMetrics> &stream,
											   const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			/*
			{
				"urgent": true,
			}
			*/

			auto source_stream = GetSourceStream(stream);
			if (source_stream == nullptr)
			{
				throw http::HttpError(http::StatusCode::NotFound,
									"Could not find stream: [%s/%s/%s]",
									vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}

			bool urgent = false;
			if (request_body.isMember("urgent") == true && request_body["urgent"].isBool() == true)
			{
				urgent = request_body["urgent"].asBool();
			}

			auto media_event = std::make_shared<MediaEvent>(MediaEvent::CommandType::ConcludeLive, nullptr);
			media_event->SetHighPriority(urgent);

			if (source_stream->SendEvent(media_event) == false)
			{
				throw http::HttpError(http::StatusCode::InternalServerError,
									"Internal Server Error - Could not inject ConcludeLive event: [%s/%s/%s]",
									vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());
			}
			
			return {http::StatusCode::OK};
		}

		ApiResponse StreamActionsController::OnGetDummyAction(const std::shared_ptr<http::svr::HttpExchange> &client,
														   const std::shared_ptr<mon::HostMetrics> &vhost,
														   const std::shared_ptr<mon::ApplicationMetrics> &app,
														   const std::shared_ptr<mon::StreamMetrics> &stream,
														   const std::vector<std::shared_ptr<mon::StreamMetrics>> &output_streams)
		{
			logte("Called OnGetDummyAction. invoke [%s/%s/%s]", vhost->GetName().CStr(), app->GetVHostAppName().GetAppName().CStr(), stream->GetName().CStr());

			return app->GetConfig().ToJson();
		}

		std::shared_ptr<pvd::Stream> StreamActionsController::GetSourceStream(const std::shared_ptr<mon::StreamMetrics> &stream)
		{
			// Get PrivderType from SourceType
			ProviderType provider_type = stream->GetProviderType();
			if(provider_type == ProviderType::Unknown)
			{
				return nullptr;
			}
			

			auto provider = std::dynamic_pointer_cast<pvd::Provider>(ocst::Orchestrator::GetInstance()->GetProviderFromType(provider_type));
			if (provider == nullptr)
			{
				return nullptr;
			}

			auto application = provider->GetApplicationByName(stream->GetApplicationInfo().GetVHostAppName());
			if (application == nullptr)
			{
				return nullptr;
			}

			return application->GetStreamByName(stream->GetName());
		}

		std::shared_ptr<ov::Data> StreamActionsController::MakeID3Data(const Json::Value &events)
		{
			// Make ID3v2 tags
			auto id3v2_event = std::make_shared<ID3v2>();
			id3v2_event->SetVersion(4, 0);
			for (const auto &event : events)
			{
				if (event.isMember("frameType") == false || event["frameType"].isString() == false)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "frameType is required in events");
				}

				ov::String frame_type = event["frameType"].asString().c_str();
				ov::String info;
				ov::String data;

				if (event.isMember("info") == true && event["info"].isString() == true)
				{
					info = event["info"].asString().c_str();
				}

				if (event.isMember("data") == true && event["data"].isString() == true)
				{
					data = event["data"].asString().c_str();
				}

				std::shared_ptr<ID3v2Frame> frame;
				if (frame_type.UpperCaseString() == "TXXX")
				{
					frame = std::make_shared<ID3v2TxxxFrame>(info, data);
				}
				else if (frame_type.UpperCaseString().Get(0) == 'T')
				{
					frame = std::make_shared<ID3v2TextFrame>(frame_type, data);
				}
				else if (frame_type.UpperCaseString() == "PRIV")
				{
					frame = std::make_shared<ID3v2PrivFrame>(info, data);
				}
				else
				{
					throw http::HttpError(http::StatusCode::BadRequest, "frameType is not supported: [%s]", frame_type.CStr());
				}

				id3v2_event->AddFrame(frame);
			}

			return id3v2_event->Serialize();
		}

		std::shared_ptr<ov::Data> StreamActionsController::MakeCueData(const Json::Value &events)
		{
			if (events.size() == 0)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "events must have at least one event");
			}

			// only first event is used
			auto event = events[0];
			if (event.isMember("cueType") == false || event["cueType"].isString() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "cueType is required in events");
			}

			ov::String cue_type = event["cueType"].asString().c_str();
			CueEvent::CueType type = CueEvent::GetCueTypeByName(cue_type);
			if (type == CueEvent::CueType::Unknown)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "cueType is not supported: [%s]", cue_type.CStr());
			}

			// duration (optional)
			uint32_t duration_msec = 0;
			if (event.isMember("duration") == true && event["duration"].isUInt() == true)
			{
				duration_msec = event["duration"].asUInt();
			}

			auto cue_event = CueEvent::Create(type, duration_msec);

			return cue_event->Serialize();
		}

		std::shared_ptr<ov::Data> StreamActionsController::MakeAMFData(const Json::Value &events)
		{
			if (events.size() == 0)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "events must have at least one event");
			}

			// only first event is used
			auto event = events[0];
			if (event.isMember("amfType") == false || event["amfType"].isString() == false)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "amfType is required in events");
			}

			ov::String amf_type = event["amfType"].asString().c_str();

			if (AmfTextDataEvent::IsMatch(amf_type))
			{
				if(AmfTextDataEvent::IsValid(event) == false)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "data is required in events");
				}

				auto amf_event = AmfTextDataEvent::Parse(event);
				if (amf_event == nullptr)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "Could not parse amf data");
				}

				return amf_event->Serialize();
			}
			else if (AmfCuePointEvent::IsMatch(amf_type))
			{
				if(AmfCuePointEvent::IsValid(event) == false)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "version, preRollTimeSec is required in events");
				}
				
				auto amf_event = AmfCuePointEvent::Parse(event);
				if (amf_event == nullptr)
				{
					throw http::HttpError(http::StatusCode::BadRequest, "Could not parse amf data");
				}

				return amf_event->Serialize();
			}

			return nullptr;
		}

		std::shared_ptr<ov::Data> StreamActionsController::MakeSEIData(const Json::Value &events)
		{
			if (events.size() == 0)
			{
				throw http::HttpError(http::StatusCode::BadRequest, "events must have at least one event");
			}

			// only first event is used
			auto event = events[0];
			H264SEI::PayloadType payload_type = H264SEI::PayloadType::USER_DATA_UNREGISTERED;
			if (event.isMember("seiType") == true && event["seiType"].isString() == true)
			{
				payload_type = H264SEI::StringToPayloadType(ov::String(event["seiType"].asString().c_str()));
			}

			// data (optional)
			std::shared_ptr<ov::Data> payload_data = nullptr;
			if (event.isMember("data") == true && event["data"].isString() == true)
			{
				payload_data = std::make_shared<ov::Data>(event["data"].asString().c_str(), event["data"].asString().size());
			}
			else
			{
				throw http::HttpError(http::StatusCode::BadRequest, "data is required in events");
			}

			auto current_eopch_time = H264SEI::GetCurrentEpochTimeToTimeCode();

			auto sei_event = std::make_shared<H264SEI>();
			sei_event->SetPayloadType(payload_type);
			sei_event->SetPayloadTimeCode(current_eopch_time);
			sei_event->SetPayloadData(payload_data);

			logtd("%s", sei_event->GetInfoString().CStr());

			return sei_event->Serialize();
		}
	} // namespace v1
} // namespace api