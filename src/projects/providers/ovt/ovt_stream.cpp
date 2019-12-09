//
// Created by getroot on 19. 12. 9.
//

#include "ovt_stream.h"

std::shared_ptr<OvtStream> OvtStream::Create(const std::shared_ptr<pvd::Application> &app, const ov::String &url)
{
	auto stream = std::make_shared<OvtStream>(app, url);

	stream->Start();

	return stream;
}

OvtStream::OvtStream(const std::shared_ptr<pvd::Application> &app, const ov::String &url)
{
	_app = app;
	_url = url;
}

OvtStream::~OvtStream()
{

}

bool OvtStream::Start()
{
	// Create Thread
	if(!_stop_thread_flag)
	{
		return true;
	}

	_stop_thread_flag = false;
	_worker_thread = std::thread(&OvtStream::WorkerThread, this);

	return true;
}

bool OvtStream::Stop()
{

	return true;
}

void OvtStream::OnReceivePacket(const std::shared_ptr<OvtPacket> &packet)
{
	std::unique_lock<std::mutex> lock(_packet_queue_guard);
	_packet_queue.push(packet);
	lock.unlock();

	_queue_event.Notify();
}

std::shared_ptr<OvtPacket> OvtStream::PopReceivedPacket()
{
	std::unique_lock<std::mutex> lock(_packet_queue_guard);

	if(_packet_queue.empty())
	{
		return nullptr;
	}

	auto data = _packet_queue.front();
	_packet_queue.pop();

	return std::move(data);
}

void OvtStream::WorkerThread()
{
	while(!_stop_thread_flag)
	{
		// Wait for events
		_queue_event.Wait();

		std::shared_ptr<OvtPacket> packet = PopReceivedPacket();
		if(packet == nullptr)
		{
			continue;
		}

		// Transaction
	}
}