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
#include "base/ovlibrary/ovlibrary.h"
#include "config/config.h"
#include "message.h"
#include "notification_data.h"

namespace mon
{
	namespace alrt
	{
		class Alert
		{
		public:
			~Alert();

			bool Start(const std::shared_ptr<const cfg::Server> &server_config);
			bool Stop();

		private:
			void DispatchThreadProc();

			bool VerifyQueueCongestionRules(const cfg::alrt::rule::Rules &rules, const std::shared_ptr<QueueMetrics> &queue_metric, std::vector<std::shared_ptr<Message>> &message_list);
			void VerifyIngressRules(const cfg::alrt::rule::Rules &rules, const std::shared_ptr<StreamMetrics> &stream_metric, std::vector<std::shared_ptr<Message>> &message_list);
			void VerifyVideoIngressRules(const cfg::alrt::rule::Ingress &ingress, const std::shared_ptr<MediaTrack> &video_track, std::vector<std::shared_ptr<Message>> &message_list);
			void VerifyAudioIngressRules(const cfg::alrt::rule::Ingress &ingress, const std::shared_ptr<MediaTrack> &audio_track, std::vector<std::shared_ptr<Message>> &message_list);

			bool IsAlertNeeded(const ov::String &messages_key, const std::vector<std::shared_ptr<Message>> &message_list);
			void SendNotification(const NotificationData &notificationData);
			void SendNotification(const NotificationData::Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const ov::String &source_uri, const std::shared_ptr<StreamMetrics> &stream_metric);
			void SendNotification(const NotificationData::Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const std::map<uint32_t, std::shared_ptr<QueueMetrics>> &queue_metric_list);

			void CleanupReleasedMessages(const std::vector<ov::String> &new_messages_keys);
			bool PutVerifiedMessages(const ov::String &messages_key, std::vector<std::shared_ptr<Message>> &message_list);
			bool RemoveVerifiedMessages(const ov::String &messages_key);
			std::vector<std::shared_ptr<Message>> GetVerifiedMessages(const ov::String &messages_key);

			std::shared_ptr<const cfg::Server> _server_config = nullptr;

			std::map<ov::String, std::vector<std::shared_ptr<Message>>> _last_verified_messages_map;

			ov::DelayQueue _timer{"MonAlert"};
		};
	}  // namespace alrt
}  // namespace mon