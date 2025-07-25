//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../queue_metrics.h"
#include "../stream_metrics.h"
#include "message.h"

namespace mon
{
	namespace alrt
	{
		class NotificationData
		{
		public:
			enum class Type : uint16_t
			{
				INGRESS,
				EGRESS,
				INTERNAL_QUEUE
			};

			static ov::String StringFromType(Type type)
			{
				switch (type)
				{
					case Type::INGRESS:
						return "INGRESS";
					case Type::EGRESS:
						return "EGRESS";
					case Type::INTERNAL_QUEUE:
						return "INTERNAL_QUEUE";
				}

				return "INGRESS";
			}

			static Type TypeFromMessageCode(Message::Code code)
			{
				switch (code)
				{
					case Message::Code::INGRESS_STREAM_CREATED:
					case Message::Code::INGRESS_STREAM_PREPARED:
					case Message::Code::INGRESS_STREAM_DELETED:
					case Message::Code::INGRESS_STREAM_CREATION_FAILED_DUPLICATE_NAME:
					case Message::Code::INGRESS_BITRATE_LOW:
					case Message::Code::INGRESS_BITRATE_HIGH:
					case Message::Code::INGRESS_FRAMERATE_LOW:
					case Message::Code::INGRESS_FRAMERATE_HIGH:
					case Message::Code::INGRESS_WIDTH_SMALL:
					case Message::Code::INGRESS_WIDTH_LARGE:
					case Message::Code::INGRESS_HEIGHT_SMALL:
					case Message::Code::INGRESS_HEIGHT_LARGE:
					case Message::Code::INGRESS_SAMPLERATE_LOW:
					case Message::Code::INGRESS_SAMPLERATE_HIGH:
					case Message::Code::INGRESS_LONG_KEY_FRAME_INTERVAL:
					case Message::Code::INGRESS_HAS_BFRAME:
						return Type::INGRESS;
					case Message::Code::EGRESS_STREAM_CREATED:
					case Message::Code::EGRESS_STREAM_PREPARED:
					case Message::Code::EGRESS_STREAM_DELETED:
					case Message::Code::EGRESS_LLHLS_READY:
					case Message::Code::EGRESS_HLS_READY:
						return Type::EGRESS;
					case Message::Code::INTERNAL_QUEUE_CONGESTION:
					default:
						return Type::INTERNAL_QUEUE;
				}
			}

			NotificationData(const Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const ov::String source_uri, const std::shared_ptr<StreamMetrics> &stream_metric);
			NotificationData(const Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const std::map<uint32_t, std::shared_ptr<QueueMetrics>> &queue_metric_list);
			ov::String ToJsonString() const;

		private:
			Type _type;
			std::vector<std::shared_ptr<Message>> _message_list = {};

			ov::String _source_uri;
			std::shared_ptr<StreamMetrics> _stream_metric = nullptr;

			std::map<uint32_t, std::shared_ptr<QueueMetrics>> _queue_metric_list;
		};
	}  // namespace alrt
}  // namespace mon