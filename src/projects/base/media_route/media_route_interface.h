#pragma once

#include <stdint.h>
#include <memory>
#include <vector>

#include <base/ovlibrary/ovlibrary.h>
#include <base/application/application.h>
#include <physical_port/physical_port.h>

class MediaRouteApplicationObserver;
class MediaRouteApplicationConnector;
class StreamInfo;
class ApplicationInfo;

class MediaRouteInterface : public ov::EnableSharedFromThis<MediaRouteInterface>
{
public:
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 프로바이더(Provider) 모듈에서 사용함
	////////////////////////////////////////////////////////////////////////////////////////////////
	// 퍼블리셔(Publisher)의 어플리케이션이 호출하는 API 
	virtual bool RegisterConnectorApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationConnector> application_connector)
	{
		return false;
	}

	virtual bool UnregisterConnectorApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationConnector> application_connector)
	{
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// 퍼블리셔(Publisher) 모듈에서 호출함
	////////////////////////////////////////////////////////////////////////////////////////////////

	// 퍼블리셔(Publisher)의 어플리케이션이 호출하는 API 
	virtual bool RegisterObserverApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationObserver> application_observer) = 0;

	virtual bool UnregisterObserverApp(
		const info::Application *application_info,
		std::shared_ptr<MediaRouteApplicationObserver> application_observer) = 0;
};

