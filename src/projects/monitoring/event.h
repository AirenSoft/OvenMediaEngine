#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "server_metrics.h"
#include <variant>
namespace mon
{
	enum class EventType
	{
		// StreamEventType
		ServerStarted,
		HostCreated, HostDeleted,
		AppCreated, AppDeleted,
		StreamCreated, StreamDeleted, StreamUpdated,
		// SessionEventType
		SessionConnected, SessionDisconnected,
		// ActoinEventType
		ApiCalled, 
		RecordingStarted, RecordingStopped,
		PushStarted, PushStopped,
		// NotificationEventType
		Info, Error,
		// StatisticsEventType
		ServerStat
	};

	using ExtraValueType = std::variant<int, float, bool, ov::String>;
	class Event
	{
	public:
		Event(EventType type, const std::shared_ptr<ServerMetrics> &server_metric);

		EventType GetType() const;
		ov::String GetTypeString() const;

		
		void SetMetric(const std::vector<std::shared_ptr<HostMetrics>> &host_metric_list);
		void SetMetric(const std::shared_ptr<HostMetrics> &host_metric);
		void SetMetric(const std::shared_ptr<ApplicationMetrics> &app_metric);
		void SetMetric(const std::shared_ptr<StreamMetrics> &stream_metric);

		void SetMessage(const ov::String &message);

		void AddExtraData(const ov::String key, const ExtraValueType value);

		ov::String SerializeToJson() const;

	private:
		bool FillProducerObject(Json::Value &json_producer, const std::shared_ptr<HostMetrics> &host_metric) const;
		bool FillProducerObject(Json::Value &json_producer, const std::shared_ptr<ApplicationMetrics> &app_metric) const;
		bool FillProducerObject(Json::Value &json_producer, const std::shared_ptr<StreamMetrics> &stream_metric) const;
		bool FillHostObject(Json::Value &json_host, const std::shared_ptr<HostMetrics> &host_metric) const;
		bool FillHostObject(Json::Value &json_host, const std::shared_ptr<ApplicationMetrics> &app_metric) const;
		bool FillHostObject(Json::Value &json_host, const std::shared_ptr<StreamMetrics> &stream_metric) const;

		enum class EventCategory
		{
			StreamEventType,
			SessionEventType,
			ActoinEventType,
			NotificationEventType,
			StatisticsEventType
		};

		EventType _type;
		EventCategory _category;
		ov::String _message;

		enum class SetMetricType
		{
			None,
			HostMetricList,
			HostMetric,
			AppMetric,
			StreamMetric
		};
		
		std::shared_ptr<ServerMetrics> _server_metrics;

		SetMetricType _set_metric_type = SetMetricType::None;
		std::vector<std::shared_ptr<HostMetrics>> _host_metric_list;
		std::shared_ptr<HostMetrics> _host_metric = nullptr;
		std::shared_ptr<ApplicationMetrics> _app_metric = nullptr;
		std::shared_ptr<StreamMetrics> _stream_metric = nullptr;

		std::map<ov::String, ExtraValueType> _extra_data;
	};
}