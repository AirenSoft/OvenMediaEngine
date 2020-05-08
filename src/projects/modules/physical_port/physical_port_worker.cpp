//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "physical_port_worker.h"

#include <algorithm>
#include <functional>

#include "physical_port.h"
#include "physical_port_private.h"

PhysicalPortWorker::PhysicalPortWorker(const std::shared_ptr<PhysicalPort> &physical_port)
	: _observer_list(physical_port->_observer_list)
{
}

PhysicalPortWorker::~PhysicalPortWorker()
{
}

bool PhysicalPortWorker::Start()
{
	if (_stop == false)
	{
		// Thread is already running
		return false;
	}

	_stop = false;
	_thread = std::thread(&PhysicalPortWorker::ThreadProc, this);

	return true;
}

bool PhysicalPortWorker::Stop()
{
	_task_list.Stop();

	if (_stop)
	{
		// Thread has already stopped
		return false;
	}

	_stop = true;

	if (_thread.joinable())
	{
		_thread.join();
	}

	return true;
}

void PhysicalPortWorker::AddTask(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<const ov::Data> &data)
{
	Task task(client, data);
	_task_list.Enqueue(std::move(task));
}

void PhysicalPortWorker::ThreadProc()
{
	while (_stop == false)
	{
		auto task = _task_list.Dequeue();

		if (task.has_value())
		{
			auto &value = task.value();

			auto &client = value.client;
			auto &data = value.data;

			// Notify observers
			auto func = std::bind(&PhysicalPortObserver::OnDataReceived, std::placeholders::_1, std::static_pointer_cast<ov::Socket>(client), std::ref(*(client->GetRemoteAddress().get())), ref(data));
			std::for_each(_observer_list.begin(), _observer_list.end(), func);
		}
	}
}