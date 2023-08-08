#include "mpegtspush_session.h"

#include <base/info/stream.h>
#include <base/publisher/stream.h>

#include "mpegtspush_private.h"

std::shared_ptr<MpegtsPushSession> MpegtsPushSession::Create(const std::shared_ptr<pub::Application> &application,
															 const std::shared_ptr<pub::Stream> &stream,
															 uint32_t session_id,
															 std::shared_ptr<info::Push> &push)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<MpegtsPushSession>(session_info, application, stream, push);

	return session;
}

MpegtsPushSession::MpegtsPushSession(const info::Session &session_info,
									 const std::shared_ptr<pub::Application> &application,
									 const std::shared_ptr<pub::Stream> &stream,
									 const std::shared_ptr<info::Push> &push)
	: pub::Session(session_info, application, stream),
	  _push(push),
	  _writer(nullptr)
{
}

MpegtsPushSession::~MpegtsPushSession()
{
	Stop();
	logtd("MpegtsPushSession(%d) has been terminated finally", GetId());
}

bool MpegtsPushSession::Start()
{
	GetPush()->UpdatePushStartTime();
	GetPush()->SetState(info::Push::PushState::Pushing);

	std::lock_guard<std::shared_mutex> lock(_mutex);

	_writer = ffmpeg::Writer::Create();
	if (_writer == nullptr)
	{
		SetState(SessionState::Error);
		GetPush()->SetState(info::Push::PushState::Error);

		return false;
	}

	if (_writer->SetUrl(GetPush()->GetUrl(), "mpegts") == false)
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
		if(IsSelectedTrack(track) == false)
		{
			continue;
		}

		bool ret = _writer->AddTrack(track);
		if (ret == false)
		{
			logtw("Failed to add new track");
		}
	}

	if (_writer->Start() == false)
	{
		_writer = nullptr;
		SetState(SessionState::Error);
		GetPush()->SetState(info::Push::PushState::Error);

		return false;
	}

	logtd("MpegtsPushSession(%d) has started.", GetId());

	return Session::Start();
}

bool MpegtsPushSession::Stop()
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	if (_writer != nullptr)
	{
		GetPush()->SetState(info::Push::PushState::Stopping);
		GetPush()->UpdatePushStartTime();

		_writer->Stop();
		_writer = nullptr;

		GetPush()->SetState(info::Push::PushState::Stopped);
		GetPush()->IncreaseSequence();

		logtd("MpegtsPushSession(%d) has stopped", GetId());
	}

	return Session::Stop();
}

void MpegtsPushSession::SendOutgoingData(const std::any &packet)
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

	std::lock_guard<std::shared_mutex> lock(_mutex);

	if (_writer == nullptr)
	{
		return;
	}

	bool ret = _writer->SendPacket(session_packet);
	if (ret == false)
	{
		logte("Failed to send packet");

		_writer->Stop();
		_writer = nullptr;

		SetState(SessionState::Error);
		GetPush()->SetState(info::Push::PushState::Error);

		return;
	}

	GetPush()->UpdatePushTime();
	GetPush()->IncreasePushBytes(session_packet->GetData()->GetLength());
}

bool MpegtsPushSession::IsSelectedTrack(const std::shared_ptr<MediaTrack> &track)
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


void MpegtsPushSession::SetPush(std::shared_ptr<info::Push> &push)
{
	_push = push;
}

std::shared_ptr<info::Push> &MpegtsPushSession::GetPush()
{
	return _push;
}
