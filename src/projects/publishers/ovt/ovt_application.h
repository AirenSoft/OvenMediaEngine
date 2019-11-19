#pragma once

#include <base/common_types.h>
#include <base/publisher/application.h>
#include <base/info/session_info.h>

#include "ovt_stream.h"

class OvtApplication : public Application
{
public:
	static std::shared_ptr<OvtApplication> Create(const info::Application &application_info);
	OvtApplication(const info::Application &application_info);
	~OvtApplication() final;

private:
	bool Start() override;
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<Stream> CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count) override;
	bool DeleteStream(std::shared_ptr<StreamInfo> info) override;
};
