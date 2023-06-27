//=============================================================================
//
//	OvenMediaEngine
//
//	Created by Gilhoon Choi
//	Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "alert.h"

#include "../monitoring_private.h"
#include "monitoring/monitoring.h"
#include "notification.h"

#define LONG_KEY_FRAME_INTERVAL_SIZE 4.0

namespace mon
{
	namespace alrt
	{
		Alert::~Alert()
		{
			Stop();
		}

		bool Alert::Start(const std::shared_ptr<const cfg::Server> &server_config)
		{
			if (server_config == nullptr)
			{
				return false;
			}

			auto alert = server_config->GetAlert();

			if (alert.IsParsed() == false)
			{
				// Doesn't use the Alert feature.
				return false;
			}

			auto notification_server_url = ov::Url::Parse(alert.GetUrl());
			if(notification_server_url == nullptr)
			{
				logte("Could not parse notification url: %s", alert.GetUrl().CStr());
				return false;
			}

			_server_config = server_config;

			_timer.Push(
				[this](void *paramter) -> ov::DelayQueueAction {
					DispatchThreadProc();
					return ov::DelayQueueAction::Repeat;
				},
				100);
			_timer.Start();

			return true;
		}

		bool Alert::Stop()
		{
			_timer.Stop();

			return true;
		}

		void Alert::DispatchThreadProc()
		{
			auto alert = _server_config->GetAlert();
			auto rules = alert.GetRules();

			NotificationData::Type type;
			ov::String messages_key;
			std::vector<std::shared_ptr<Message>> message_list;
			std::vector<ov::String> new_messages_keys;

			{
				// Check Internal queues
				
				type = NotificationData::Type::INTERNAL_QUEUE;

				messages_key = NotificationData::StringFromType(type);
				new_messages_keys.push_back(messages_key);

				const auto &queue_metric_list = MonitorInstance->GetServerMetrics()->GetQueueMetricsList();
				for (const auto &[queue_key, queue_metric] : queue_metric_list)
				{
					if (!VerifyQueueCongestionRules(rules, queue_metric, message_list))
					{
						break;
					}
				}

				if (IsAlertNeeded(messages_key, message_list))
				{
					SendNotification(type, message_list, queue_metric_list);
				}

				PutVerifiedMessages(messages_key, message_list);
			}

			{
				// Check streams

				for (const auto &[host_key, host_metric] : MonitorInstance->GetHostMetricsList())
				{
					for (const auto &[app_key, app_metric] : host_metric->GetApplicationMetricsList())
					{
						for (const auto &[stream_key, stream_metric] : app_metric->GetStreamMetricsMap())
						{
							message_list.clear();

							if (stream_metric->IsInputStream())
							{
								type = NotificationData::Type::INGRESS;

								messages_key = stream_metric->GetUri();
								new_messages_keys.push_back(messages_key);

								VerifyIngressRules(rules, stream_metric, message_list);

								if (IsAlertNeeded(messages_key, message_list))
								{
									SendNotification(type, message_list, stream_metric->GetUri(), stream_metric);
								}

								PutVerifiedMessages(messages_key, message_list);
							}
						}
					}
				}
			}

			CleanupReleasedMessages(new_messages_keys);
		}

		template <typename T>
		void AddNonOkMessage(std::vector<std::shared_ptr<Message>> &message_list, Message::Code code, T config_value, T measured_value)
		{
			if (code != Message::Code::OK)
			{
				ov::String description = Message::DescriptionFromMessageCode(code, config_value, measured_value);
				auto message = Message::CreateMessage(code, description);

				message_list.push_back(message);
			}
		}

		bool Alert::VerifyQueueCongestionRules(const cfg::alrt::rule::Rules &rules, const std::shared_ptr<QueueMetrics> &queue_metric, std::vector<std::shared_ptr<Message>> &message_list)
		{
			if (!rules.IsInternalQueueCongestion())
			{
				return true;
			}

			if ((queue_metric->GetThreshold()) > 0 && (queue_metric->GetSize() > queue_metric->GetThreshold()))
			{
				AddNonOkMessage<size_t>(message_list, Message::Code::INTERNAL_QUEUE_CONGESTION, queue_metric->GetThreshold(), queue_metric->GetSize());

				return false;
			}

			return true;
		}

