

#include "ovt_private.h"
#include "ovt_stream.h"
#include "ovt_session.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"

std::shared_ptr<OvtStream> OvtStream::Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info,
											 uint32_t worker_count)
{
	auto stream = std::make_shared<OvtStream>(application, info, worker_count);
	return stream;
}

OvtStream::OvtStream(const std::shared_ptr<pub::Application> application,
					 const info::Stream &info,
					 uint32_t worker_count)
		: Stream(application, info),
		_worker_count(worker_count)
{
	logtd("OvtStream(%s/%s) has been started", GetApplicationName() , GetName().CStr());
}

OvtStream::~OvtStream()
{
	logtd("OvtStream(%s/%s) has been terminated finally", GetApplicationName() , GetName().CStr());
}

bool OvtStream::Start()
{
	if(GetState() != Stream::State::CREATED)
	{
		return false;
	}

	if(!CreateStreamWorker(_worker_count))
	{
		return false;
	}

	logtd("OvtStream(%d) has been started", GetId());
	_packetizer = std::make_shared<OvtPacketizer>(OvtPacketizerInterface::GetSharedPtr());
	_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(pub::Stream::GetSharedPtr()));

	return Stream::Start();
}

bool OvtStream::Stop()
{
	if(GetState() != Stream::State::STARTED)
	{
		return false;
	}

	logtd("OvtStream(%u) has been stopped", GetId());

	std::unique_lock<std::shared_mutex> mlock(_packetizer_lock);
	if(_packetizer != nullptr)
	{	
		_packetizer->Release();
		_packetizer.reset();
	}
	mlock.unlock();

	return Stream::Stop();
}

bool OvtStream::GenerateDecription()
{
/*
	"stream" :
	{
		"appName" : "app",
		"streamName" : "stream_720p",
		"tracks":
		[
			{
				"id" : 3291291,
				"codecId" : 32198392,
				"mediaType" : 0 | 1 | 2, # video | audio | data
				"timebase_num" : 90000,
				"timebase_den" : 90000,
				"bitrate" : 5000000,
				"startFrameTime" : 1293219321,
				"lastFrameTime" : 1932193921,
				"videoTrack" :
				{
					"framerate" : 29.97,
					"width" : 1280,
					"height" : 720
				},
				"audioTrack" :
				{
					"samplerate" : 44100,
					"sampleFormat" : "s16",
					"layout" : "stereo"
				}
			}
		]
	}
*/

	Json::Value 	json_root;
	Json::Value		json_stream;
	Json::Value		json_tracks;

	json_stream["appName"] = GetApplicationName();
	json_stream["streamName"] = GetName().CStr();

	for(auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		Json::Value json_track;
		Json::Value json_video_track;
		Json::Value json_audio_track;

		track->GetCodecExtradata();

		json_track["id"] = track->GetId();
		json_track["codecId"] = static_cast<int8_t>(track->GetCodecId());
		json_track["mediaType"] = static_cast<int8_t>(track->GetMediaType());
		json_track["timebase_num"] = track->GetTimeBase().GetNum();
		json_track["timebase_den"] = track->GetTimeBase().GetDen();
		json_track["bitrate"] = track->GetBitrate();
		json_track["startFrameTime"] = track->GetStartFrameTime();
		json_track["lastFrameTime"] = track->GetLastFrameTime();

		json_video_track["framerate"] = track->GetFrameRate();
		json_video_track["width"] = track->GetWidth();
		json_video_track["height"] = track->GetHeight();

		json_audio_track["samplerate"] = track->GetSampleRate();
		json_audio_track["sampleFormat"] = static_cast<int8_t>(track->GetSample().GetFormat());
		json_audio_track["layout"] = static_cast<uint32_t>(track->GetChannel().GetLayout());

		json_track["videoTrack"] = json_video_track;
		json_track["audioTrack"] = json_audio_track;

		auto &extra_data = track->GetCodecExtradata();
		if(!extra_data.empty())
		{
			auto extra_data_base64 = ov::Base64::Encode(ov::Data(extra_data.data(), extra_data.size()));
			json_track["extra_data"] = extra_data_base64.CStr();
		}
		
		json_tracks.append(json_track);
	}

	json_stream["tracks"] = json_tracks;
	json_root["stream"] = json_stream;
	
	_description = json_root;

	return true;
}

void OvtStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(GetState() != Stream::State::STARTED)
	{
		return;
	}

	// Callback OnOvtPacketized()
	std::shared_lock<std::shared_mutex> mlock(_packetizer_lock);
	if(_packetizer != nullptr)
	{
		_packetizer->PacketizeMediaPacket(media_packet->GetPts(), media_packet);
	}
}

void OvtStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(GetState() != Stream::State::STARTED)
	{
		return;
	}

	// Callback OnOvtPacketized()
	std::shared_lock<std::shared_mutex> mlock(_packetizer_lock);
	if(_packetizer != nullptr)
	{
		_packetizer->PacketizeMediaPacket(media_packet->GetPts(), media_packet);
	}
}

bool OvtStream::OnOvtPacketized(std::shared_ptr<OvtPacket> &packet)
{
	// Broadcasting
	auto stream_packet = std::make_any<std::shared_ptr<OvtPacket>>(packet);
	BroadcastPacket(stream_packet);
	
	if(_stream_metrics != nullptr)
	{
		_stream_metrics->IncreaseBytesOut(PublisherType::Ovt, packet->GetData()->GetLength() * GetSessionCount());
	}

	return true;
}

bool OvtStream::GetDescription(Json::Value &description)
{
	if(GetState() != Stream::State::STARTED)
	{
		return false;
	}
	
	GenerateDecription();
	description = _description;

	return true;
}

bool OvtStream::RemoveSessionByConnectorId(int connector_id)
{
	auto sessions = GetAllSessions();

	logtd("RemoveSessionByConnectorId : all(%d) connector(%d)", sessions.size(), connector_id);

	for(const auto &item : sessions)
	{
		auto session = std::static_pointer_cast<OvtSession>(item.second);
		logtd("session : %d %d", session->GetId(), session->GetConnector()->GetNativeHandle());

		if(session->GetConnector()->GetNativeHandle() == connector_id)
		{
			RemoveSession(session->GetId());
			return true;
		}
	}

	return false;
}