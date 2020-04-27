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

	const std::map<uint32_t, std::shared_ptr<Stream>> Application::GetStreams() const
	{
		return _streams;
	}

	const std::shared_ptr<Stream> Application::GetStreamById(uint32_t stream_id)
	{
		std::shared_lock<std::shared_mutex> lock(_streams_map_guard);

		if(_streams.find(stream_id) == _streams.end())
		{
			return nullptr;
		}

		return _streams.at(stream_id);
	}

	const std::shared_ptr<Stream> Application::GetStreamByName(ov::String stream_name)
	{
		std::shared_lock<std::shared_mutex> lock(_streams_map_guard);
		
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

	std::shared_ptr<pvd::Stream> Application::CreateStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks)
	{
		auto stream = CreatePushStream(stream_id, stream_name);
		if(stream == nullptr)
		{
			return nullptr;
		}

		for(const auto &track : tracks)
		{
			stream->AddTrack(track);
		}
	
		std::unique_lock<std::shared_mutex> lock(_streams_map_guard);
		_streams[stream->GetId()] = stream;
		lock.unlock();

		NotifyStreamCreated(stream);

		return stream;
	}

	std::shared_ptr<pvd::Stream> Application::CreateStream(const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks)
	{
		return CreateStream(IssueUniqueStreamId(), stream_name, tracks);
	}

	std::shared_ptr<pvd::Stream> Application::CreateStream(const ov::String &stream_name, const std::vector<ov::String> &url_list)
	{
		auto stream = CreatePullStream(IssueUniqueStreamId(), stream_name, url_list);
		if(stream == nullptr)
		{
			return nullptr;
		}

		std::unique_lock<std::shared_mutex> lock(_streams_map_guard);
		_streams[stream->GetId()] = stream;
		lock.unlock();

		NotifyStreamCreated(stream);

		stream->Play();

		return stream;
	}

	bool Application::DeleteStream(std::shared_ptr<Stream> stream)
	{
		std::unique_lock<std::shared_mutex> lock(_streams_map_guard);
		if(_streams.find(stream->GetId()) == _streams.end())
		{
			return false;
		}

		stream->Stop();

		NotifyStreamDeleted(stream);

		_streams.erase(stream->GetId());

		return true;
	}

	bool Application::NotifyStreamCreated(std::shared_ptr<Stream> stream)
	{
		MediaRouteApplicationConnector::CreateStream(stream);
		return true;
	}

	bool Application::NotifyStreamDeleted(std::shared_ptr<Stream> stream)
	{
		MediaRouteApplicationConnector::DeleteStream(stream);
		return true;
	}

	bool Application::DeleteAllStreams()
	{
		std::unique_lock<std::shared_mutex> lock(_streams_map_guard);

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
		std::unique_lock<std::shared_mutex> lock(_streams_map_guard);

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