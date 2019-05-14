#pragma once

#include <base/common_types.h>
#include <base/ovcrypto/ovcrypto.h>
#include <base/publisher/application.h>
#include <base/publisher/stream.h>
#include <base/media_route/media_route_application_observer.h>

#include <physical_port/physical_port.h>
#include <ice/ice_port_manager.h>

#include <chrono>

//====================================================================================================
// Monitoring Collect Data
//====================================================================================================
enum class MonitroingCollectionType
{
    Stream = 0,
    App,
    Origin,
    Host,
};

struct MonitoringCollectionData
{
    MonitoringCollectionData() = default;

    MonitoringCollectionData(MonitroingCollectionType type_,
                           const ov::String &origin_name_ = "",
                            const ov::String &app_name_ = "",
                            const ov::String &stream_name_ = "")
    {
        type = type_;
        type_string = GetTypeString(type);
        origin_name = origin_name_;
        app_name = app_name_;
        stream_name = stream_name_;
    }

    void Append(const std::shared_ptr<MonitoringCollectionData> &collection)
    {
        edge_connection += collection->edge_connection;
        edge_bitrate += collection->edge_bitrate;
        p2p_connection += collection->p2p_connection;
        p2p_bitrate += collection->p2p_bitrate;
    }

    static ov::String GetTypeString(MonitroingCollectionType type)
    {
        ov::String result;

        if(type == MonitroingCollectionType::Stream)
            result = "stream";
        else if(type == MonitroingCollectionType::App)
            result = "app";
        else if(type == MonitroingCollectionType::Origin)
            result = "org";
        else if(type == MonitroingCollectionType::Host)
            result = "host";

        return result;
    }

    MonitroingCollectionType type = MonitroingCollectionType::Stream;
    ov::String type_string;
    ov::String origin_name;
    ov::String app_name;
    ov::String stream_name;
    uint32_t edge_connection = 0;   // count
    uint64_t edge_bitrate = 0;      // bps
    uint32_t p2p_connection = 0;    // count
    uint64_t p2p_bitrate = 0;       // bps
    std::chrono::system_clock::time_point check_time ; // (chrono)
};

// WebRTC, HLS, MPEG-DASH 등 모든 Publisher는 다음 Interface를 구현하여 MediaRouterInterface에 자신을 등록한다.
class Publisher
{
public:
	virtual bool Start();
	virtual bool Stop();

	// app_name으로 Application을 찾아서 반환한다.
	std::shared_ptr<Application> GetApplicationByName(ov::String app_name);
	std::shared_ptr<Stream> GetStream(ov::String app_name, ov::String stream_name);

	std::shared_ptr<Application> GetApplicationById(info::application_id_t application_id);
	std::shared_ptr<Stream> GetStream(info::application_id_t application_id, uint32_t stream_id);

	// monitoring data pure virtual function
	// - collected_datas vector must be insert processed
	virtual bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections) = 0;

protected:
	explicit Publisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);
	virtual ~Publisher() = default;

	// 모든 Publisher는 Type을 정의해야 하며, Config과 일치해야 한다.
	virtual cfg::PublisherType GetPublisherType() = 0;
	virtual std::shared_ptr<Application> OnCreateApplication(const info::Application *application_info) = 0;

	// 모든 application들의 map
	std::map<info::application_id_t, std::shared_ptr<Application>> _applications;

	// Publisher를 상속받은 클래스에서 사용되는 정보
	std::shared_ptr<MediaRouteApplicationInterface> _application;
	const info::Application *_application_info;

	std::shared_ptr<MediaRouteInterface> _router;
};

