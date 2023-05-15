//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../stream_metrics.h"
#include "base/ovlibrary/ovlibrary.h"
#include "config/config.h"
#include "message.h"

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

			void VerifyIngressRules(const ov::String &source_uri, cfg::alrt::rule::Rules rules, const std::shared_ptr<StreamMetrics> &stream_metric, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list);
			void VerifyVideoIngressRules(const ov::String &source_uri, cfg::alrt::rule::Ingress ingress, const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list);
			void VerifyAudioIngressRules(const ov::String &source_uri, cfg::alrt::rule::Ingress ingress, const std::shared_ptr<MediaTrack> &audio_track, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list);

			bool IsAlertNeeded(const ov::String &source_uri, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list);

			void CleanupDeletedSources(const std::vector<ov::String> *exist_source_uri_list);
			bool PutSourceUriMessages(const ov::String &source_uri, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list);
			bool RemoveSourceUriMessages(const ov::String &source_uri);
			std::shared_ptr<std::vector<std::shared_ptr<Message>>> GetSourceUriMessages(const ov::String &source_uri);

			std::shared_ptr<const cfg::Server> _server_config = nullptr;

			std::map<ov::String, std::shared_ptr<std::vector<std::shared_ptr<Message>>>> _source_uri_messages_map;
			std::map<ov::String, bool> _changed_map;

			ov::DelayQueue _timer{"MonAlert"};
		};
	}  // namespace alrt
}  // namespace mon