//
// Created by getroot on 19. 12. 9.
//

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>

#include <base/provider/stream.h>
#include <base/ovlibrary/semaphore.h>
#include <base/provider/application.h>
#include <modules/ovt_packetizer/ovt_packet.h>

class OvtStream : public pvd::Stream
{
public:
	static std::shared_ptr<OvtStream> Create(const std::shared_ptr<pvd::Application> &app, const std::shared_ptr<ov::Url> &url);

	explicit OvtStream(const std::shared_ptr<pvd::Application> &app, const StreamInfo &stream_info, const std::shared_ptr<ov::Url> &url);
	~OvtStream() final;

	void OnReceivePacket(const std::shared_ptr<OvtPacket> &packet);

private:
	bool Start();
	bool Stop();
	void WorkerThread();
	std::shared_ptr<OvtPacket> PopReceivedPacket();

	bool ConnectOrigin();
	bool RequestDescribe();
	bool ReceiveDescribe(uint32_t request_id);

	std::shared_ptr<OvtPacket> ReceivePacket();

	std::shared_ptr<ov::Url> _url;
	bool            _stop_thread_flag;
	std::thread     _worker_thread;
	std::mutex      _packet_queue_guard;
	std::queue<std::shared_ptr<OvtPacket>>   _packet_queue;
	ov::Semaphore	_queue_event;

	ov::Socket 		_client_socket;

	uint32_t 		_last_request_id;

	std::shared_ptr<pvd::Application> _app;
};