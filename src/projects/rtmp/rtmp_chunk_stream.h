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

#include <base/ovlibrary/ovlibrary.h>
#include <base/common_types.h>
#include <base/provider/stream.h>
#include <base/ovsocket/ovsocket.h>
#include <base/application/application.h>
#include <config/config.h>

#include "chunk/rtmp_import_chunk.h"
#include "chunk/rtmp_export_chunk.h"
#include "chunk/rtmp_handshake.h"
#include "chunk/amf_document.h"

//====================================================================================================
// Interface
//====================================================================================================
class IRtmpChunkStream
{
public:
	virtual bool OnChunkStreamReadyComplete(ov::ClientSocket *remote,
	                                        ov::String &app_name, ov::String &stream_name,
	                                        std::shared_ptr<RtmpMediaInfo> &media_info,
	                                        info::application_id_t &applicaiton_id,
	                                        uint32_t &stream_id) = 0;

	virtual bool OnChunkStreamVideoData(ov::ClientSocket *remote,
	                                    info::application_id_t applicaiton_id,
	                                    uint32_t stream_id,
	                                    uint32_t timestamp,
	                                    RtmpFrameType frame_type,
	                                    std::shared_ptr<std::vector<uint8_t>> &data) = 0;

	virtual bool OnChunkStreamAudioData(ov::ClientSocket *remote,
	                                    info::application_id_t applicaiton_id,
	                                    uint32_t stream_id,
	                                    uint32_t timestamp,
	                                    RtmpFrameType frame_type,
	                                    std::shared_ptr<std::vector<uint8_t>> &data) = 0;

	virtual bool OnDeleteStream(ov::ClientSocket *remote,
	                            ov::String &app_name,
	                            ov::String &stream_name,
	                            info::application_id_t applicaiton_id,
	                            uint32_t stream_id) = 0;
};

//====================================================================================================
// RtmpChunkStream
//====================================================================================================
class RtmpChunkStream : public ov::EnableSharedFromThis<RtmpChunkStream>
{
public:
	RtmpChunkStream(ov::ClientSocket *remote, IRtmpChunkStream *stream_interface);

	~RtmpChunkStream() override = default;

public:
	int32_t OnDataReceived(const std::unique_ptr<std::vector<uint8_t>> &data);

	static ov::String GetCodecString(RtmpCodecType codec_type);

	static ov::String GetEncoderTypeString(RtmpEncoderType encoder_type);

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

private :
	bool SendData(int data_size, uint8_t *data);

	int32_t ReceiveHandshakePacket(const std::shared_ptr<const std::vector<uint8_t>> &data);

	int32_t ReceiveChunkPacket(const std::shared_ptr<const std::vector<uint8_t>> &data);

	bool SendHandshake(const std::shared_ptr<const std::vector<uint8_t>> &data);

	bool ReceiveChunkMessage();

	bool ReceiveSetChunkSize(std::shared_ptr<ImportMessage> &message);

	void ReceiveWindowAcknowledgementSize(std::shared_ptr<ImportMessage> &message);

	void ReceiveAmfCommandMessage(std::shared_ptr<ImportMessage> &message);

	void ReceiveAmfDataMessage(std::shared_ptr<ImportMessage> &message);

	bool ReceiveAudioMessage(std::shared_ptr<ImportMessage> &message);

	bool ReceiveVideoMessage(std::shared_ptr<ImportMessage> &message);

	void
	OnAmfConnect(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);

	void OnAmfCreateStream(std::shared_ptr<RtmpMuxMessageHeader> &message_header,
	                       AmfDocument &document,
	                       double transaction_id);

	void
	OnAmfFCPublish(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);

	void
	OnAmfPublish(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);

	void OnAmfDeleteStream(std::shared_ptr<RtmpMuxMessageHeader> &message_header,
	                       AmfDocument &document,
	                       double transaction_id);

	bool
	OnAmfMetaData(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, int32_t object_index);

	bool SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header,
	                       std::shared_ptr<std::vector<uint8_t>> &data);

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

	bool VideoSequenceHeaderProcess(std::shared_ptr<std::vector<uint8_t>> &data, uint8_t control_byte);

	bool AudioSequenceHeaderProcess(std::shared_ptr<std::vector<uint8_t>> &data, uint8_t control_byte);

	bool StreamCreate();

protected :
	ov::ClientSocket *_remote;
	IRtmpChunkStream *_stream_interface;
	ov::String _app_name;
	ov::String _stream_name;
	info::application_id_t _app_id;
	uint32_t _stream_id;
	ov::String _device_string;

	std::unique_ptr<std::vector<uint8_t>> _remained_data;
	RtmpHandshakeState _handshake_state;
	std::unique_ptr<RtmpImportChunk> _import_chunk;
	std::unique_ptr<RtmpExportChunk> _export_chunk;
	std::shared_ptr<RtmpMediaInfo> _media_info;

	uint32_t _rtmp_stream_id;
	uint32_t _peer_bandwidth;
	uint32_t _acknowledgement_size;
	uint32_t _acknowledgement_traffic;
	double _client_id;
	int32_t _chunk_stream_id;

	// video/audio sequence 완료 후에 스트림 생성
	std::vector<std::shared_ptr<ImportMessage>> _stream_messages;
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

};
