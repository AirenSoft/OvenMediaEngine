#pragma once

#include <stdint.h>
#include <memory>
#include <vector>

#include <base/ovlibrary/ovlibrary.h>
#include <base/info/application.h>
#include <modules/physical_port/physical_port.h>

class MediaRouteApplicationObserver;
class MediaRouteApplicationConnector;
class Stream;

class MediaRouteInterface : public ov::EnableSharedFromThis<MediaRouteInterface>
{
public:

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Used by Provider modules
	////////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool RegisterConnectorApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationConnector> &application_connector) = 0;

	virtual bool UnregisterConnectorApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationConnector> &application_connector) = 0;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// Used by Publisher modules
	////////////////////////////////////////////////////////////////////////////////////////////////
	virtual bool RegisterObserverApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationObserver> &application_observer) = 0;

	virtual bool UnregisterObserverApp(
		const info::Application &application_info,
		const std::shared_ptr<MediaRouteApplicationObserver> &application_observer) = 0;
};

