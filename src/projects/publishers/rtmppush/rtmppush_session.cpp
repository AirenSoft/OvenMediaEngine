#include <base/info/stream.h>
#include <base/publisher/stream.h>
#include "rtmppush_session.h"
#include "rtmppush_private.h"

std::shared_ptr<RtmpPushSession> RtmpPushSession::Create(const std::shared_ptr<pub::Application> &application,
										  	   const std::shared_ptr<pub::Stream> &stream,
										  	   uint32_t session_id)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<RtmpPushSession>(session_info, application, stream);
	
	return session;
}

RtmpPushSession::RtmpPushSession(const info::Session &session_info,
		   const std::shared_ptr<pub::Application> &application,
		   const std::shared_ptr<pub::Stream> &stream)
   : pub::Session(session_info, application, stream),
   _writer(nullptr)
{
	_sent_ready = false;

}

RtmpPushSession::~RtmpPushSession()
{
	Stop();
	logtd("RtmpPushSession(%d) has been terminated finally", GetId());
}


bool RtmpPushSession::Start()
{
	logtd("RtmpPushSession(%d) has started. Destination url(%s), stream_key(%s)", GetId(), _url.CStr(), _stream_key.CStr());
	
	ov::String rtmp_url = ov::String::FormatString("%s/%s", _url.CStr(), _stream_key.CStr());

	_writer = RtmpWriter::Create();
	if(_writer == nullptr)
	{
		SetState(SessionState::Error);	

		return false;
	}

	if(_writer->SetPath(rtmp_url, "flv") == false)
	{
		SetState(SessionState::Error);
		_writer = nullptr;

		return false;
	}

	for(auto &track_item : GetStream()->GetTracks())
	{
		auto &track = track_item.second;

		auto track_info = RtmpTrackInfo::Create();

		// It does not transmit unless it is H264 and AAC codec.
		if( !(track->GetCodecId() == common::MediaCodecId::H264 || track->GetCodecId() == common::MediaCodecId::Aac))
		{
			logtw("Could not supported codec. codec_id(%d)", track->GetCodecId());
			continue;
		}

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
			logte("Failed to add new track");
		}
	}

	if(_writer->Start() == false)
	{
		_writer = nullptr;
		SetState(SessionState::Error);
		return false;
	}

	return Session::Start();
}

bool RtmpPushSession::Stop()
{
	logtd("RtmpPushSession(%d) has stopped", GetId());
	
	if(_writer != nullptr)
	{
		_writer->Stop();
		_writer = nullptr;
	}

	return Session::Stop();
}

bool RtmpPushSession::SendOutgoingData(const std::any &packet)
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

	if(_writer != nullptr)
    {
	  	bool ret = _writer->PutData(
			session_packet->GetTrackId(), 
			session_packet->GetPts(),
			session_packet->GetDts(), 
			session_packet->GetFlag(), 
			session_packet->GetData());

		if(ret == false)
		{
			logte("Failed to add packet");
			SetState(SessionState::Error);
			_writer->Stop();
			_writer = nullptr;

			return false;
		} 
    }    

	return true;
}

void RtmpPushSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
									const std::shared_ptr<const ov::Data> &data)
{
	// Not used
}


void RtmpPushSession::SetUrl(ov::String url)
{
	_url = url;
}

void RtmpPushSession::SetStreamKey(ov::String stream_key)
{
	_stream_key = stream_key;
}
