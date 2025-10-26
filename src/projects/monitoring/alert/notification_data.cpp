//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "notification_data.h"

#include <modules/json_serdes/converters.h>

namespace mon
{
	namespace alrt
	{
		NotificationData::NotificationData(const Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const ov::String source_uri, const std::shared_ptr<StreamMetrics> &stream_metric)
		{
			_type		   = type;
			_message_list  = message_list;
			_source_uri	   = source_uri;
			_stream_metric = stream_metric;
		}

		NotificationData::NotificationData(const Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const std::map<uint32_t, std::shared_ptr<QueueMetrics>> &queue_metric_list)
		{
			_type			   = type;
			_message_list	   = message_list;
			_queue_metric_list = queue_metric_list;
		}

		ov::String NotificationData::ToJsonString() const
		{
			// Make request message
			Json::Value jv_root;

			// Type
			jv_root["type"] = StringFromType(_type).CStr();

			// Messages
			Json::Value jv_messages;
			if (_message_list.size() <= 0)
			{
				// If the message_list is empty, it means that the status of the stream has become normal, so send an OK message.

				Json::Value jv_message;

				jv_message["code"]		  = Message::StringFromMessageCode(Message::Code::OK).CStr();
				jv_message["description"] = Message::DescriptionFromMessageCode<bool>(Message::Code::OK, true, true).CStr();

				jv_messages.append(jv_message);
			}
			else
			{
				for (auto &message : _message_list)
				{
					Json::Value jv_message;

					jv_message["code"]		  = Message::StringFromMessageCode(message->GetCode()).CStr();
					jv_message["description"] = message->GetDescription().CStr();

					jv_messages.append(jv_message);
				}
			}
			jv_root["messages"] = jv_messages;

			// Source uri
			if (!_source_uri.IsEmpty())
			{
				jv_root["sourceUri"] = _source_uri.CStr();
			}

			// Metric infos
			if (_stream_metric != nullptr)
			{
				Json::Value jv_source_info = ::serdes::JsonFromStream(_stream_metric);
				jv_root["sourceInfo"]	   = jv_source_info;
			}

			if (_queue_metric_list.size() > 0)
			{
				Json::Value jv_queues;

				for (const auto &[queue_key, queue_metric] : _queue_metric_list)
				{
					Json::Value jv_queue = ::serdes::JsonFromQueueMetrics(queue_metric);

					jv_queues.append(jv_queue);
				}

				jv_root["internalQueues"] = jv_queues;
			}

			if (_output_profile != nullptr)
			{
				Json::Value jv_output_profile = ::serdes::JsonFromOutputProfile(*_output_profile);
				jv_root["outputProfile"] = jv_output_profile;
			}

			if (_codec_modules.size() > 0)
			{
				Json::Value jv_codec_modules;

				for (const auto &codec_module : _codec_modules)
				{
					Json::Value jv_codec_module = ::serdes::JsonFromCodecModule(codec_module);
					jv_codec_modules.append(jv_codec_module);
				}

				jv_root["codecModules"] = jv_codec_modules;
			}

			return ov::Converter::ToString(jv_root);
		}
	}  // namespace alrt
}  // namespace mon