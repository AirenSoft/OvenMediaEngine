//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "application.h"
#include "stream.h"

#define OV_LOG_TAG "Application"

namespace pvd
{
	Application::Application(const info::Application &application_info)
		: info::Application(application_info)
	{

	}

	Application::~Application()
	{
		Stop();
	}

	bool Application::Start()
	{
		// TODO(soulk): Check this return value
		return false;
	}

	bool Application::Stop()
	{
		// TODO(soulk): Check this return value
		return false;
	}

	uint32_t Application::IssueUniqueStreamId()
	{
		auto new_stream_id = _last_issued_stream_id++;

		while(true)
		{
			if (_streams.find(new_stream_id) == _streams.end())
			{
				// not found
				break;
			}

			new_stream_id++;
		}

		return new_stream_id;
	}

	std::shared_ptr<Stream> Application::GetStreamById(uint32_t stream_id)
	{
		if(_streams.find(stream_id) == _streams.end())
		{
			return nullptr;
		}

		return _streams[stream_id];
	}

	std::shared_ptr<Stream> Application::GetStreamByName(ov::String stream_name)
	{
		for(auto const &x : _streams)
		{
			auto stream = x.second;
			if(stream->GetName() == stream_name)
			{
				return stream;
			}
		}

		return nullptr;
	}

	bool Application::NotifyStreamCreated(std::shared_ptr<Stream> stream)
	{
		logtd("CreateStream");

		if(stream == nullptr)
		{
			return false;
		}

		MediaRouteApplicationConnector::CreateStream(stream);

		_streams[stream->GetId()] = stream;

		return true;
	}

	bool Application::NotifyStreamDeleted(std::shared_ptr<Stream> stream)
	{
		logtd("DeleteStream");

		if(_streams.find(stream->GetId()) == _streams.end())
		{
			return false;
		}

		MediaRouteApplicationConnector::DeleteStream(stream);

		_streams.erase(stream->GetId());

		return true;
	}
}