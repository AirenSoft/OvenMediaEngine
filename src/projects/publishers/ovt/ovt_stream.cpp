

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
		"streamUUID" : "OvenMediaEngine_90b8b53e-3140-4e59-813d-9ace51c0e186/default/#default#app/stream",
		"playlists":
		[
			{
				"name" : "for llhls",
				"fileName" : "llhls_abr.oven",
				"options" :	// Optional
				{
					"webrtcAutoAbr" : true // default true
				},
				"renditions":
				[
					{
						"name" : "1080p",
						"videoTrackName" : "1080p",
						"audioTrackName" : "default",
					},
					{
						"name" : "720",
						"videoTrackName" : "720p",
						"audioTrackName" : "default",
					}
				],
				[
					...
				]
			},
			{
				...
			}
		],
		"tracks":
		[
			{
				"id" : 3291291,
				"name" : "1080p",
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
	Json::Value		json_playlists;

	json_stream["appName"] = GetApplicationName();
	json_stream["streamName"] = GetName().CStr();

	// Since the OVT publisher is also an output stream, it transmits the UUID of the input stream.
	if (GetLinkedInputStream() != nullptr)
	{
		json_stream["originStreamUUID"] = GetLinkedInputStream()->GetUUID().CStr();
	}
	else
	{
		json_stream["originStreamUUID"] = GetUUID().CStr();
	}

	for(const auto &[file_name, playlist] : GetPlaylists())
	{
		Json::Value json_playlist;
		
		json_playlist["name"] = playlist->GetName().CStr();
		json_playlist["fileName"] = playlist->GetFileName().CStr();

		Json::Value json_options;
		json_options["webrtcAutoAbr"] = playlist->IsWebRtcAutoAbr();
		json_options["hlsChunklistPathDepth"] = playlist->GetHlsChunklistPathDepth();

		json_playlist["options"] = json_options;

		for (const auto &rendition : playlist->GetRenditionList())
		{
			Json::Value json_rendition;

			json_rendition["name"] = rendition->GetName().CStr();
			json_rendition["videoTrackName"] = rendition->GetVideoVariantName().CStr();
			json_rendition["audioTrackName"] = rendition->GetAudioVariantName().CStr();

			json_playlist["renditions"].append(json_rendition);
		}

		json_playlists.append(json_playlist);
	}

	for(auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		Json::Value json_track;
		Json::Value json_video_track;
		Json::Value json_audio_track;

		json_track["id"] = track->GetId();
		json_track["name"] = track->GetVariantName().CStr();
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
		if(extra_data != nullptr)
		{
			auto extra_data_base64 = ov::Base64::Encode(extra_data);
			json_track["extra_data"] = extra_data_base64.CStr();
		}
		
		json_tracks.append(json_track);
	}

	json_stream["playlists"] = json_playlists;
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

	//logti("Recv Video Frame : pts(%lld) data_len(%lld)", media_packet->GetPts(), media_packet->GetDataLength());

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
	
	
	MonitorInstance->IncreaseBytesOut(*pub::Stream::GetSharedPtrAs<info::Stream>(), PublisherType::Ovt, packet->GetData()->GetLength() * GetSessionCount());

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