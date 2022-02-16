#pragma once

#include <base/info/media_track.h>
#include <base/publisher/session.h>
#include <modules/mpegts/mpegts_writer.h>
#include "base/info/push.h"

class MpegtsPushSession : public pub::Session
{
public:
	static std::shared_ptr<MpegtsPushSession> Create(const std::shared_ptr<pub::Application> &application,
											  const std::shared_ptr<pub::Stream> &stream,
											  uint32_t ovt_session_id);

	MpegtsPushSession(const info::Session &session_info,
			const std::shared_ptr<pub::Application> &application,
			const std::shared_ptr<pub::Stream> &stream);
	~MpegtsPushSession() override;

	bool Start() override;
	bool Stop() override;

	bool SendOutgoingData(const std::any &packet) override;
	void OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
						const std::shared_ptr<const ov::Data> &data) override;

	
	void SetPush(std::shared_ptr<info::Push> &record);
	std::shared_ptr<info::Push>& GetPush();
	
private:
	std::shared_ptr<info::Push> _push;
	
	std::shared_mutex _mutex;
	
	std::shared_ptr<MpegtsWriter> _writer;
};
