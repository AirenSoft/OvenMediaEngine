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
	Application::Application(const info::Application *application_info)
		: info::Application(*application_info)
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

	// 스트림을 생성한다
	std::shared_ptr<Stream> Application::MakeStream()
	{
		auto stream = OnCreateStream();
		if(!stream)
		{
			// Stream 생성 실패
			return nullptr;
		}

		return stream;
	}

	bool Application::CreateStream2(std::shared_ptr<Stream> stream)
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

	bool Application::DeleteStream2(std::shared_ptr<Stream> stream)
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