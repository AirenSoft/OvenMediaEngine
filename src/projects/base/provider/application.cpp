//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "provider.h"
#include "application.h"
#include "stream.h"
#include "provider_private.h"

namespace pvd
{
	Application::Application(const std::shared_ptr<Provider> &provider, const info::Application &application_info)
		: info::Application(application_info)
	{
		_provider = provider;
		_last_issued_stream_id = 100;
	}

	Application::~Application()
	{
		Stop();
	}

	const char* Application::GetApplicationTypeName()
	{
		if(_provider == nullptr)
		{
			return "";
		}

		if(_app_type_name.IsEmpty())
		{
			_app_type_name.Format("%s %s",  _provider->GetProviderName(), "Application");
		}

		return _app_type_name.CStr();
	}

	bool Application::Start()
	{
		logti("%s has created [%s] application", _provider->GetProviderName(), GetName().CStr());

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
		logti("%s has deleted [%s] application", _provider->GetProviderName(), GetName().CStr());
		_state = ApplicationState::Stopped;
		return true;
	}

	info::stream_id_t Application::IssueUniqueStreamId()
	{
		return _last_issued_stream_id++;
	}

	const std::shared_ptr<Stream> Application::GetStreamById(uint32_t stream_id)
	{
		std::shared_lock<std::shared_mutex> lock(_streams_guard);

		if(_streams.find(stream_id) == _streams.end())
		{
			return nullptr;
		}

		return _streams.at(stream_id);
	}

	const std::shared_ptr<Stream> Application::GetStreamByName(ov::String stream_name)
	{
		std::shared_lock<std::shared_mutex> lock(_streams_guard);
		
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

	bool Application::DeleteStream(const std::shared_ptr<Stream> &stream)
	{
		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);

		if(_streams.find(stream->GetId()) == _streams.end())
		{
			logtc("Could not find stream to be removed : %s/%s(%u)", stream->GetApplicationInfo().GetName().CStr(), stream->GetName().CStr(), stream->GetId());
			return false;
		}
		_streams.erase(stream->GetId());

		streams_lock.unlock();
		
		stream->Stop();

		NotifyStreamDeleted(stream);

		return true;
	}

	bool Application::NotifyStreamCreated(const std::shared_ptr<Stream> &stream)
	{
		MediaRouteApplicationConnector::CreateStream(stream);
		return true;
	}

	bool Application::NotifyStreamDeleted(const std::shared_ptr<Stream> &stream)
	{
		MediaRouteApplicationConnector::DeleteStream(stream);
		return true;
	}

	bool Application::DeleteAllStreams()
	{
		std::unique_lock<std::shared_mutex> lock(_streams_guard);

		for(auto it = _streams.cbegin(); it != _streams.cend(); )
		{
			auto stream = it->second;
			it = _streams.erase(it);
			stream->Stop();

			NotifyStreamDeleted(stream);
		}

		return true;
	}
}