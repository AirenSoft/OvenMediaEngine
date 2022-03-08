#include <base/info/stream.h>
#include <base/publisher/stream.h>

#include "mpegtspush_session.h"
#include "mpegtspush_private.h"

std::shared_ptr<MpegtsPushSession> MpegtsPushSession::Create(const std::shared_ptr<pub::Application> &application,
										  	   const std::shared_ptr<pub::Stream> &stream,
										  	   uint32_t session_id)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<MpegtsPushSession>(session_info, application, stream);
	
	return session;
}

MpegtsPushSession::MpegtsPushSession(const info::Session &session_info,
		   const std::shared_ptr<pub::Application> &application,
		   const std::shared_ptr<pub::Stream> &stream)
   : pub::Session(session_info, application, stream),
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

	logtd("MpegtsPushSession(%d) has started.", GetId());

	GetPush()->UpdatePushStartTime();
	GetPush()->SetState(info::Push::PushState::Pushing);

	ov::String mpegts_url { GetPush()->GetUrl() };

	std::lock_guard<std::shared_mutex> lock(_mutex);

	_writer = MpegtsWriter::Create();
	if(_writer == nullptr)
	{
		SetState(SessionState::Error);	
		GetPush()->SetState(info::Push::PushState::Error);		

		return false;
	}

	if(_writer->SetPath(mpegts_url, "mpegts") == false)
	{
		SetState(SessionState::Error);
		GetPush()->SetState(info::Push::PushState::Error);		

		_writer = nullptr;

		return false;
	}

	for(auto &track_item : GetStream()->GetTracks())
	{
		auto &track = track_item.second;

		// If the user selected a track, ignore the unselected track.
		auto selected_track = GetPush()->GetStream().GetTracks();		
		if (selected_track.size() > 0 && selected_track.find(track->GetId()) == selected_track.end())
		{
			continue;
		}

		// MPEGTS supports various codecs.

		// (Deprecated) It does not transmit unless it is H264 and AAC codec.
		// if( !(track->GetCodecId() == cmn::MediaCodecId::H264 || track->GetCodecId() == cmn::MediaCodecId::Aac))
		// {
		// 	logtw("Could not supported codec. codec_id(%d)", track->GetCodecId());
		// 	continue;
		// }
		
		auto track_info = MpegtsTrackInfo::Create();
		track_info->SetCodecId( track->GetCodecId() );
		track_info->SetBitrate( track->GetBitrate() );
		track_info->SetTimeBase( track->GetTimeBase() );
		track_info->SetWidth( track->GetWidth() );
		track_info->SetHeight( track->GetHeight() );
		track_info->SetSample( track->GetSample() );
		track_info->SetChannel( track->GetChannel() );
		track_info->SetExtradata( track->GetCodecExtradata() );

		bool ret = _writer->AddTrack(track->GetMediaType(), track->GetId(), track_info);
		if(ret == false)
		{
			logtw("Failed to add new track");
		}
	}

	if(_writer->Start() == false)
	{
		_writer = nullptr;
		SetState(SessionState::Error);
		GetPush()->SetState(info::Push::PushState::Error);		

		return false;
	}

	return Session::Start();
}

bool MpegtsPushSession::Stop()
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	if(_writer != nullptr)
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

bool MpegtsPushSession::SendOutgoingData(const std::any &packet)
{
	std::shared_ptr<MediaPacket> session_packet;

	try 
	{
        session_packet = std::any_cast<std::shared_ptr<MediaPacket>>(packet);
		if(session_packet == nullptr)
		{
			return false;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream. (%s)", e.what());

		return false;
    }

	std::lock_guard<std::shared_mutex> lock(_mutex);

	if(_writer != nullptr)
    {
	  	bool ret = _writer->PutData(
			session_packet->GetTrackId(), 
			session_packet->GetPts(),
			session_packet->GetDts(), 
			session_packet->GetFlag(), 
			session_packet->GetBitstreamFormat(),
			session_packet->GetData());

		if(ret == false)
		{
			logte("Failed to send packet");

			SetState(SessionState::Error);
			GetPush()->SetState(info::Push::PushState::Error);
			
			_writer->Stop();
			_writer = nullptr;

			return false;
		} 

		GetPush()->UpdatePushTime();
		GetPush()->IncreasePushBytes(session_packet->GetData()->GetLength());		
    }    

	return true;
}

void MpegtsPushSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
									const std::shared_ptr<const ov::Data> &data)
{
	// Not used
}

void MpegtsPushSession::SetPush(std::shared_ptr<info::Push> &push)
{
	_push = push;
}

std::shared_ptr<info::Push>& MpegtsPushSession::GetPush()
{
	return _push;
}
