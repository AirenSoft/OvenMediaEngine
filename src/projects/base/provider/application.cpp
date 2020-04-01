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

#define OV_LOG_TAG "Provider"

namespace pvd
{
	Application::Application(const info::Application &application_info)
		: info::Application(application_info)
	{
		_last_issued_stream_id = 100;
	}

	Application::~Application()
	{
		Stop();
	}

	bool Application::Start()
	{
		logti("%s has started [%s] application", GetApplicationTypeName(), GetName().CStr());
		return true;
	}

	bool Application::Stop()
	{
		DeleteAllStreams();
		logti("%s has stopped [%s] application", GetApplicationTypeName(), GetName().CStr());
		return true;
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

		_last_issued_stream_id = new_stream_id;
		return new_stream_id;
	}

	const std::map<uint32_t, std::shared_ptr<Stream>>& Application::GetStreams() const
	{
		return _streams;
	}

	const std::shared_ptr<Stream> Application::GetStreamById(uint32_t stream_id) const
	{
		if(_streams.find(stream_id) == _streams.end())
		{
			return nullptr;
		}

		return _streams.at(stream_id);
	}

	const std::shared_ptr<Stream> Application::GetStreamByName(ov::String stream_name) const
	{
		for(auto const &x : _streams)
		{
			auto& stream = x.second;
			if(stream->GetName() == stream_name)
			{
				return stream;
			}
		}

		return nullptr;
	}

	bool Application::NotifyStreamCreated(std::shared_ptr<Stream> stream)
	{
		std::unique_lock<std::mutex> lock(_streams_map_guard);
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
		std::unique_lock<std::mutex> lock(_streams_map_guard);
		if(_streams.find(stream->GetId()) == _streams.end())
		{
			return false;
		}

		MediaRouteApplicationConnector::DeleteStream(stream);

		_streams.erase(stream->GetId());

		return true;
	}

	bool Application::DeleteAllStreams()
	{
		std::unique_lock<std::mutex> lock(_streams_map_guard);
		_streams.clear();

		return true;
	}

	bool Application::DeleteTerminatedStreams()
	{
		std::unique_lock<std::mutex> lock(_streams_map_guard);

		for(auto it = _streams.cbegin(); it != _streams.cend(); )
		{
			auto stream = it->second;
			if(stream->GetState() == Stream::State::STOPPED ||
				stream->GetState() == Stream::State::ERROR)
			{
				MediaRouteApplicationConnector::DeleteStream(stream);
				it = _streams.erase(it);
			}
			else
			{
				++it;
			}
		}

		return true;
	}
}