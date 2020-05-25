//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovsocket/ovsocket.h>

#include <thread>

class PhysicalPort;
class PhysicalPortObserver;

class PhysicalPortWorker
{
public:
	PhysicalPortWorker(const std::shared_ptr<PhysicalPort> &physical_port);
	virtual ~PhysicalPortWorker();

	bool Start();
	bool Stop();

	bool AddTask(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<const ov::Data> &data);

protected:
	struct Task
	{
		Task(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<const ov::Data> &data)
			: client(client),
			  data(data)
		{
		}

		const std::shared_ptr<ov::ClientSocket> client;
		const std::shared_ptr<const ov::Data> data;
	};

	void ThreadProc();

	std::vector<PhysicalPortObserver *> &_observer_list;
	std::shared_ptr<PhysicalPort> _physical_port;

	std::thread _thread;
	volatile bool _stop = true;

	ov::Queue<Task> _task_list { nullptr, 500 };
};
