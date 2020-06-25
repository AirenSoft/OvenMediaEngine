//==============================================================================
//
//  MpegTs Stream
//
//  Created by Hyunjun Jang
//  Moved by Getroot
//
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//==============================================================================

#include "mpegts_stream.h"
#include "base/info/application.h"
#include "base/provider/push_provider/application.h"
#include "modules/mpegts/mpegts_packet.h"
#include "mpegts_provider_private.h"

#include "modules/codec_analyzer/aac/aac_adts.h"

#include <orchestrator/orchestrator.h>
#include <base/media_route/media_type.h>
#include <base/info/media_extradata.h>

#include <modules/mpegts/mpegts_packet.h>

namespace pvd
{
	std::shared_ptr<MpegTsStream> MpegTsStream::Create(StreamSourceType source_type, uint32_t client_id, const ov::String app_name, const ov::String stream_name, const std::shared_ptr<ov::Socket> &client_socket, const std::shared_ptr<PushProvider> &provider)
	{
		auto stream = std::make_shared<MpegTsStream>(source_type, client_id, app_name, stream_name, client_socket, provider);
		if(stream != nullptr)
		{
			stream->Start();
		}
		return stream;
	}

	MpegTsStream::MpegTsStream(StreamSourceType source_type, uint32_t client_id, const ov::String app_name, const ov::String stream_name, std::shared_ptr<ov::Socket> client_socket, const std::shared_ptr<PushProvider> &provider)
		: PushStream(source_type, client_id, provider)
	{
		_app_name = app_name;
		SetName(stream_name);
		_remote = client_socket;
	}

	MpegTsStream::~MpegTsStream()
	{
	}

	bool MpegTsStream::Start()
	{
		_state = Stream::State::PLAYING;
		return PushStream::Start();
	}

	bool MpegTsStream::Stop()
	{
		if(GetState() == Stream::State::STOPPED)
		{
			return true;
		}

		_state = Stream::State::STOPPED;

		if(_remote->GetState() == ov::SocketState::Connected)
		{
			_remote->Close();
		}

		return PushStream::Stop();
	}

	const std::shared_ptr<ov::Socket>& MpegTsStream::GetClientSock()
	{
		return _remote;
	}

	bool MpegTsStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		if(GetState() == Stream::State::ERROR || GetState() == Stream::State::STOPPED || GetState() == Stream::State::STOPPING)
		{
			return false;
		}

		std::lock_guard<std::shared_mutex> lock(_depacketizer_lock);
		_depacketizer.AddPacket(data);
		
		// Publish
		if(IsPublished() == false && _depacketizer.IsTrackInfoAvailable())
		{
			Publish();
		}

		if(IsPublished() == true)
		{
			while(_depacketizer.IsESAvailable())
			{
				auto es = _depacketizer.PopES();
				
				if(es->IsVideoStream())
				{	

					auto data = std::make_shared<ov::Data>(es->Payload(), es->PayloadLength());
					auto media_packet = std::make_shared<MediaPacket>(common::MediaType::Video,
												es->PID(),
												data,
												es->Pts(), // PTS
												es->Dts(), // DTS
												-1LL, // duration,
												es->IsKeyframe()?MediaPacketFlag::Key:MediaPacketFlag::NoFlag);
					SendFrame(media_packet);
				}
				else if(es->IsAudioStream())
				{
					auto payload = es->Payload();
					auto payload_length = es->PayloadLength();
#if 1
					auto data = std::make_shared<ov::Data>(payload, payload_length);
					auto media_packet = std::make_shared<MediaPacket>(common::MediaType::Audio,
												es->PID(),
												data,
												es->Pts(), // PTS
												es->Dts(), // DTS
												-1LL, // duration,
												MediaPacketFlag::Key);
					SendFrame(media_packet);
#endif

#if 0
					uint32_t adts_start = 0;
					int i=0;
					while(adts_start < payload_length)
					{
						AACAdts adts;

						if(AACAdts::ParseAdtsHeader(payload + adts_start, payload_length - adts_start, adts) == false)
						{
							return false;
						}

						auto frame_length = adts.AacFrameLength();

						auto data = std::make_shared<ov::Data>(payload + adts_start, frame_length);
						auto media_packet = std::make_shared<MediaPacket>(common::MediaType::Audio,
													es->PID(),
													data,
													es->Pts() + (i*2000), // PTS
													es->Dts(), // DTS
													-1LL, // duration,
													MediaPacketFlag::Key);

						logtc("PTS : %lld | CPTS : %lld", es->Pts(), es->Pts() + (i*2000));
						
						SendFrame(media_packet);

						adts_start += frame_length;
						i++;
					}
#endif
				}
			}
		}

		return true;
	}

	bool MpegTsStream::Publish()
	{
		std::map<uint16_t, std::shared_ptr<MediaTrack>> track_list;
		
		if(_depacketizer.GetTrackList(&track_list) == false)
		{
			logte("Cannot get track list from mpeg-ts depacketizer.");
			return false;
		}

		for(const auto &x : track_list)
		{
			auto track = x.second;
			AddTrack(track);
		}

		// Publish
		PublishInterleavedChannel(_app_name);

		return true;
	}
}