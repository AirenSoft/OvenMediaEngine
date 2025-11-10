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
#include "alert_rules_updater.h"
#include "base/ovlibrary/ovlibrary.h"
#include "config/config.h"
#include "message.h"
#include "notification_data.h"

namespace mon::alrt
{
	class Alert
	{
	public:
		~Alert();

		bool Start(const std::shared_ptr<const cfg::Server> &server_config);
		bool Stop();

		void SendStreamMessage(Message::Code code, const std::shared_ptr<StreamMetrics> &stream_metric, const std::shared_ptr<StreamMetrics> &parent_source_info, const std::shared_ptr<cfg::vhost::app::oprf::OutputProfile> &output_profile, const std::vector<std::shared_ptr<info::CodecModule>> &codec_modules);
		void SendStreamMessage(Message::Code code, const std::shared_ptr<StreamMetrics> &stream_metric);

	private:
		void MetricWorkerThread();
		void EventWorkerThread();

		bool VerifyStreamEventRule(const cfg::alrt::rule::Rules &rules, Message::Code code);

		bool VerifyQueueCongestionRules(const cfg::alrt::rule::Rules &rules, const std::shared_ptr<QueueMetrics> &queue_metric, std::vector<std::shared_ptr<Message>> &message_list);
		void VerifyIngressMetricRules(const cfg::alrt::rule::Rules &rules, const std::shared_ptr<StreamMetrics> &stream_metric, std::vector<std::shared_ptr<Message>> &message_list);
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
		std::shared_ptr<AlertRulesUpdater> _rules_updater = nullptr;

		std::map<ov::String, std::vector<std::shared_ptr<Message>>> _last_verified_messages_map;

		ov::DelayQueue _timer{"MonAlert"};

		struct StreamEvent
		{
			StreamEvent(Message::Code code, const std::shared_ptr<StreamMetrics> &stream_metric, const std::shared_ptr<StreamMetrics> &parent_source_info, const std::shared_ptr<cfg::vhost::app::oprf::OutputProfile> &output_profile, const std::vector<std::shared_ptr<info::CodecModule>> &codec_modules)
			{
				_code				= code;
				_metric				= stream_metric;
				_parent_source_info = parent_source_info;
				_output_profile		= output_profile;
				_codec_modules		= codec_modules;
			}

			StreamEvent(Message::Code code, const std::shared_ptr<StreamMetrics> &stream_metric)
			{
				_code	= code;
				_metric = stream_metric;
			}

			Message::Code _code;
			std::shared_ptr<StreamMetrics> _metric;
			std::shared_ptr<StreamMetrics> _parent_source_info					  = nullptr;
			std::shared_ptr<cfg::vhost::app::oprf::OutputProfile> _output_profile = nullptr;
			std::vector<std::shared_ptr<info::CodecModule>> _codec_modules;
		};

		ov::Semaphore _queue_event;

		std::shared_ptr<StreamEvent> PopStreamEvent();
		ov::Queue<std::shared_ptr<StreamEvent>> _stream_event_queue;

		std::atomic<bool> _stop_thread_flag{true};
		std::thread _event_worker_thread;
	};
}  // namespace mon::alrt
