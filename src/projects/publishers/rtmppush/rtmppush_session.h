#pragma once

#include <base/info/media_track.h>
#include <base/publisher/session.h>
#include <modules/rtmp/rtmp_writer.h>

class RtmpPushSession : public pub::Session
{
public:
	static std::shared_ptr<RtmpPushSession> Create(const std::shared_ptr<pub::Application> &application,
											  const std::shared_ptr<pub::Stream> &stream,
											  uint32_t ovt_session_id);

	RtmpPushSession(const info::Session &session_info,
			const std::shared_ptr<pub::Application> &application,
			const std::shared_ptr<pub::Stream> &stream);
	~RtmpPushSession() override;

	bool Start() override;
	bool Stop() override;

	bool SendOutgoingData(const std::any &packet) override;
	void OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
						const std::shared_ptr<const ov::Data> &data) override;


public:
	// Destination RTMP Url and Stream Key/
	void SetUrl(ov::String url);
	void SetStreamKey(ov::String stream_key);


private:
	bool _sent_ready;

	ov::String _url;
	ov::String _stream_key;

	std::shared_ptr<RtmpWriter>				_writer;

	// statistics
	int64_t _stats_sent_bytes;
	int64_t _stats_sent_packets;
	int32_t _stats_recoonect;
	
};
