#pragma once

#include <base/info/media_track.h>
#include "base/publisher/session.h"

class OvtSession : public Session
{
public:
	static std::shared_ptr<RtcSession> Create(std::shared_ptr<Application> application,
											  std::shared_ptr<Stream> stream,
											  std::shared_ptr<SessionDescription> offer_sdp,
											  std::shared_ptr<SessionDescription> peer_sdp,
											  std::shared_ptr<IcePort> ice_port);

	RtcSession(SessionInfo &session_info,
			std::shared_ptr<Application> application,
			std::shared_ptr<Stream> stream,
	std::shared_ptr<SessionDescription> offer_sdp,
			std::shared_ptr<SessionDescription> peer_sdp,
	std::shared_ptr<IcePort> ice_port);
	~RtcSession() override;

	bool Start() override;
	bool Stop() override;

};
