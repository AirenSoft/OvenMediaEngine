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
				STREAM,
				INGRESS,
				INTERNAL_QUEUE
			};

			static ov::String StringFromType(Type type)
			{
				switch (type)
				{
					case Type::STREAM:
						return "STREAM";
					case Type::INGRESS:
						return "INGRESS";
					case Type::INTERNAL_QUEUE:
						return "INTERNAL_QUEUE";
				}

				return "INGRESS";
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