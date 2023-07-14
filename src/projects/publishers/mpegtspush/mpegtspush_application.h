#pragma once

#include <base/common_types.h>
#include <base/publisher/application.h>
#include <base/info/session.h>

// #include "mpegtspush_userdata.h"
#include "mpegtspush_stream.h"

class MpegtsPushApplication : public pub::PushApplication
{
public:
	static std::shared_ptr<MpegtsPushApplication> Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	MpegtsPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	~MpegtsPushApplication();
	bool Start() override;

private:
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<pub::Stream> CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count) override;
	bool DeleteStream(const std::shared_ptr<info::Stream> &info) override;

public:
	virtual std::shared_ptr<ov::Error> StartPush(const std::shared_ptr<info::Push> &push) override;

};
