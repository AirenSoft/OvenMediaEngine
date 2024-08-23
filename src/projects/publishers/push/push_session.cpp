//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================

#include "push_session.h"

#include <base/info/stream.h>
#include <base/publisher/stream.h>

#include "push_private.h"

namespace pub
{
	std::shared_ptr<PushSession> PushSession::Create(const std::shared_ptr<pub::Application> &application,
															 const std::shared_ptr<pub::Stream> &stream,
															 uint32_t session_id,
															 std::shared_ptr<info::Push> &push)
	{
		auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
		auto session = std::make_shared<PushSession>(session_info, application, stream, push);
		return session;
	}

	PushSession::PushSession(const info::Session &session_info,
									 const std::shared_ptr<pub::Application> &application,
									 const std::shared_ptr<pub::Stream> &stream,
									 const std::shared_ptr<info::Push> &push)
		: pub::Session(session_info, application, stream),
		  _push(push),
		  _writer(nullptr)
	{
	}

	PushSession::~PushSession()
	{
		Stop();
		logtd("PushSession(%d) has been terminated finally", GetId());
	}

	bool PushSession::Start()
	{
		GetPush()->UpdatePushStartTime();
		GetPush()->SetState(info::Push::PushState::Connecting);
		

		ov::String rtmp_url;
		if (GetPush()->GetStreamKey().IsEmpty())
		{
			rtmp_url = GetPush()->GetUrl();
		}
		else
		{
			rtmp_url = ov::String::FormatString("%s/%s", GetPush()->GetUrl().CStr(), GetPush()->GetStreamKey().CStr());
		}

		std::unique_lock<std::shared_mutex> lock(_mutex);

		_writer = ffmpeg::Writer::Create();
		if (_writer == nullptr)
		{
			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return false;
		}

		ov::String format = (GetPush()->GetProtocolType() == info::Push::ProtocolType::RTMP) ? "flv" : "mpegts" ;
		if (_writer->SetUrl(rtmp_url, format) == false)
		{
			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			_writer = nullptr;

			return false;
		}

		for (auto &[track_id, track] : GetStream()->GetTracks())
		{
			if (track->GetMediaType() != cmn::MediaType::Video && track->GetMediaType() != cmn::MediaType::Audio)
			{
				continue;
			}

			// If the selected track list exists. if the current trackid does not exist on the list, ignore it.
			// If no track list is selected, save all tracks.
			if (IsSelectedTrack(track) == false)
			{
				continue;
			}

			// RTMP protocol does not supported except for H264 and AAC codec.
			if (GetPush()->GetProtocolType() == info::Push::ProtocolType::RTMP)
			{
				if (!(track->GetCodecId() == cmn::MediaCodecId::H264 || track->GetCodecId() == cmn::MediaCodecId::Aac))
				{
					logtw("Could not supported codec. track_id:%d, codec_id: %d", track->GetId(), track->GetCodecId());
					continue;
				}
			}

			bool ret = _writer->AddTrack(track);
			if (ret == false)
			{
				logtw("Failed to add new track");
			}
		}

		// Notice: If there are more than one video track, RTMP Push is not created and returns an error. You must use 1 video track.
		if (_writer->Start() == false)
		{
			_writer = nullptr;
			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return false;
		}

		GetPush()->SetState(info::Push::PushState::Pushing);

		logtd("PushSession(%d) has started.", GetId());

		lock.unlock();

		return Session::Start();
	}

	bool PushSession::IsSelectedTrack(const std::shared_ptr<MediaTrack> &track)
	{
		auto selected_track_ids = GetPush()->GetTrackIds();
		auto selected_track_names = GetPush()->GetVariantNames();

		if (selected_track_ids.size() > 0 || selected_track_names.size() > 0)
		{
			if ((find(selected_track_ids.begin(), selected_track_ids.end(), track->GetId()) == selected_track_ids.end()) &&
				(find(selected_track_names.begin(), selected_track_names.end(), track->GetVariantName()) == selected_track_names.end()))
			{
				return false;
			}
		}

		return true;
	}

	bool PushSession::Stop()
	{
		std::unique_lock<std::shared_mutex> lock(_mutex);

		if (_writer != nullptr)
		{
			GetPush()->SetState(info::Push::PushState::Stopping);
			GetPush()->UpdatePushStartTime();

			_writer->Stop();
			_writer = nullptr;

			GetPush()->SetState(info::Push::PushState::Stopped);
			GetPush()->IncreaseSequence();

			logtd("PushSession(%d) has stopped", GetId());
		}

		lock.unlock();

		return Session::Stop();
	}

	void PushSession::SendOutgoingData(const std::any &packet)
	{
		std::shared_ptr<MediaPacket> session_packet;

		try
		{
			session_packet = std::any_cast<std::shared_ptr<MediaPacket>>(packet);
			if (session_packet == nullptr)
			{
				return;
			}
		}
		catch (const std::bad_any_cast &e)
		{
			logtd("An incorrect type of packet was input from the stream. (%s)", e.what());

			return;
		}

		std::shared_lock<std::shared_mutex> lock(_mutex);
		if (_writer == nullptr)
		{
			return;
		}
		bool ret = _writer->SendPacket(session_packet);
		lock.unlock();
		if (ret == false)
		{
			logte("Failed to send packet");

			std::unique_lock<std::shared_mutex> release_lock(_mutex);

			_writer->Stop();
			_writer = nullptr;

			release_lock.unlock();

			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return;
		}

		GetPush()->UpdatePushTime();
		GetPush()->IncreasePushBytes(session_packet->GetData()->GetLength());
	}

	std::shared_ptr<info::Push> &PushSession::GetPush()
	{
		return _push;
	}
}  // namespace pub