#pragma once

#include <base/common_types.h>
#include <base/publisher/application.h>
#include <base/info/session.h>

#include "rtmppush_userdata.h"
#include "rtmppush_stream.h"

class RtmpPushApplication : public pub::Application
{
public:
	static std::shared_ptr<RtmpPushApplication> Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	RtmpPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	~RtmpPushApplication() final;

private:
	bool Start() override;
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<pub::Stream> CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count) override;
	bool DeleteStream(const std::shared_ptr<info::Stream> &info) override;

public:
	void SessionController();
	void SessionStart(std::shared_ptr<RtmpPushSession> session);
	void SessionStop(std::shared_ptr<RtmpPushSession> session);

	std::shared_ptr<ov::Error> PushStart(const std::shared_ptr<info::Push> &record);
	std::shared_ptr<ov::Error> PushStop(const std::shared_ptr<info::Push> &record);
	std::shared_ptr<ov::Error> GetPushes(std::vector<std::shared_ptr<info::Push>> &record_list);	

	RtmpPushUserdataSets _userdata_sets;

};
