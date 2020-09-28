//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/common_types.h"
#include "base/provider/push_provider/stream.h"

#include "chunk/amf_document.h"
#include "chunk/rtmp_chunk_parser.h"
#include "chunk/rtmp_export_chunk.h"
#include "chunk/rtmp_handshake.h"
#include "chunk/rtmp_import_chunk.h"

#define MAX_STREAM_MESSAGE_COUNT (100)
#define BASELINE_PROFILE (66)
#define MAIN_PROFILE (77)

// Fix track id
#define RTMP_VIDEO_TRACK_ID		0
#define RTMP_AUDIO_TRACK_ID		1

namespace pvd
{
	class RtmpStream : public pvd::PushStream
	{
	public:
		static std::shared_ptr<RtmpStream> Create(StreamSourceType source_type, uint32_t channel_id, const std::shared_ptr<ov::Socket> &client_socket, const std::shared_ptr<PushProvider> &provider);
		
		explicit RtmpStream(StreamSourceType source_type, uint32_t channel_id, std::shared_ptr<ov::Socket> client_socket, const std::shared_ptr<PushProvider> &provider);
		~RtmpStream() final;

		bool Stop() override;

		// bool ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts);
		// bool ConvertToAudioData(const std::shared_ptr<ov::Data> &data);

		// ------------------------------------------
		// Implementation of PushStream
		// ------------------------------------------
		PushStreamType GetPushStreamType() override
		{
			return PushStream::PushStreamType::INTERLEAVED;
		}
		bool OnDataReceived(const std::shared_ptr<const ov::Data> &data) override;

	protected:
		bool Start() override;

		bool ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts);
		bool ConvertToAudioData(const std::shared_ptr<ov::Data> &data);
		
	private:
		// AMF Event
		void OnAmfConnect(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
		void OnAmfCreateStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
		void OnAmfFCPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
		void OnAmfPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
		void OnAmfDeleteStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
		bool OnAmfMetaData(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, int32_t object_index);


		// Send messages
		bool SendData(int data_size, uint8_t *data);
		bool SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data);
		bool SendAcknowledgementSize(uint32_t acknowledgement_traffic);

		bool SendUserControlMessage(uint16_t message, std::shared_ptr<std::vector<uint8_t>> &data);
		bool SendWindowAcknowledgementSize(uint32_t size);
		bool SendSetPeerBandwidth(uint32_t bandwidth);
		bool SendStreamBegin();
		bool SendAcknowledgementSize();
		bool SendAmfCommand(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document);
		bool SendAmfConnectResult(uint32_t chunk_stream_id, double transaction_id, double object_encoding);
		bool SendAmfOnFCPublish(uint32_t chunk_stream_id, uint32_t stream_id, double client_id);
		bool SendAmfCreateStreamResult(uint32_t chunk_stream_id, double transaction_id);
		bool SendAmfOnStatus(uint32_t chunk_stream_id,
							uint32_t stream_id,
							char *level,
							char *code,
							char *description,
							double client_id);

		// Parsing handshake messages
		off_t ReceiveHandshakePacket(const std::shared_ptr<const ov::Data> &data);
		bool SendHandshake(const std::shared_ptr<const ov::Data> &data);

		// Parsing chunk messages
		int32_t ReceiveChunkPacket(const std::shared_ptr<const ov::Data> &data);
		bool ReceiveChunkMessage();

		bool ReceiveSetChunkSize(const std::shared_ptr<const RtmpMessage> &message);
		void ReceiveWindowAcknowledgementSize(const std::shared_ptr<const RtmpMessage> &message);
		void ReceiveAmfCommandMessage(const std::shared_ptr<const RtmpMessage> &message);
		void ReceiveAmfDataMessage(const std::shared_ptr<const RtmpMessage> &message);

		bool ReceiveAudioMessage(const std::shared_ptr<const RtmpMessage> &message);
		bool ReceiveVideoMessage(const std::shared_ptr<const RtmpMessage> &message);

		ov::String GetCodecString(RtmpCodecType codec_type);
		ov::String GetEncoderTypeString(RtmpEncoderType encoder_type);

		bool CheckReadyToPublish();
		bool PublishStream();
		bool SetTrackInfo(const std::shared_ptr<RtmpMediaInfo> &media_info);

		// RTMP related
		RtmpHandshakeState _handshake_state = RtmpHandshakeState::Uninitialized;
		
		std::shared_ptr<RtmpImportChunk> _import_chunk;
		std::shared_ptr<RtmpExportChunk> _export_chunk;
		std::shared_ptr<RtmpMediaInfo> _media_info;

		std::vector<std::shared_ptr<const RtmpMessage>> _stream_message_cache;
		uint32_t _stream_message_cache_video_count = 0;
		uint32_t _stream_message_cache_audio_count = 0;

		uint32_t _acknowledgement_size = RTMP_DEFAULT_ACKNOWNLEDGEMENT_SIZE / 2;
		uint32_t _acknowledgement_traffic = 0;
		uint32_t _rtmp_stream_id = 1;
		uint32_t _peer_bandwidth = RTMP_DEFAULT_PEER_BANDWIDTH;
		double _client_id = 12345.0;
		// Set from OnAmfPublish
		int32_t _chunk_stream_id = 0;

		// parsed from packet
		ov::String _domain_name;
		info::VHostAppName _app_name;
		ov::String _stream_name;
		ov::String _device_string;

		// Cache (GetApplicationInfo()->GetId())
		info::application_id_t _app_id = 0;

		// For sending data
		std::shared_ptr<ov::Socket> _remote = nullptr;

		// Received data buffer
		std::shared_ptr<ov::Data> 	_remained_data = nullptr;

		// For statistics 
		time_t _stream_check_time = 0;

		uint32_t _key_frame_interval = 0;
		uint32_t _previous_key_frame_timestamp = 0;
		uint32_t _last_video_timestamp = 0;
		uint32_t _last_audio_timestamp = 0;
		uint32_t _previous_last_video_timestamp = 0;
		uint32_t _previous_last_audio_timestamp = 0;
		uint32_t _video_frame_count = 0;
		uint32_t _audio_frame_count = 0;
	};
}