		void Alert::VerifyIngressRules(const cfg::alrt::rule::Rules &rules, const std::shared_ptr<StreamMetrics> &stream_metric, std::vector<std::shared_ptr<Message>> &message_list)
		{
			auto ingress = rules.GetIngress();
			if (!ingress.IsParsed())
			{
				return;
			}

			int32_t totalBitrate = 0;

			for (auto &[track_id, track] : stream_metric->GetTracks())
			{
				if (track->GetMediaType() == cmn::MediaType::Video)
				{
					totalBitrate += track->GetBitrateByMeasured();
					VerifyVideoIngressRules(ingress, track, message_list);
				}
				else if (track->GetMediaType() == cmn::MediaType::Audio)
				{
					totalBitrate += track->GetBitrateByMeasured();
					VerifyAudioIngressRules(ingress, track, message_list);
				}
			}

			if (totalBitrate > 0)
			{
				// Verify MinBitrates
				if (ingress.GetMinBitrate() > 0)
				{
					if (totalBitrate < ingress.GetMinBitrate())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_BITRATE_LOW, ingress.GetMinBitrate(), totalBitrate);
					}
				}

				// Verify MaxBitrates
				if (ingress.GetMaxBitrate() > 0)
				{
					if (totalBitrate > ingress.GetMaxBitrate())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_BITRATE_HIGH, ingress.GetMaxBitrate(), totalBitrate);
					}
				}
			}
		}

		void Alert::VerifyVideoIngressRules(const cfg::alrt::rule::Ingress &ingress, const std::shared_ptr<MediaTrack> &video_track, std::vector<std::shared_ptr<Message>> &message_list)
		{
			// Verify HasBFrame
			if (ingress.GetHasBFrames())
			{
				if (video_track->HasBframes())
				{
					AddNonOkMessage<bool>(message_list, Message::Code::INGRESS_HAS_BFRAME, true, true);
				}
			}

			if (video_track->GetFrameRateByMeasured() > 0)
			{
				// Verify MinFramerate
				if (ingress.GetMinFramerate() > 0)
				{
					if (video_track->GetFrameRateByMeasured() < ingress.GetMinFramerate())
					{
						AddNonOkMessage<double>(message_list, Message::Code::INGRESS_FRAMERATE_LOW, ingress.GetMinFramerate(), video_track->GetFrameRateByMeasured());
					}
				}

				// Verify MaxFramerate
				if (ingress.GetMaxFramerate() > 0)
				{
					if (video_track->GetFrameRateByMeasured() > ingress.GetMaxFramerate())
					{
						AddNonOkMessage<double>(message_list, Message::Code::INGRESS_FRAMERATE_HIGH, ingress.GetMaxFramerate(), video_track->GetFrameRateByMeasured());
					}
				}

				// Verify LongKeyFrameInterval
				if (video_track->GetKeyFrameInterval() > 0 && ingress.IsLongKeyFrameInterval())
				{
					double interval = video_track->GetKeyFrameInterval() / video_track->GetFrameRateByMeasured();
					if (interval > LONG_KEY_FRAME_INTERVAL_SIZE)
					{
						AddNonOkMessage<double>(message_list, Message::Code::INGRESS_LONG_KEY_FRAME_INTERVAL, LONG_KEY_FRAME_INTERVAL_SIZE, interval);
					}
				}
			}

			if (video_track->GetWidth() > 0)
			{
				// Verify MinWidth
				if (ingress.GetMinWidth() > 0)
				{
					if (video_track->GetWidth() < ingress.GetMinWidth())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_WIDTH_SMALL, ingress.GetMinWidth(), video_track->GetWidth());
					}
				}

				// Verify MaxWidth
				if (ingress.GetMaxWidth() > 0)
				{
					if (video_track->GetWidth() > ingress.GetMaxWidth())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_WIDTH_LARGE, ingress.GetMaxWidth(), video_track->GetWidth());
					}
				}
			}

			if (video_track->GetHeight() > 0)
			{
				// Verify MinHeight
				if (ingress.GetMinHeight() > 0)
				{
					if (video_track->GetHeight() < ingress.GetMinHeight())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_HEIGHT_SMALL, ingress.GetMinHeight(), video_track->GetHeight());
					}
				}

				// Verify MaxHeight
				if (ingress.GetMaxHeight() > 0)
				{
					if (video_track->GetHeight() > ingress.GetMaxHeight())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_HEIGHT_LARGE, ingress.GetMaxHeight(), video_track->GetHeight());
					}
				}
			}
		}

		void Alert::VerifyAudioIngressRules(const cfg::alrt::rule::Ingress &ingress, const std::shared_ptr<MediaTrack> &audio_track, std::vector<std::shared_ptr<Message>> &message_list)
		{
			if (audio_track->GetSampleRate() > 0)
			{
				// Verify MinSamplerate
				if (ingress.GetMinSamplerate() > 0)
				{
					if (audio_track->GetSampleRate() < ingress.GetMinSamplerate())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_SAMPLERATE_LOW, ingress.GetMinSamplerate(), audio_track->GetSampleRate());
					}
				}

				// Verify MaxSamplerate
				if (ingress.GetMaxSamplerate() > 0)
				{
					if (audio_track->GetSampleRate() > ingress.GetMaxSamplerate())
					{
						AddNonOkMessage<int32_t>(message_list, Message::Code::INGRESS_SAMPLERATE_HIGH, ingress.GetMinSamplerate(), audio_track->GetSampleRate());
					}
				}
			}
		}

		bool Alert::IsAlertNeeded(const ov::String &messages_key, const std::vector<std::shared_ptr<Message>> &message_list)
		{
			auto last_verified_message_list = GetVerifiedMessages(messages_key);

			if (last_verified_message_list.size() == 0)
			{
				// New messages

				if (message_list.size() > 0)
				{
					return true;
				}
			}
			else
			{
				// Changed messages

				// Compare the previously sent Alert Messages with the new Alert Messages to check if there are any changes.
				if (last_verified_message_list.size() != message_list.size())
				{
					return true;
				}
				else
				{
					for (size_t i = 0; i < last_verified_message_list.size(); ++i)
					{
						auto alerted_message = last_verified_message_list[i];
						auto new_message = message_list[i];

						if (alerted_message->GetCode() != new_message->GetCode())
						{
							return true;
						}
					}
				}
			}

			return false;
		}

		void Alert::SendNotification(const NotificationData &notificationData)
		{
			auto alert = _server_config->GetAlert();

			auto message_body = notificationData.ToJsonString();
			if (message_body.IsEmpty())
			{
				logte("Message body is empty");
				return;
			}

			// Notification
			auto notification_server_url = ov::Url::Parse(alert.GetUrl());
			std::shared_ptr<Notification> notification_response = Notification::Query(notification_server_url, alert.GetTimeoutMsec(), alert.GetSecretKey(), message_body);
			if (notification_response == nullptr)
			{
				// Probably this doesn't happen
				logte("Could not load Notification");
			}

			if (notification_response->GetStatusCode() != Notification::StatusCode::OK)
			{
				logte("%s", notification_response->GetErrorReason().CStr());
			}
		}

		void Alert::SendNotification(const NotificationData::Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const ov::String &source_uri, const std::shared_ptr<StreamMetrics> &stream_metric)
		{
			NotificationData data(type, message_list, source_uri, stream_metric);

			SendNotification(data);
		}

		void Alert::SendNotification(const NotificationData::Type &type, const std::vector<std::shared_ptr<Message>> &message_list, const std::map<uint32_t, std::shared_ptr<QueueMetrics>> &queue_metric_list)
		{
			NotificationData data(type, message_list, queue_metric_list);

			SendNotification(data);
		}

		void Alert::CleanupReleasedMessages(const std::vector<ov::String> &new_messages_keys)
		{
			// Find and cleanup the messages that have already been released among the alerts that were sent.

			std::vector<ov::String> messages_keys_to_cleanup;
			for (const auto &[messages_key, verified_messages] : _last_verified_messages_map)
			{
				bool exist = false;
				for (const auto &new_messages_key : new_messages_keys)
				{
					if (messages_key == new_messages_key)
					{
						exist = true;
						break;
					}
				}

				if (!exist)
				{
					messages_keys_to_cleanup.push_back(messages_key);
				}
			}

			for (auto const &messages_key : messages_keys_to_cleanup)
			{
				RemoveVerifiedMessages(messages_key);
			}
		}

		bool Alert::PutVerifiedMessages(const ov::String &messages_key, std::vector<std::shared_ptr<Message>> &message_list)
		{
			if (messages_key.IsEmpty())
			{
				return false;
			}

			RemoveVerifiedMessages(messages_key);

			_last_verified_messages_map.emplace(messages_key, std::move(message_list));

			return true;
		}

		bool Alert::RemoveVerifiedMessages(const ov::String &messages_key)
		{
			if (messages_key.IsEmpty())
			{
				return false;
			}

			auto messages = _last_verified_messages_map.find(messages_key);
			if (messages != _last_verified_messages_map.end())
			{
				_last_verified_messages_map.erase(messages);
			}

			return true;
		}

		std::vector<std::shared_ptr<Message>> Alert::GetVerifiedMessages(const ov::String &messages_key)
		{
			if (messages_key.IsEmpty())
			{
				return {};
			}

			auto item = _last_verified_messages_map.find(messages_key);
			if (item == _last_verified_messages_map.end())
			{
				return {};
			}

			return item->second;
		}
	}  // namespace alrt
}  // namespace mon