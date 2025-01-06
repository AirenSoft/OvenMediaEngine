//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "srt_stream.h"

#include <orchestrator/orchestrator.h>

#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "srt_private.h"
#include "srt_session.h"

#define logap(format, ...) logtp("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)
#define logad(format, ...) logtd("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)
#define logas(format, ...) logts("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)

#define logai(format, ...) logti("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)
#define logae(format, ...) logte("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)
#define logac(format, ...) logtc("[%s/%s(%u)] " format, GetApplicationName(), GetName().CStr(), GetId(), ##__VA_ARGS__)

namespace pub
{
	std::shared_ptr<SrtStream> SrtStream::Create(const std::shared_ptr<Application> application,
												 const info::Stream &info,
												 uint32_t worker_count)
	{
		auto stream = std::make_shared<SrtStream>(application, info, worker_count);
		return stream;
	}

	SrtStream::SrtStream(const std::shared_ptr<Application> application,
						 const info::Stream &info,
						 uint32_t worker_count)
		: Stream(application, info),
		  _worker_count(worker_count)
	{
		logad("SrtStream has been started");
	}

	SrtStream::~SrtStream()
	{
		logad("SrtStream has been terminated finally");
	}

#define SRT_SET_TRACK(from, to, supported, message, ...)  \
	if (to == nullptr)                                    \
	{                                                     \
		if (supported)                                    \
		{                                                 \
			to = from;                                    \
		}                                                 \
		else                                              \
		{                                                 \
			logai("SrtStream - " message, ##__VA_ARGS__); \
		}                                                 \
	}

	bool SrtStream::Start()
	{
		if (GetState() != Stream::State::CREATED)
		{
			return false;
		}

		// If this stream is from OriginMapStore, don't register it to OriginMapStore again.
		if (IsFromOriginMapStore() == false)
		{
			auto result = ocst::Orchestrator::GetInstance()->RegisterStreamToOriginMapStore(GetApplicationInfo().GetVHostAppName(), GetName());

			if (result == CommonErrorCode::ERROR)
			{
				logaw("Failed to register stream to origin map store");
				return false;
			}
		}

		if (CreateStreamWorker(_worker_count) == false)
		{
			return false;
		}

		logad("The SRT stream has been started");

		auto packetizer = std::make_shared<mpegts::Packetizer>();

		std::shared_ptr<MediaTrack> first_video_track = nullptr;
		std::shared_ptr<MediaTrack> first_audio_track = nullptr;
		std::shared_ptr<MediaTrack> first_data_track = nullptr;

		for (const auto &[id, track] : GetTracks())
		{
			switch (track->GetMediaType())
			{
				case cmn::MediaType::Video:
					SRT_SET_TRACK(track, first_video_track,
								  mpegts::Packetizer::IsSupportedCodec(track->GetCodecId()),
								  "Ignore unsupported video codec (%s)", StringFromMediaCodecId(track->GetCodecId()).CStr());
					break;

				case cmn::MediaType::Audio:
					SRT_SET_TRACK(track, first_audio_track,
								  mpegts::Packetizer::IsSupportedCodec(track->GetCodecId()),
								  "Ignore unsupported audio codec (%s)", StringFromMediaCodecId(track->GetCodecId()).CStr());
					break;

				case cmn::MediaType::Data:
					SRT_SET_TRACK(track, first_data_track, true, );
					break;

				default:
					logad("SrtStream - Ignore unsupported media type(%s)", GetMediaTypeString(track->GetMediaType()).CStr());
					continue;
			}
		}

		if ((first_video_track == nullptr) && (first_audio_track == nullptr))
		{
			logaw("There is no track to create SRT stream");
			return false;
		}

		bool result = ((first_video_track != nullptr) ? packetizer->AddTrack(first_video_track) : true) &&
					  ((first_audio_track != nullptr) ? packetizer->AddTrack(first_audio_track) : true) &&
					  ((first_data_track != nullptr) ? packetizer->AddTrack(first_data_track) : true);

		if (result == false)
		{
			logae("Failed to add track to packetizer");
			return false;
		}

		_psi_data->Clear();
		_data_to_send->Clear();

		if (packetizer->AddSink(GetSharedPtrAs<mpegts::PacketizerSink>()))
		{
			std::unique_lock lock(_packetizer_mutex);
			_packetizer = packetizer;
		}
		else
		{
			logae("Could not initialize packetizer for SRT Stream");
			return false;
		}

		return packetizer->Start() && Stream::Start();
	}

	bool SrtStream::Stop()
	{
		if (GetState() != Stream::State::STARTED)
		{
			return false;
		}

		logad("The SRT stream has been stopped");

		auto linked_input_stream = GetLinkedInputStream();

		if ((linked_input_stream != nullptr) && (linked_input_stream->IsFromOriginMapStore() == false))
		{
			// Unregister stream if OriginMapStore is enabled
			auto result = ocst::Orchestrator::GetInstance()->UnregisterStreamFromOriginMapStore(GetApplicationInfo().GetVHostAppName(), GetName());

			if (result == CommonErrorCode::ERROR)
			{
				logaw("Failed to unregister stream from origin map store");
				return false;
			}
		}

		std::shared_ptr<mpegts::Packetizer> packetizer;

		{
			std::unique_lock lock(_packetizer_mutex);
			packetizer = std::move(_packetizer);
		}

		OV_ASSERT2(packetizer != nullptr);

		if (packetizer != nullptr)
		{
			packetizer->Stop();
			packetizer.reset();
		}

		return Stream::Stop();
	}

	void SrtStream::EnqueuePacket(const std::shared_ptr<MediaPacket> &media_packet)
	{
		if (GetState() != Stream::State::STARTED)
		{
			return;
		}

		std::unique_lock lock(_packetizer_mutex);

		if (_packetizer == nullptr)
		{
			OV_ASSERT2(false);
#if DEBUG
			logaw("Packetizer is not initialized");
#endif	// DEBUG
			return;
		}

		_packetizer->AppendFrame(media_packet);
	}

	void SrtStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
	{
		EnqueuePacket(media_packet);
	}

	void SrtStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
	{
		EnqueuePacket(media_packet);
	}

	void SrtStream::SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet)
	{
		EnqueuePacket(media_packet);
	}

