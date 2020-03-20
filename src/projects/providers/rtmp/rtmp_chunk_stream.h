//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define _CONSOLE

#include <base/info/application.h>
#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>
#include <base/provider/stream.h>
#include <config/config.h>

#include "providers/rtmp/chunk/amf_document.h"
#include "providers/rtmp/chunk/rtmp_chunk_parser.h"
#include "providers/rtmp/chunk/rtmp_export_chunk.h"
#include "providers/rtmp/chunk/rtmp_handshake.h"
#include "providers/rtmp/chunk/rtmp_import_chunk.h"

class IRtmpChunkStream
{
public:
	virtual bool OnChunkStreamReady(ov::ClientSocket *remote,
									ov::String &app_name, ov::String &stream_name,
									std::shared_ptr<RtmpMediaInfo> &media_info,
									info::application_id_t &application_id, uint32_t &stream_id) = 0;

	virtual bool OnChunkStreamVideoData(ov::ClientSocket *remote,
										info::application_id_t application_id, uint32_t stream_id,
										uint64_t timestamp,
										RtmpFrameType frame_type,
										const std::shared_ptr<const ov::Data> &data) = 0;

	virtual bool OnChunkStreamAudioData(ov::ClientSocket *remote,
										info::application_id_t application_id, uint32_t stream_id,
										uint64_t timestamp,
										RtmpFrameType frame_type,
										const std::shared_ptr<const ov::Data> &data) = 0;

	virtual bool OnDeleteStream(ov::ClientSocket *remote,
								ov::String &app_name, ov::String &stream_name,
								info::application_id_t application_id, uint32_t stream_id) = 0;
};

class RtmpChunkStream : public ov::EnableSharedFromThis<RtmpChunkStream>
{
public:
	RtmpChunkStream(ov::ClientSocket *remote, IRtmpChunkStream *stream_interface);
	~RtmpChunkStream() override = default;

	int32_t OnDataReceived(const std::shared_ptr<const ov::Data> &data);

	static ov::String GetCodecString(RtmpCodecType codec_type);

	static ov::String GetEncoderTypeString(RtmpEncoderType encoder_type);

	ov::String &GetDomainName()
	{
		return _domain_name;
	}

	ov::String &GetAppName()
	{
		return _app_name;
	}

	ov::String &GetStreamName()
	{
		return _stream_name;
	}

	info::application_id_t GetAppId()
	{
		return _app_id;
	}

	uint32_t GetStreamId()
	{
		return _stream_id;
	}

	time_t GetLastPacketTime()
	{
		return _last_packet_time;
	}

	bool Close()
	{
		return _remote->Close();
	}

	ov::ClientSocket *GetRemoteSocket()
	{
		return _remote;
	}

private:
	bool SendData(int data_size, uint8_t *data);

	off_t ReceiveHandshakePacket(const std::shared_ptr<const ov::Data> &data);

	// This function will called after OnMetaData
	int32_t ReceiveChunkPacket(const std::shared_ptr<const ov::Data> &data);

	bool SendHandshake(const std::shared_ptr<const ov::Data> &data);

	bool ReceiveChunkMessage();

	bool ReceiveSetChunkSize(const std::shared_ptr<const RtmpMessage> &message);

	void ReceiveWindowAcknowledgementSize(const std::shared_ptr<const RtmpMessage> &message);

	void ReceiveAmfCommandMessage(const std::shared_ptr<const RtmpMessage> &message);

	void ReceiveAmfDataMessage(const std::shared_ptr<const RtmpMessage> &message);

	bool ReceiveAudioMessage(const std::shared_ptr<const RtmpMessage> &message);

	bool ReceiveVideoMessage(const std::shared_ptr<const RtmpMessage> &message);

	void OnAmfConnect(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
	void OnAmfCreateStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
	void OnAmfFCPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
	void OnAmfPublish(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
	void OnAmfDeleteStream(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, double transaction_id);
	bool OnAmfMetaData(const std::shared_ptr<const RtmpChunkHeader> &header, AmfDocument &document, int32_t object_index);

	bool SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data);

	bool SendUserControlMessage(uint16_t message, std::shared_ptr<std::vector<uint8_t>> &data);

	bool SendWindowAcknowledgementSize();

	bool SendSetPeerBandwidth();

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

	bool VideoSequenceHeaderProcess(const std::shared_ptr<const ov::Data> &data, uint8_t control_byte);

	bool AudioSequenceHeaderProcess(const std::shared_ptr<const ov::Data> &data, uint8_t control_byte);

	bool StreamCreate();

protected:
	ov::ClientSocket *_remote;
	IRtmpChunkStream *_stream_interface;
	ov::String _domain_name;
	ov::String _app_name;
	ov::String _stream_name;
	info::application_id_t _app_id;
	uint32_t _stream_id;
	ov::String _device_string;

	std::shared_ptr<ov::Data> _remained_data;
	RtmpHandshakeState _handshake_state;
	std::shared_ptr<RtmpImportChunk> _import_chunk;
	std::shared_ptr<RtmpExportChunk> _export_chunk;
	std::shared_ptr<RtmpMediaInfo> _media_info;

	uint32_t _rtmp_stream_id;
	uint32_t _peer_bandwidth;
	uint32_t _acknowledgement_size;
	uint32_t _acknowledgement_traffic;
	double _client_id;
	int32_t _chunk_stream_id;

	// video/audio sequence 완료 후에 스트림 생성
	std::vector<std::shared_ptr<const RtmpMessage>> _stream_messages;
	bool _video_sequence_info_process;
	bool _audio_sequence_info_process;

	time_t _stream_check_time;
	uint32_t _key_frame_interval = 0;
	uint32_t _previous_key_frame_timestamp;
	uint32_t _last_video_timestamp = 0;
	uint32_t _last_audio_timestamp = 0;
	uint32_t _previous_last_video_timestamp = 0;
	uint32_t _previous_last_audio_timestamp = 0;
	uint32_t _video_frame_count = 0;
	uint32_t _audio_frame_count = 0;

	time_t _last_packet_time;

	ov::StopWatch _stat_stop_watch;
};
