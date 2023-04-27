//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "alert.h"

#include "monitoring/monitoring.h"
#include "notification.h"
#include "../monitoring_private.h"

#define LONG_KEY_FRAME_INTERVAL_SIZE		4.0

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
      if(server_config == nullptr)
      {
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

      if (alert.IsParsed() == false || rules.IsParsed() == false)
      {
        // Doesn't use the Alert feature.
        return;
      }

      ov::String source_uri;
      std::shared_ptr<std::vector<std::shared_ptr<Message>>> message_list;
      std::vector<ov::String> exist_source_uri_list;

      for (const auto& [host_key, host_metric] : MonitorInstance->GetHostMetricsList())
      {
        for (const auto& [app_key, app_metric] : host_metric->GetApplicationMetricsList())
        {
          for (const auto& [stream_key, stream_metric] : app_metric->GetStreamMetricsMap())
          {
            if (stream_metric->IsInputStream())
            {
              source_uri = stream_metric->GetUri();
              message_list = std::make_shared<std::vector<std::shared_ptr<Message>>>();
              exist_source_uri_list.push_back(source_uri);

              ValidateIngressRules(source_uri, rules, stream_metric, message_list);

              if (NeedsAlert(source_uri, message_list))
              {
                // Notification
                auto notification_server_url = ov::Url::Parse(alert.GetUrl());
                std::shared_ptr<Notification> notification_response = Notification::Query(notification_server_url, alert.GetTimeoutMsec(), alert.GetSecretKey(), source_uri, message_list, stream_metric);
                if (notification_response == nullptr)
                {
                  // Probably this doesn't happen
                  logte("Could not load Notification");
                }

                if (notification_response->GetStatusCode() != Notification::StatusCode::OK)
                {
                  logtc("%s", notification_response->GetErrReason().CStr());
                }
              }

              PutSourceUriMessages(source_uri, message_list);
            }
          }
        }
      }

      CleanupDeletedSources(&exist_source_uri_list);
    }

    template <typename T>
    void AddNonOkMessage(const ov::String &source_uri, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list, Message::Code code, T config_value, T measured_value)
    {
      if (code != Message::Code::OK)
      {
        ov::String description = Message::DescriptionFromMessageCode(code, config_value, measured_value);
        auto message = Message::CreateMessage(code, description);

        message_list->push_back(message);

        logtd("[%s] %s", source_uri.CStr(), description.CStr());
      }
    }

    void Alert::ValidateIngressRules(const ov::String &source_uri, cfg::alrt::rule::Rules rules, const std::shared_ptr<StreamMetrics> &stream_metric, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list)
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
          ValidateVideoIngressRules(source_uri, ingress, track, message_list);
        }
        else if (track->GetMediaType() == cmn::MediaType::Audio)
        {
          totalBitrate += track->GetBitrateByMeasured();
          ValidateAudioIngressRules(source_uri, ingress, track, message_list);
        }
      }

      if (totalBitrate > 0)
      {
        // Validate MinBitrates
        if (ingress.GetMinBitrate() > 0)
        {
          if (totalBitrate < ingress.GetMinBitrate())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_BITRATE_LOW, ingress.GetMinBitrate(), totalBitrate);
          }
        }

        // Validate MaxBitrates
        if (ingress.GetMaxBitrate() > 0)
        {
          if (totalBitrate > ingress.GetMaxBitrate())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_BITRATE_HIGH, ingress.GetMaxBitrate(), totalBitrate);
          }
        }
      }
    }

    void Alert::ValidateVideoIngressRules(const ov::String &source_uri, cfg::alrt::rule::Ingress ingress, const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list)
    {
      // Validate HasBFrame
      if (ingress.GetHasBFrame())
      {
        if (video_track->HasBframes())
        {
          AddNonOkMessage<bool>(source_uri, message_list, Message::Code::INGRESS_HAS_BFRAME, true, true);
        }
      }

      if (video_track->GetFrameRateByMeasured() > 0)
      {
        // Validate MinFramerate
        if (ingress.GetMinFramerate() > 0)
        {
          if (video_track->GetFrameRateByMeasured() < ingress.GetMinFramerate())
          {
            AddNonOkMessage<double>(source_uri, message_list, Message::Code::INGRESS_FRAMERATE_LOW, ingress.GetMinFramerate(), video_track->GetFrameRateByMeasured());
          }
        }

        // Validate MaxFramerate
        if (ingress.GetMaxFramerate() > 0)
        {
          if (video_track->GetFrameRateByMeasured() > ingress.GetMaxFramerate())
          {
            AddNonOkMessage<double>(source_uri, message_list, Message::Code::INGRESS_FRAMERATE_HIGH, ingress.GetMaxFramerate(), video_track->GetFrameRateByMeasured());
          }
        }

        // Validate LongKeyFrameInterval
        if (video_track->GetKeyFrameInterval() > 0 && ingress.IsLongKeyFrameInterval())
        {
          double interval = video_track->GetKeyFrameInterval() / video_track->GetFrameRateByMeasured();
          if (interval > LONG_KEY_FRAME_INTERVAL_SIZE)
          {
            AddNonOkMessage<double>(source_uri, message_list, Message::Code::INGRESS_LONG_KEY_FRAME_INTERVAL, LONG_KEY_FRAME_INTERVAL_SIZE, interval);
          }
        }
      }

      if (video_track->GetWidth() > 0)
      {
        // Validate MinWidth
        if (ingress.GetMinWidth() > 0)
        {
          if (video_track->GetWidth() < ingress.GetMinWidth())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_WIDTH_SMALL, ingress.GetMinWidth(), video_track->GetWidth());
          }
        }

        // Validate MaxWidth
        if (ingress.GetMaxWidth() > 0)
        {
          if (video_track->GetWidth() > ingress.GetMaxWidth())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_WIDTH_LARGE, ingress.GetMaxWidth(), video_track->GetWidth());
          }
        }
      }

      if (video_track->GetHeight() > 0)
      {
        // Validate MinHeight
        if (ingress.GetMinHeight() > 0)
        {
          if (video_track->GetHeight() < ingress.GetMinHeight())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_HEIGHT_SMALL, ingress.GetMinHeight(), video_track->GetHeight());
          }
        }

        // Validate MaxHeight
        if (ingress.GetMaxHeight() > 0)
        {
          if (video_track->GetHeight() > ingress.GetMaxHeight())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_HEIGHT_LARGE, ingress.GetMaxHeight(), video_track->GetHeight());
          }
        }
      }
    }

    void Alert::ValidateAudioIngressRules(const ov::String &source_uri, cfg::alrt::rule::Ingress ingress, const std::shared_ptr<MediaTrack> &audio_track, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list)
    {
      if (audio_track->GetSampleRate() > 0)
      {
        // Validate MinSamplerate
        if (ingress.GetMinSamplerate() > 0)
        {
          if (audio_track->GetSampleRate() < ingress.GetMinSamplerate())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_SAMPLERATE_LOW, ingress.GetMinSamplerate(), audio_track->GetSampleRate());
          }
        }

        // Validate MaxSamplerate
        if (ingress.GetMaxSamplerate() > 0)
        {
          if (audio_track->GetSampleRate() > ingress.GetMaxSamplerate())
          {
            AddNonOkMessage<int32_t>(source_uri, message_list, Message::Code::INGRESS_SAMPLERATE_HIGH, ingress.GetMinSamplerate(), audio_track->GetSampleRate());
          }
        }
      }
    }

    bool Alert::NeedsAlert(const ov::String &source_uri, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list)
    {
      auto last_alerted_message_list = GetSourceUriMessages(source_uri);

      if (last_alerted_message_list == nullptr)
      {
        // Created Stream

        if (message_list->size() > 0)
        {
          return true;
        }
      }
      else
      {
        // Exist Stream

        // Compare the previously sent Alert Messages with the new Alert Messages to check if there are any changes.
        if (last_alerted_message_list->size() != message_list->size())
        {
          return true;
        }
        else
        {
          for (size_t i = 0; i < last_alerted_message_list->size(); ++i)
          {
            auto alerted_message = (*last_alerted_message_list)[i];
            auto new_message = (*message_list)[i];

            if (alerted_message->GetCode() != new_message->GetCode())
            {
              return true;
            }
          }
        }
      }

      return false;
    }

    void Alert::CleanupDeletedSources(const std::vector<ov::String> *exist_source_uri_list)
    {
      // Find and remove the deleted streams that exist in the _source_uri_messages_map
      
      std::vector<ov::String> source_uri_list_to_remove;
      for (auto const &x : _source_uri_messages_map)
      {
        bool exist = false;
        for (auto const &exist_source_uri : *exist_source_uri_list)
        {
          exist = (x.first == exist_source_uri) ? true : false;
          break;
        }

        if (!exist)
        {
          source_uri_list_to_remove.push_back(x.first);
        }
      }

      for (auto const &source_uri_to_remove : source_uri_list_to_remove)
      {
        RemoveSourceUriMessages(source_uri_to_remove);
      }
    }

    bool Alert::PutSourceUriMessages(const ov::String &source_uri, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list)
    {
      if (source_uri == nullptr)
      {
        return false;
      }

      RemoveSourceUriMessages(source_uri);

      _source_uri_messages_map.emplace(source_uri, message_list);

      return true;
    }

    bool Alert::RemoveSourceUriMessages(const ov::String &source_uri)
    {
      if (source_uri == nullptr)
      {
        return false;
      }

      auto item = _source_uri_messages_map.find(source_uri);
      if (item != _source_uri_messages_map.end())
      {
        _source_uri_messages_map.erase(item);
      }

      return true;
    }

    std::shared_ptr<std::vector<std::shared_ptr<Message>>> Alert::GetSourceUriMessages(const ov::String &source_uri)
    {
      if (source_uri == nullptr)
      {
        return nullptr;
      }

      auto item = _source_uri_messages_map.find(source_uri);
      if (item == _source_uri_messages_map.end())
      {
        return nullptr;
      }

      return item->second;
    }
  }
}