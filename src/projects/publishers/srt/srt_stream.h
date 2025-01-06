//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>
#include <modules/containers/mpegts/mpegts_packetizer.h>

#include "monitoring/monitoring.h"

namespace pub
{
	class SrtStream final : public Stream, public mpegts::PacketizerSink
	{
	public:
		static std::shared_ptr<SrtStream> Create(const std::shared_ptr<Application> application,
												 const info::Stream &info,
												 uint32_t worker_count);
		explicit SrtStream(const std::shared_ptr<Application> application,
						   const info::Stream &info,
						   uint32_t worker_count);
		~SrtStream() override final;

		//--------------------------------------------------------------------
		// Overriding of Stream
		//--------------------------------------------------------------------
		void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
		void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
		void SendDataFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
		//--------------------------------------------------------------------

		//--------------------------------------------------------------------
		// Implementation of mpegts::PacketizerSink
		//--------------------------------------------------------------------
		void OnPsi(const std::vector<std::shared_ptr<const MediaTrack>> &tracks, const std::vector<std::shared_ptr<mpegts::Packet>> &psi_packets) override;
		void OnFrame(const std::shared_ptr<const MediaPacket> &media_packet, const std::vector<std::shared_ptr<mpegts::Packet>> &pes_packets) override;
		//--------------------------------------------------------------------

		std::shared_ptr<const ov::Data> GetPsiData() const
		{
			std::shared_lock lock(_psi_data_mutex);

			return _psi_data;
		}

	private:
		bool Start() override;
		bool Stop() override;

		void SetTrack(const std::shared_ptr<MediaTrack> &from, std::shared_ptr<MediaTrack> *to);

		void EnqueuePacket(const std::shared_ptr<MediaPacket> &media_packet);
		void BroadcastIfReady(const std::vector<std::shared_ptr<mpegts::Packet>> &packets);

		uint32_t _worker_count = 0;

		std::shared_mutex _packetizer_mutex;
		std::shared_ptr<mpegts::Packetizer> _packetizer;

		mutable std::shared_mutex _psi_data_mutex;
		std::shared_ptr<ov::Data> _psi_data = std::make_shared<ov::Data>();
		std::shared_ptr<ov::Data> _data_to_send = std::make_shared<ov::Data>();
	};
}  // namespace pub
