#pragma once

#include <base/info/media_track.h>
#include <base/ovsocket/socket.h>
#include <base/publisher/session.h>

class OvtSession : public pub::Session
{
public:
	static std::shared_ptr<OvtSession> Create(const std::shared_ptr<pub::Application> &application,
											  const std::shared_ptr<pub::Stream> &stream,
											  uint32_t ovt_session_id,
											  const std::shared_ptr<ov::Socket> &connector);

	OvtSession(const info::Session &session_info,
			const std::shared_ptr<pub::Application> &application,
			const std::shared_ptr<pub::Stream> &stream,
			const std::shared_ptr<ov::Socket> &connector);
	~OvtSession() override;

	bool Start() override;
	bool Stop() override;

	bool SendOutgoingData(const std::any &packet) override;
	void OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
						const std::shared_ptr<const ov::Data> &data) override;


	const std::shared_ptr<ov::Socket> GetConnector();

private:
	std::shared_ptr<ov::Socket>		_connector;
	bool 							_sent_ready;
};
