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
#include "provider_private.h"

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
		logti("%s has created [%s] application", GetApplicationTypeName(), GetName().CStr());
		_state = ApplicationState::Started;
		return true;
	}

	bool Application::Stop()
	{
		if(_state == ApplicationState::Stopped)
		{
			return true;
		}
		DeleteAllStreams();
		logti("%s has deleted [%s] application", GetApplicationTypeName(), GetName().CStr());
		_state = ApplicationState::Stopped;
		return true;
	}

	info::stream_id_t Application::IssueUniqueStreamId()
	{
		return _last_issued_stream_id++;
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

		stream->Stop();
		MediaRouteApplicationConnector::DeleteStream(stream);
		_streams.erase(stream->GetId());

		return true;
	}

	bool Application::DeleteAllStreams()
	{
		std::unique_lock<std::mutex> lock(_streams_map_guard);

		for(auto it = _streams.cbegin(); it != _streams.cend(); )
		{
			auto stream = it->second;

			stream->Stop();
			MediaRouteApplicationConnector::DeleteStream(stream);
			it = _streams.erase(it);
		}

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
				stream->Stop();
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