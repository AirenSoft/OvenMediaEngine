#pragma once

#include <base/info/media_track.h>
#include <base/publisher/session.h>
#include <modules/ffmpeg/ffmpeg_writer.h>

#include "base/info/push.h"

class RtmpPushSession : public pub::Session
{
public:
	static std::shared_ptr<RtmpPushSession> Create(const std::shared_ptr<pub::Application> &application,
												   const std::shared_ptr<pub::Stream> &stream,
												   uint32_t ovt_session_id,
												   std::shared_ptr<info::Push> &push);

	RtmpPushSession(const info::Session &session_info,
					const std::shared_ptr<pub::Application> &application,
					const std::shared_ptr<pub::Stream> &stream,
					const std::shared_ptr<info::Push> &push);
	~RtmpPushSession() override;

	bool Start() override;
	bool Stop() override;

	void SendOutgoingData(const std::any &packet) override;

	// void SetPush(std::shared_ptr<info::Push> &record);
	std::shared_ptr<info::Push> &GetPush();

private:
	bool IsSelectedTrack(const std::shared_ptr<MediaTrack> &track);

	std::shared_ptr<info::Push> _push;

	std::shared_mutex _mutex;

	std::shared_ptr<ffmpeg::Writer> _writer;
};
