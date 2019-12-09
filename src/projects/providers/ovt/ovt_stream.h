//
// Created by getroot on 19. 12. 9.
//

#pragma once

#include <base/common_types.h>
#include <base/provider/stream.h>
#include <base/ovlibrary/semaphore.h>
#include <base/provider/application.h>
#include <modules/ovt_packetizer/ovt_packet.h>

class OvtStream : public pvd::Stream
{
public:
	static std::shared_ptr<OvtStream> Create(const std::shared_ptr<pvd::Application> &app, const ov::String &url);

	explicit OvtStream(const std::shared_ptr<pvd::Application> &app, const ov::String &url);
	~OvtStream() final;

	void OnReceivePacket(const std::shared_ptr<OvtPacket> &packet);

private:
	bool Start();
	bool Stop();
	void WorkerThread();
	std::shared_ptr<OvtPacket> PopReceivedPacket();

	ov::String		_url;

	bool            _stop_thread_flag;
	std::thread     _worker_thread;
	std::mutex      _packet_queue_guard;
	std::queue<std::shared_ptr<OvtPacket>>   _packet_queue;
	ov::Semaphore	_queue_event;

	std::shared_ptr<pvd::Application> _app;
};