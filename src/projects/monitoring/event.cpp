#include "event.h"

namespace mon
{
	Event::Event(EventType type, const std::shared_ptr<ServerMetrics> &server_metric)
		: _type(type), _server_metrics(server_metric)
	{
		switch(_type)
		{
			// StreamEventType
			case EventType::ServerStarted:
			case EventType::HostCreated:
			case EventType::HostDeleted:
			case EventType::AppCreated:
			case EventType::AppDeleted:
			case EventType::StreamCreated:
			case EventType::StreamDeleted: 
			case EventType::StreamUpdated:
				_category = EventCategory::StreamEventType;
				break;
			// SessionEventType
			case EventType::SessionConnected:
			case EventType::SessionDisconnected:
				_category = EventCategory::SessionEventType;
				break;
			// ActoinEventType
			case EventType::ApiCalled:
			case EventType::RecordingStarted:
			case EventType::RecordingStopped:
			case EventType::PushStarted:
			case EventType::PushStopped:
				_category = EventCategory::ActoinEventType;
				break;
			// NotificationEventType
			case EventType::Info:
			case EventType::Error:
				_category = EventCategory::NotificationEventType;
				break;
			// StatisticsEventType
			case EventType::ServerStat:
				_category = EventCategory::StatisticsEventType;
				break;
		}

	}

	EventType Event::GetType() const
	{
		return _type;
	}

	ov::String Event::GetTypeString() const
	{
		switch(_type)
		{
			// StreamEventType
			case EventType::ServerStarted:
				return "ServerStarted";
			case EventType::HostCreated:
				return "HostCreated";
			case EventType::HostDeleted:
				return "HostDeleted";
			case EventType::AppCreated:
				return "AppCreated";
			case EventType::AppDeleted:
				return "AppDeleted";
			case EventType::StreamCreated:
				return "StreamCreated";
			case EventType::StreamDeleted: 
				return "StreamDeleted";
			case EventType::StreamUpdated:
				return "StreamUpdated";
			// SessionEventType
			case EventType::SessionConnected:
				return "SessionConnected";
			case EventType::SessionDisconnected:
				return "SessionDisconnected";
			// ActoinEventType
			case EventType::ApiCalled:
				return "ApiCalled";
			case EventType::RecordingStarted:
				return "RecordingStarted";
			case EventType::RecordingStopped:
				return "RecordingStopped";
			case EventType::PushStarted:
				return "PushStarted";
			case EventType::PushStopped:
				return "PushStopped";
			// NotificationEventType
			case EventType::Info:
				return "Info";
			case EventType::Error:
				return "Error";
			// StatisticsEventType
			case EventType::ServerStat:
				return "ServerStat";
		}

		return "";
	}

	void Event::SetMetric(const std::vector<std::shared_ptr<HostMetrics>> &host_metric_list)
	{
		_set_metric_type = SetMetricType::HostMetricList;
		_host_metric_list = host_metric_list;
	}

	void Event::SetMetric(const std::shared_ptr<HostMetrics> &host_metric)
	{
		_set_metric_type = SetMetricType::HostMetric;
		_host_metric = host_metric;
	}

	void Event::SetMetric(const std::shared_ptr<ApplicationMetrics> &app_metric)
	{
		_set_metric_type = SetMetricType::AppMetric;
		_app_metric = app_metric;
	}

	void Event::SetMetric(const std::shared_ptr<StreamMetrics> &stream_metric)
	{
		_set_metric_type = SetMetricType::StreamMetric;
		_stream_metric = stream_metric;
	}

	void Event::SetMessage(const ov::String &message)
	{
		_message = message;
	}

	void Event::AddExtraData(const ov::String key, const ExtraValueType value)
	{
		_extra_data.emplace(key, value);
	}

