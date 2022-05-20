//==============================================================================
//
//  PullProvider Base Class
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#include "stream.h"

#include "application.h"
#include "base/info/application.h"
#include "provider_private.h"
#include "stream_props.h"

namespace pvd
{
	PullStream::PullStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list, std::shared_ptr<pvd::PullStreamProperties> properties)
		: Stream(application, stream_info)
	{
		for (auto &url : url_list)
		{
			auto parsed_url = ov::Url::Parse(url);
			if (parsed_url)
			{
				_url_list.push_back(parsed_url);
			}
		}

		// In case of Pull Stream created by Origins, Properties information is included.
		_properties = properties;
	}

	bool PullStream::Start(uint32_t max_retry_count)
	{
		std::lock_guard<std::mutex> lock(_start_stop_stream_lock);
		_restart_count = 0;
		while (true)
		{
			if (StartStream(GetNextURL()) == false)
			{
				_restart_count++;
				if (_restart_count > _url_list.size() * max_retry_count)
				{
					SetState(Stream::State::TERMINATED);
					return false;
				}
			}
			else
			{
				_restart_count = 0;
				break;
			}
		}

		logti("%s has started to play [%s(%u)] stream : %s", GetApplicationTypeName(), GetName().CStr(), GetId(), GetMediaSource().CStr());
		return Stream::Start();
	}

	bool PullStream::Stop()
	{
		std::lock_guard<std::mutex> lock(_start_stop_stream_lock);
		StopStream();
		return Stream::Stop();
	}

	bool PullStream::Resume(uint32_t max_retry_count)
	{
		if (RestartStream(GetNextURL()) == false)
		{
			Stop();
			_restart_count++;
			if (_restart_count > _url_list.size() * max_retry_count)
			{
				SetState(Stream::State::TERMINATED);
			}

			return false;
		}

		_restart_count = 0;
		return Stream::Start();
	}

	const std::shared_ptr<const ov::Url> PullStream::GetNextURL()
	{
		if (_url_list.size() == 0)
		{
			return nullptr;
		}

		if (static_cast<size_t>(_curr_url_index + 1) > _url_list.size())
		{
			_curr_url_index = 0;
		}

		auto curr_url = _url_list[_curr_url_index];
		SetMediaSource(curr_url->ToUrlString(true));

		_curr_url_index++;

		return curr_url;
	}

	const std::shared_ptr<const ov::Url> PullStream::GetPrimaryURL()
	{
		if (_url_list.size() == 0)
		{
			return nullptr;
		}

		auto curr_url = _url_list[0];

		return curr_url;
	}

	bool PullStream::IsCurrPrimaryURL()
	{
		// If the currently playing URL and the 0th URL are the same, it is the primary URL.
		if (GetMediaSource() == _url_list[0]->ToUrlString(true))
			return true;

		return false;
	}

	void PullStream::InitCurrUrlIndex()
	{
		_curr_url_index = 0;
	}

	std::shared_ptr<pvd::PullStreamProperties> PullStream::GetProperties()
	{
		return _properties;
	}

}  // namespace pvd