	void SrtStream::BroadcastIfReady(const std::vector<std::shared_ptr<mpegts::Packet>> &packets)
	{
		std::vector<std::shared_ptr<ov::Data>> data_list;
		size_t total_data_size = 0;

		{
			for (auto &packet : packets)
			{
				auto size = _data_to_send->GetLength();
				const auto &data = packet->GetData();

				if ((size + data->GetLength()) > SRT_LIVE_DEF_PLSIZE)
				{
					total_data_size += _data_to_send->GetLength();
					data_list.push_back(std::move(_data_to_send));

					_data_to_send = data->Clone();
				}
				else
				{
					_data_to_send->Append(packet->GetData());
				}
			}
		}

		for (const auto &data : data_list)
		{
			BroadcastPacket(std::make_any<std::shared_ptr<const ov::Data>>(data));
		}

		MonitorInstance->IncreaseBytesOut(
			*GetSharedPtrAs<info::Stream>(),
			PublisherType::Srt,
			total_data_size * GetSessionCount());
	}

	void SrtStream::OnPsi(const std::vector<std::shared_ptr<const MediaTrack>> &tracks, const std::vector<std::shared_ptr<mpegts::Packet>> &psi_packets)
	{
		std::shared_ptr<ov::Data> psi_data = std::make_shared<ov::Data>();

		for (const auto &packet : psi_packets)
		{
			psi_data->Append(packet->GetData());
		}

		logap("OnPsi - %zu packets (total %zu bytes)", psi_packets.size(), psi_data->GetLength());

		{
			std::unique_lock lock(_psi_data_mutex);
			_psi_data = std::move(psi_data);
		}

		BroadcastIfReady(psi_packets);
	}

	void SrtStream::OnFrame(const std::shared_ptr<const MediaPacket> &media_packet, const std::vector<std::shared_ptr<mpegts::Packet>> &pes_packets)
	{
#if DEBUG
		// Since adding up the total packet size is costly, it is calculated only in debug mode
		size_t total_packet_size = 0;

		for (const auto &packet : pes_packets)
		{
			total_packet_size += packet->GetData()->GetLength();
		}

		logap("OnFrame - %zu packets (total %zu bytes)", pes_packets.size(), total_packet_size);
#endif	// DEBUG

		BroadcastIfReady(pes_packets);
	}
}  // namespace pub
