#pragma once

#include <base/ovlibrary/enable_shared_from_this.h>
#include <base/info/application.h>

class MediaRouteObserver : public ov::EnableSharedFromThis<MediaRouteObserver>
{
public:
	// Create Application
	virtual bool OnCreateApplication(const info::Application &app_info) = 0;

	// Delete Application
	virtual bool OnDeleteApplication(const info::Application &app_info) = 0;
};