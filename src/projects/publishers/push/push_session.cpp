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
		MonitorInstance->OnSessionConnected(*stream, PublisherType::Push);
	}

	PushSession::~PushSession()
	{
		Stop();
		logtd("PushSession(%d) has been terminated finally", GetId());
		MonitorInstance->OnSessionDisconnected(*GetStream(), PublisherType::Push);
	}

	bool PushSession::Start()
	{
		if (GetPush() == nullptr)
		{
			logte("Push object is null");
			SetState(SessionState::Error);
			return false;
		}

		GetPush()->UpdatePushStartTime();
		GetPush()->SetState(info::Push::PushState::Connecting);

		ov::String dest_url;
		if (GetPush()->GetStreamKey().IsEmpty())
		{
			dest_url = GetPush()->GetUrl();
		}
		else
		{
			dest_url = ov::String::FormatString("%s/%s", GetPush()->GetUrl().CStr(), GetPush()->GetStreamKey().CStr());
		}

		auto writer = CreateWriter();
		if (writer == nullptr)
		{
			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return false;
		}

		if (writer->SetUrl(dest_url, ffmpeg::Conv::GetFormatByProtocolType(GetPush()->GetProtocolType())) == false)
		{
			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return false;
		}

		for (auto &[track_id, track] : GetStream()->GetTracks())
		{
			// If the track defined in VariantNames exists, use it. If not, it is ignored.
			// If VariantNames is empty, all tracks are selected.
			if (IsSelectedTrack(track) == false)
			{
				continue;
			}

			if (IsSupportTrack(GetPush()->GetProtocolType(), track) == false)
			{
				logtd("Could not supported track. track_id:%d, codec_id: %d", track->GetId(), track->GetCodecId());
				continue;
			}

			if (IsSupportCodec(GetPush()->GetProtocolType(), track->GetCodecId()) == false)
			{
				logtd("Could not supported codec. track_id:%d, codec_id: %d", track->GetId(), track->GetCodecId());
				continue;
			}

			bool ret = writer->AddTrack(track);
			if (ret == false)
			{
				logtw("Failed to add new track");
			}
		}

		// Notice: If there are more than one video track, RTMP Push is not created and returns an error. You must use 1 video track.
		if (writer->Start() == false)
		{
			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return false;
		}

		GetPush()->SetState(info::Push::PushState::Pushing);

		logtd("PushSession(%d) has started.", GetId());

		return Session::Start();
	}

	bool PushSession::Stop()
	{
		auto writer = GetWriter();
		if (writer != nullptr)
		{
			auto push = GetPush();
			if (push != nullptr)
			{
				push->SetState(info::Push::PushState::Stopping);
				push->UpdatePushStartTime();
			}

			writer->Stop();

			if (push != nullptr)
			{
				GetPush()->SetState(info::Push::PushState::Stopped);
				GetPush()->IncreaseSequence();
			}

			DestoryWriter();

			logtd("PushSession(%d) has stopped", GetId());
		}

		return Session::Stop();
	}

	void PushSession::SendOutgoingData(const std::any &packet)
	{
		if (GetState() != SessionState::Started)
		{
			return;
		}

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

		auto writer = GetWriter();
		if (writer == nullptr)
		{
			return;
		}

		uint64_t sent_bytes = 0;

		bool ret = writer->SendPacket(session_packet, &sent_bytes);
		if (ret == false)
		{
			logte("Failed to send packet");

			writer->Stop();

			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);

			return;
		}

		GetPush()->UpdatePushTime();
		GetPush()->IncreasePushBytes(sent_bytes);

		MonitorInstance->IncreaseBytesOut(*GetStream(), PublisherType::Push, sent_bytes);
	}

	std::shared_ptr<ffmpeg::Writer> PushSession::CreateWriter()
	{
		std::lock_guard<std::shared_mutex> lock(_writer_mutex);
		if (_writer != nullptr)
		{
			_writer->Stop();
			_writer = nullptr;
		}

		_writer = ffmpeg::Writer::Create();
		if (_writer == nullptr)
		{
			return nullptr;
		}

		return _writer;
	}

	std::shared_ptr<ffmpeg::Writer> PushSession::GetWriter()
	{
		std::shared_lock<std::shared_mutex> lock(_writer_mutex);
		return _writer;
	}

	void PushSession::DestoryWriter()
	{
		std::lock_guard<std::shared_mutex> lock(_writer_mutex);
		if (_writer != nullptr)
		{
			_writer->Stop();
			_writer = nullptr;
		}
	}

	std::shared_ptr<info::Push> PushSession::GetPush()
	{
		std::shared_lock<std::shared_mutex> lock(_push_mutex);
		return _push;
	}

	bool PushSession::IsSelectedTrack(const std::shared_ptr<MediaTrack> &track)
	{
		auto selected_track_ids = GetPush()->GetTrackIds();
		auto selected_track_names = GetPush()->GetVariantNames();

		// Data type track is always selected.
		if(track->GetMediaType() == cmn::MediaType::Data)
		{
			return true;
		}

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

	bool PushSession::IsSupportTrack(const info::Push::ProtocolType protocol_type, const std::shared_ptr<MediaTrack> &track)
	{
		if (protocol_type == info::Push::ProtocolType::RTMP)
		{
			if (track->GetMediaType() == cmn::MediaType::Video ||
				track->GetMediaType() == cmn::MediaType::Audio ||
				track->GetMediaType() == cmn::MediaType::Data)
			{
				return true;
			}
		}
		else if (protocol_type == info::Push::ProtocolType::SRT || protocol_type == info::Push::ProtocolType::MPEGTS)
		{
			if (track->GetMediaType() == cmn::MediaType::Video ||
				track->GetMediaType() == cmn::MediaType::Audio)
				// SRT and MPEGTS do not support data track.
			{
				return true;
			}
		}

		return false;
	}

	bool PushSession::IsSupportCodec(const info::Push::ProtocolType protocol_type, cmn::MediaCodecId codec_id)
	{
		// RTMP protocol does not supported except for H264 and AAC codec.
		if (protocol_type == info::Push::ProtocolType::RTMP)
		{
			if (codec_id == cmn::MediaCodecId::H264 ||
				codec_id == cmn::MediaCodecId::Aac ||
				codec_id == cmn::MediaCodecId::None)
			{
				return true;
			}
		}
		else if (protocol_type == info::Push::ProtocolType::SRT || protocol_type == info::Push::ProtocolType::MPEGTS)
		{
			if (codec_id == cmn::MediaCodecId::H264 ||
				codec_id == cmn::MediaCodecId::H265 ||
				codec_id == cmn::MediaCodecId::Vp8 ||
				codec_id == cmn::MediaCodecId::Vp9 ||
				codec_id == cmn::MediaCodecId::Aac ||
				codec_id == cmn::MediaCodecId::Mp3 ||
				codec_id == cmn::MediaCodecId::Opus)
			{
				return true;
			}
		}

		return false;
	}
}  // namespace pub