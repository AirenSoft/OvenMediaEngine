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

#include "orchestrator/orchestrator.h"
namespace pvd
{
	Application::Application(const std::shared_ptr<Provider> &provider, const info::Application &application_info)
		: info::Application(application_info)
	{
		_provider = provider;
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
		// Don't get confused: Because this is static, a unique id is created even if different applications are different.
		static std::atomic<info::stream_id_t>	last_issued_stream_id(100);

		return last_issued_stream_id++;
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

	bool Application::AddStream(const std::shared_ptr<Stream> &stream)
	{
		// Check if same stream name is exist in MediaRouter(may be created by another provider)
		if (IsExistingInboundStream(stream->GetName()) == true)
		{
			logtw("Reject to add stream : there is already an incoming stream (%s) with the same name in application(%s) ", stream->GetName().CStr(), GetName().CStr());
			return false;
		}

		// If there is no data track, add data track
		if (stream->GetFirstTrackByType(cmn::MediaType::Data) == nullptr)
		{
			// Add data track 
			auto data_track = std::make_shared<MediaTrack>();
			// Issue unique track id
			data_track->SetId(stream->IssueUniqueTrackId());
			auto public_name = ov::String::FormatString("Data_%d", data_track->GetId());
			data_track->SetPublicName(public_name);
			data_track->SetMediaType(cmn::MediaType::Data);
			data_track->SetTimeBase(1, 1000);
			data_track->SetOriginBitstream(cmn::BitstreamFormat::Unknown);
			
			stream->AddTrack(data_track);
		}

		stream->SetApplication(GetSharedPtrAs<Application>());
		stream->SetApplicationInfo(GetSharedPtrAs<Application>());

		// This is not an official feature
		// OutputProfile(without encoding) is not applied to a specific provider.
		// To reduce transcoding cost when using Persistent Stream
		if (_provider->GetProviderType() == ProviderType::Rtmp)
		{
			if(GetConfig().GetProviders().GetRtmpProvider().IsPassthroughOutputProfile() == true)
			{
				stream->SetRepresentationType(StreamRepresentationType::Relay);
			}
		}

		// Add mapped audio track info
		auto audio_map_item_count = GetAudioMapItemCount();
		if (audio_map_item_count > 0)
		{
			for (size_t index=0; index < audio_map_item_count; index++)
			{
				auto audio_map_item = GetAudioMapItem(index);
				auto audio_track = stream->GetMediaTrackByOrder(cmn::MediaType::Audio, index);
				if (audio_map_item == nullptr || audio_track == nullptr)
				{
					break;
				}

				audio_track->SetPublicName(audio_map_item->GetName());
				audio_track->SetLanguage(audio_map_item->GetLanguage());
			}
		}

		// If track has not PublicName, set PublicName as TrackId
		for (auto &it : stream->GetTracks())
		{
			auto track = it.second;
			if (track->GetPublicName().IsEmpty())
			{
				// MediaType_TrackId
				auto public_name = ov::String::FormatString("%s_%u", cmn::GetMediaTypeString(track->GetMediaType()).CStr(), track->GetId());
				track->SetPublicName(public_name);
			}
		}

		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);
		_streams[stream->GetId()] = stream;
		streams_lock.unlock();

		NotifyStreamCreated(stream);
		
		return true;
	}

	// Update stream, if stream is not exist, add stream
	bool Application::UpdateStream(const std::shared_ptr<Stream> &stream)
	{
		std::unique_lock<std::shared_mutex> streams_lock(_streams_guard);

		if(_streams.find(stream->GetId()) == _streams.end())
		{
			// If stream is not exist, add stream
			streams_lock.unlock();
			AddStream(stream);
		}
		else
		{
			// If stream is exist, update stream
			_streams[stream->GetId()] = stream;
			streams_lock.unlock();
		}

		NotifyStreamUpdated(stream);

		return true;
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
		return MediaRouterApplicationConnector::CreateStream(stream);
	}

	bool Application::NotifyStreamUpdated(const std::shared_ptr<info::Stream> &stream)
	{
		return MediaRouterApplicationConnector::UpdateStream(stream);
	}

	bool Application::NotifyStreamDeleted(const std::shared_ptr<Stream> &stream)
	{
		return MediaRouterApplicationConnector::DeleteStream(stream);
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