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

#include "../base/common_types.h"
#include "../base/provider/stream.h"
#include "../base/ovsocket/ovsocket.h"

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
	virtual bool OnChunkStreamReadyComplete(ov::ClientSocket *	remote, ov::String &app_name, ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, uint32_t &app_id, uint32_t &stream_id) = 0;
	virtual bool OnChunkStreamVideoData(ov::ClientSocket *remote, uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) = 0;
	virtual bool OnChunkStreamAudioData(ov::ClientSocket *remote, uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) = 0;
	virtual bool OnChunkStreamDelete(ov::ClientSocket *remote, ov::String &app_name, ov::String &stream_name, uint32_t app_id, uint32_t stream_id) = 0;
};

//====================================================================================================
// RtmpChunkStream
//====================================================================================================
class RtmpChunkStream : public ov::EnableSharedFromThis<RtmpChunkStream>
{
public:
	RtmpChunkStream(ov::ClientSocket *remote, IRtmpChunkStream * stream_interface);
    ~RtmpChunkStream() override = default;

public:
    int32_t	OnDataReceived(const std::unique_ptr<std::vector<uint8_t>> &data);

private :
    bool   SendData(int data_size, uint8_t * data);

	int32_t	ReceiveHandshakePacket(const std::shared_ptr<const std::vector<uint8_t>> &data);
	int32_t	ReceiveChunkPacket(const std::shared_ptr<const std::vector<uint8_t>> &data);

	bool	SendHandshake(const std::shared_ptr<const std::vector<uint8_t>> &data);

	bool	ReceiveChunkMessage();
	bool	ReceiveSetChunkSize(std::shared_ptr<ImportMessage> &message);
	void	ReceiveWindowAcknowledgementSize(std::shared_ptr<ImportMessage> &message);
	void	ReceiveAmfCommandMessage(std::shared_ptr<ImportMessage> &message);
	void	ReceiveAmfDataMessage(std::shared_ptr<ImportMessage> &message);
	bool	ReceiveAudioMessage(std::shared_ptr<ImportMessage> &message);
	bool	ReceiveVideoMessage(std::shared_ptr<ImportMessage> &message);

	void 	OnAmfConnect(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);
	void 	OnAmfCreateStream(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);
	void 	OnAmfFCPublish(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);
	void 	OnAmfPublish(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);
	void 	OnAmfDeleteStream(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id);
	bool 	OnAmfMetaData(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, int32_t object_index);

	bool	SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data);
	bool	SendUserControlMessage(uint8_t message, std::shared_ptr<std::vector<uint8_t>> &data);

	bool	SendWindowAcknowledgementSize();
	bool	SendSetPeerBandwidth();
	bool	SendStreamBegin();
	bool	SendAcknowledgementSize();

	bool	SendAmfCommand(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document);
	bool	SendAmfConnectResult(uint32_t chunk_stream_id, double transaction_id, double object_encoding);
	bool	SendAmfOnFCPublish(uint32_t chunk_stream_id, uint32_t stream_id, double client_id );
	bool	SendAmfCreateStreamResult(uint32_t chunk_stream_id, double transaction_id);
	bool	SendAmfOnStatus(uint32_t chunk_stream_id, uint32_t stream_id, char * level, char * code, char * description, double client_id);

	bool 	ProcessVideoSequenceData(std::unique_ptr<std::vector<uint8_t>> data);
	bool 	ProcessAudioSequenceData(std::unique_ptr<std::vector<uint8_t>> data);

protected :
	ov::ClientSocket *						_remote;
	IRtmpChunkStream *  					_stream_interface;
	ov::String								_app_name;
	ov::String          					_app_stream_name;
	uint32_t            					_app_id;
	uint32_t            					_app_stream_id;

	std::unique_ptr<std::vector<uint8_t>> 	_remained_data;
	tRTMP_HANDSHAKE_STATE   				_handshake_state;
	std::unique_ptr<RtmpImportChunk> 		_import_chunk;
	std::unique_ptr<RtmpExportChunk> 		_export_chunk;
	bool 									_delete_stream;
	std::shared_ptr<RtmpMediaInfo> 			_media_info;

	uint32_t								_stream_id;
	uint32_t								_peer_bandwidth;
	uint32_t								_acknowledgement_size;
	uint32_t								_acknowledgement_traffic;
	double 								_client_id;
    int32_t									_chunk_stream_id;

};