	ov::String Event::SerializeToJson() const
	{
		Json::Value json_root;

		json_root["timestampMillis"] = ov::Clock::NowMSec();
		json_root["userKey"] = "Not Implemented";
		
		Json::Value json_server;
		json_server["serverID"] = _server_metrics->GetConfig()->GetID().CStr();
		json_server["startedTime"] = ov::Converter::ToISO8601String(std::chrono::system_clock::now()).CStr();	
		json_server["bind"] = _server_metrics->GetConfig()->GetBind().ToJson();

		json_root["server"] = json_server;

		Json::Value json_event;
		json_event["type"] = GetTypeString().CStr();
		if(_message.IsEmpty() == false)
		{
			json_event["message"] = _message.CStr();
		}
		
		Json::Value json_producer;
		Json::Value json_host;
		json_producer["serverID"] = _server_metrics->GetConfig()->GetID().CStr();
		switch(_set_metric_type)
		{
			case SetMetricType::HostMetric:
				FillProducerObject(json_producer, _host_metric);
				FillHostObject(json_host, _host_metric);
				break;
			case SetMetricType::AppMetric:
				FillProducerObject(json_producer, _app_metric);
				FillHostObject(json_host, _app_metric);
				break;
			case SetMetricType::StreamMetric:
				FillProducerObject(json_producer, _stream_metric);
				FillHostObject(json_host, _stream_metric);
				break;
			case SetMetricType::HostMetricList:
				// this means server so only needed serverID
			case SetMetricType::None:
			default:
				break;
		}

		json_event["producer"] = json_producer;
		if(json_host.isNull() == false)
		{
			json_event["host"] = json_host;
		}
		json_root["event"] = json_event;

		Json::StreamWriterBuilder builder;
		/* 
			Default StreamWriterBuilder settings

		  	(*settings)["commentStyle"] = "All";
			(*settings)["indentation"] = "\t";
			(*settings)["enableYAMLCompatibility"] = false;
			(*settings)["dropNullPlaceholders"] = false;
			(*settings)["useSpecialFloats"] = false;
			(*settings)["emitUTF8"] = false;
			(*settings)["precision"] = 17;
			(*settings)["precisionType"] = "significant";
		*/
		builder["indentation"] = "";
		return Json::writeString(builder, json_root).c_str();
	}

	bool Event::FillProducerObject(Json::Value &jsonProducer, const std::shared_ptr<HostMetrics> &host_metric) const
	{
		jsonProducer["serverID"] = _server_metrics->GetConfig()->GetID().CStr();
		jsonProducer["hostID"] = host_metric->GetUUID().CStr();
		return true;
	}

	bool Event::FillProducerObject(Json::Value &jsonProducer, const std::shared_ptr<ApplicationMetrics> &app_metric) const
	{
		FillProducerObject(jsonProducer, app_metric->GetHostMetrics());
		jsonProducer["appID"] = app_metric->GetUUID().CStr();
		return true;
	}

	bool Event::FillProducerObject(Json::Value &jsonProducer, const std::shared_ptr<StreamMetrics> &stream_metric) const
	{
		FillProducerObject(jsonProducer, stream_metric->GetApplicationMetrics());
		jsonProducer["streamID"] = stream_metric->GetUUID().CStr();
		return true;
	}

	bool Event::FillHostObject(Json::Value &json_host, const std::shared_ptr<HostMetrics> &host_metric) const
	{
		json_host["hostID"] = host_metric->GetUUID().CStr();
		json_host["name"] = host_metric->GetName().CStr();
		json_host["createdTime"] = ov::Converter::ToISO8601String(host_metric->CommonMetrics::GetCreatedTime()).CStr();
		//TODO(Getroot): Change this to real data
		json_host["distribution"] = "Not Implemented";
		json_host["hostNames"] = host_metric->GetHost().ToJson();

		return true;
	}

	bool Event::FillHostObject(Json::Value &json_host, const std::shared_ptr<ApplicationMetrics> &app_metric) const
	{
		auto host_metric = app_metric->GetHostMetrics();
		FillHostObject(json_host, host_metric);

		Json::Value json_app;
		json_app["appID"] = app_metric->GetUUID().CStr();
		json_app["name"] = app_metric->GetName().CStr();
		json_app["createdTime"] = ov::Converter::ToISO8601String(app_metric->CommonMetrics::GetCreatedTime()).CStr();
		json_app["outputProfiles"] = app_metric->GetConfig().GetOutputProfiles().ToJson();
		json_app["providers"] = app_metric->GetConfig().GetProviders().ToJson();
		json_app["publishers"] = app_metric->GetConfig().GetPublishers().ToJson();

		json_host["app"] = json_app;

		return true;
	}

	bool Event::FillHostObject(Json::Value &json_host, const std::shared_ptr<StreamMetrics> &stream_metric) const
	{
		auto app_metric = stream_metric->GetApplicationMetrics();
		FillHostObject(json_host, app_metric);

		Json::Value json_stream;
		json_stream["streamID"] = stream_metric->GetUUID().CStr();
		json_stream["name"] = stream_metric->GetName().CStr();
		json_stream["createdTime"] = ov::Converter::ToISO8601String(stream_metric->CommonMetrics::GetCreatedTime()).CStr();
		
		Json::Value json_provider;
		json_provider["type"] = StringFromStreamSourceType(stream_metric->GetSourceType()).CStr();		
		if(stream_metric->GetOriginStream() != nullptr)  
		{
			json_provider["source"] = stream_metric->GetOriginStreamUUID().CStr();
		}
		else
		{
			//TODO(Getroot): Implement this
			json_provider["source"] = "Not implemented";
		}

		json_stream["provider"] = json_provider;
		json_host["app"]["stream"] = json_stream;

		return true;
	}
}