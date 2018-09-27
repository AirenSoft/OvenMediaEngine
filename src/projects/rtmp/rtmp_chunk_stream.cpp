//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <string>
#include "rtmp_chunk_stream.h"

/*Publishing 절차
- Handshakeing
 C->S : C0 + C1
 S->C : S0 + S1 + S2
 C->S : C2

- Connect
 C->S : Set Chunk Size
 C->S : Connect
 S->C : WindowAcknowledgement Size
 S->C : Set Peer Bandwidth
 C->S : Window Acknowledgement Size
 S->C : Stream Begin
 S->C : Set Chunk Size
 S->C : _result

- Publishing
 C->S : releaseStream
 C->S : FCPublish
 C->S : createStream
 S->C : _result
 C->S : publish

 S->C : Stream Begin
 S->C : onStatus(Start)
 C->S : @setDataFrame
 C->S : Video/Audio Data Stream
 - H.264 : SPS/PPS 
 - AAC : Control Byte 
*/

#define OV_LOG_TAG "RtmpProvider"

//====================================================================================================
// RtmpChunkStream
//====================================================================================================
RtmpChunkStream::RtmpChunkStream(ov::ClientSocket *remote, IRtmpChunkStream * stream_interface)
{
    OV_ASSERT2(remote != nullptr);
    OV_ASSERT2(stream_interface != nullptr);

    _remote                         = remote;
    _stream_interface               = stream_interface;
    _remained_data                  = std::make_unique<std::vector<uint8_t>>();
    _app_id                         = 0;
    _app_stream_id                  = 0;

    _import_chunk = std::make_shared<RtmpImportChunk>(RTMP_DEFAULT_CHUNK_SIZE);
    _export_chunk = std::make_shared<RtmpExportChunk>(false, RTMP_DEFAULT_CHUNK_SIZE);
    _media_info = std::make_shared<RtmpMediaInfo>();

    _delete_stream                  = false;
    _stream_id					    = 0;
    _handshake_state                = RTMP_HANDSHAKE_READY_STATE;
    _peer_bandwidth				    = RTMP_DEFAULT_PEER_BANDWIDTH;
    _acknowledgement_size		    = RTMP_DEFAULT_ACKNOWNLEDGEMENT_SIZE/2;
    _acknowledgement_traffic	    = 0;
    _client_id					    = 12345.0;
    _chunk_stream_id			    = 0;
}

//====================================================================================================
// ~RtmpChunkStream
//====================================================================================================
RtmpChunkStream::~RtmpChunkStream()
{

}

//====================================================================================================
//  패킷 전송
// - 1M단위 제한
//====================================================================================================
bool RtmpChunkStream::SendData(ssize_t data_size, uint8_t * data)
{
    ssize_t     remained        = data_size;
    uint8_t *   data_to_send    = data;

    while(remained > 0L)
    {
        ssize_t to_send = std::min(remained, (1024L * 1024L));
        ssize_t sent = _remote->Send(data_to_send, to_send);

        if(sent != to_send)
        {
            logtw("Send Data Loop Fail");
            return false;
        }

        remained -= sent;
        data_to_send += sent;
    }

    return true;
}

//====================================================================================================
//  패킷 수신
// - Handshake 처리
// - Chunkstream 처리
//====================================================================================================
int32_t RtmpChunkStream::OnDataReceived(const std::unique_ptr<std::vector<uint8_t>> &data)
{
    int32_t process_size = 0;
    std::shared_ptr<std::vector<uint8_t>> process_data = nullptr;

    if(!_remained_data->empty())
    {
        process_data = std::make_shared<std::vector<uint8_t>>(_remained_data->begin(), _remained_data->end());
        process_data->insert(process_data->end(), data->begin(), data->end());
        _remained_data->clear();
    }
    else
    {
        process_data = std::make_shared<std::vector<uint8_t>>(data->begin(), data->end());
    }

    // 최대 크기 확인
    if(process_data->size() > RTMP_MAX_PACKET_SIZE)
    {
        logte("Process data size fail - DataSize(%d:%d)", process_data->size(), RTMP_MAX_PACKET_SIZE);
        return -1;
    }

    if(_handshake_state != RTMP_HANDSHAKE_COMPLETE_STATE)    process_size = RecvHandshakePacket(process_data);
    else                                                     process_size = RecvChunkPacket(process_data);

    if(process_size < 0)
    {
        logte("Process Size Fail - Size(%d)", process_size);
        return -1;
    }

    // remained 데이터 설정
    if(process_size < process_data->size())
    {
        _remained_data->assign(process_data->begin() + process_size, process_data->end());
    }

    return process_size;
}


//====================================================================================================
// Recv Handshake Packet
// c0 + c1 Recv
// s0 + s1 + s2 Send
// c2 Recv
//====================================================================================================
int32_t RtmpChunkStream::RecvHandshakePacket(const std::shared_ptr<const std::vector<uint8_t>> &data)
{
    int32_t process_size = 0;
    int32_t chunk_process_size = 0;

    if      (_handshake_state == RTMP_HANDSHAKE_READY_STATE) process_size = (sizeof(uint8_t) + RTMP_HANDSHAKE_PACKET_SIZE);// c0 + c1
    else if( _handshake_state == RTMP_HANDSHAKE_S2_STATE)    process_size = (RTMP_HANDSHAKE_PACKET_SIZE);// c2
    else
    {
        logte("Handshake State Fail - State(%d)", (int32_t)_handshake_state);
        return -1;
    }
    
    // Process Data Size Check 
    if(data->size() < process_size )
    {
        return 0;
    }

    // c0 + c1 / s0 + s1 + s2
    if(_handshake_state == RTMP_HANDSHAKE_READY_STATE)
    {
        // c0 + c1 수신 확인
        // 버전 체크
        if(data->at(0) != RTMP_HANDSHAKE_VERSION )
        {
            logte("Handshake Version Fail - Version(%d:%d)", data->at(0), RTMP_HANDSHAKE_VERSION);
            return -1;
        }
        _handshake_state = RTMP_HANDSHAKE_C0_STATE;

        // S0,S1,S2 전송
        if(!SendHandshake(data))
        {
           return -1;
        }

        return process_size;
    }

    _handshake_state = RTMP_HANDSHAKE_C2_STATE;

    // 최종 c3와 chunk 패킷이 같이 들어오는 경우 처리(encoder 전송 대기 상태에 빠질 수 있음)
    if(process_size < data->size())
    {
        auto  process_data = std::make_shared<std::vector<uint8_t>>(data->begin() + process_size, data->end());
        chunk_process_size = RecvChunkPacket(process_data);
        if(chunk_process_size < 0)
        {
            return -1;
        }
        process_size += chunk_process_size;
    }

    _handshake_state = RTMP_HANDSHAKE_COMPLETE_STATE;
    logtd("Handshake Complete");
    return process_size;
}

//====================================================================================================
// Handshake 전송
// s0 + s1 + s2
//====================================================================================================
bool RtmpChunkStream::SendHandshake(const std::shared_ptr<const std::vector<uint8_t>> &data)
{
    uint8_t	s0								= 0;
    uint8_t	s1[RTMP_HANDSHAKE_PACKET_SIZE]	= {0, };
    uint8_t	s2[RTMP_HANDSHAKE_PACKET_SIZE] 	= {0, };

    // 데이터 설정
    s0 = RTMP_HANDSHAKE_VERSION;
    RtmpHandshake::MakeS1(s1);
    RtmpHandshake::MakeS2((uint8_t *)data->data() + sizeof(uint8_t), s2);
    _handshake_state = RTMP_HANDSHAKE_C1_STATE;

    // s0 전송
    if(!SendData(sizeof(s0), &s0))
    {
        logte("Handshake s0 Send Fail");
        return false;
    }
    _handshake_state = RTMP_HANDSHAKE_S0_STATE;

    // s1 전송
    if(!SendData(sizeof(s1), s1))
    {
        logte("Handshake s1 Send Fail");
        return false;
    }
    _handshake_state = RTMP_HANDSHAKE_S1_STATE;

    // s2 전송
    if(!SendData(sizeof(s2), s2))
    {
        logte("Handshake s2 Send Fail");
        return false;
    }
    _handshake_state = RTMP_HANDSHAKE_S2_STATE;

    return true;
}

//====================================================================================================
// Chunk 패킷 수신
// - OnMetaData 이후에 스트리밍 시작
//====================================================================================================
int32_t RtmpChunkStream::RecvChunkPacket(const std::shared_ptr<const std::vector<uint8_t>> &data)
{
    int32_t process_size 		= 0;
    int32_t	import_size			= 0;
    bool	message_complete 	= false;

    while(process_size < data->size())
    {
        message_complete = false;

        import_size = _import_chunk->ImportStreamData((uint8_t *)data->data() + process_size, data->size() - process_size, message_complete);

        if(import_size == 0)
        {
            break;
        }
        else if(import_size < 0)
        {
            logte("ImportStream Fail");
            return import_size;
        }

        if(message_complete)
        {
            if(!RecvChunkMessageProc())
            {
                logte("RecvChunkMessageProc Fail");
                return -1;
            }
        }

        process_size += import_size;
    }

    //Acknowledgement append
    _acknowledgement_traffic += process_size;

    if(_acknowledgement_traffic > _acknowledgement_size)
    {
        SendAcknowledgementSize();

        // Init
        _acknowledgement_traffic = 0;
    }

    return process_size;
}

//====================================================================================================
// ChunkMessage 처리
//====================================================================================================
bool RtmpChunkStream::RecvChunkMessageProc()
{
    // 메시지 처리(지연 발생시 thread에서  처리)
    while(true)
    {
        auto message =  _import_chunk->GetMessage();

        if(message == nullptr || message->body == nullptr)
        {
            break;
        }

        if(message->message_header->body_size > RTMP_MAX_PACKET_SIZE)
        {
            logte("Packet Size Fail - Size(%u:%u)", message->message_header->body_size, RTMP_MAX_PACKET_SIZE);
            return false;
        }

        bool bSuccess = true;

        //처리
        switch(message->message_header->type_id)
        {
            case RTMP_MSGID_AUDIO_MESSAGE:				    bSuccess = RecvAudioMessage(message);	break;// 에러 종료 처리 필요
            case RTMP_MSGID_VIDEO_MESSAGE:				    bSuccess = RecvVideoMessage(message);   break;// 에러 종료 처리 필요
            case RTMP_MSGID_SET_CHUNKSIZE :				    bSuccess = RecvSetChunkSize(message);	break;// 에러 종료 처리 필요
            case RTMP_MSGID_AMF0_DATA_MESSAGE :			    RecvAmfDataMessage(message);			break;
            case RTMP_MSGID_AMF0_COMMAND_MESSAGE :		    RecvAmfCommandMessage(message);			break;
            case RTMP_MSGID_WINDOWACKNOWLEDGEMENT_SIZE :    RecvWindowAcknowledgementSize(message);	break;
            default:
            {
                logtw("Unknown Type - Type(%d)", message->message_header->type_id);
                break;
            }
        }

        // 실패로 종료 처리 필요
        if(!bSuccess)
        {
            return false;
        }
    }

    return true;
}

//====================================================================================================
// Chunk Message - SetChunkSize
//====================================================================================================
bool RtmpChunkStream::RecvSetChunkSize(std::shared_ptr<ImportMessage> & message)
{
    int32_t chunk_size = RtmpMuxUtil::ReadInt32(message->body->data());

    if(chunk_size <= 0)
    {
        logte("ChunkSize Fail - Size(%d) ***", chunk_size);
        return false;
    }

    _import_chunk->SetChunkSize(chunk_size);
    logtd("Set Recv ChunkSize(%u)", chunk_size);

    return true;
}

//====================================================================================================
// Chunk Message - WindowAcknowledgementSize
//====================================================================================================
void RtmpChunkStream::RecvWindowAcknowledgementSize(std::shared_ptr<ImportMessage> & message)
{
    uint32_t chunk_size 		= RtmpMuxUtil::ReadInt32(message->body->data());
    uint32_t ackledgement_size 	= RtmpMuxUtil::ReadInt32(message->body->data());

    if(ackledgement_size != 0)
    {
        _acknowledgement_size 		= ackledgement_size/2;
        _acknowledgement_traffic 	= 0;
    }
}

//====================================================================================================
// Chunk Message - Amf0CommandMessage
//====================================================================================================
void RtmpChunkStream::RecvAmfCommandMessage(std::shared_ptr<ImportMessage> & message)
{
    AmfDocument	document;
    ov::String  message_name;
    double		transaction_id			= 0.0;


    if(document.Decode(message->body->data(), message->message_header->body_size) ==0)
    {
        logte("AmfDocument Size 0 ");
        return;
    }

    // Message Name  
    if( document.GetProperty(0) == nullptr || document.GetProperty(0)->GetType() != AMF_STRING )
    {
        logte("Message Name Fail");
        return;
    }
    message_name = document.GetProperty(0)->GetString();


    // Message Transaction ID 얻기
    if( document.GetProperty(1)  != nullptr && document.GetProperty(1)->GetType() == AMF_NUMBER )
    {
        transaction_id = document.GetProperty(1)->GetNumber();
    }

    // 처리
    if		 (message_name == RTMP_CMD_NAME_CONNECT)		OnAmfConnect(message->message_header, document, transaction_id);
    else if (message_name == RTMP_CMD_NAME_CREATESTREAM)	OnAmfCreateStream(message->message_header, document, transaction_id);
    else if (message_name == RTMP_CMD_NAME_FCPUBLISH)		OnAmfFCPublish(message->message_header, document, transaction_id);
    else if (message_name == RTMP_CMD_NAME_PUBLISH)		OnAmfPublish(message->message_header, document, transaction_id);
    else if (message_name == RTMP_CMD_NAME_RELEASESTREAM)	{;}
    else if (message_name == RTMP_PING)					{;}
    else if (message_name == RTMP_CMD_NAME_DELETESTREAM)	OnAmfDeleteStream(message->message_header, document, transaction_id);
    else
    {
        logtw("Unknown Amf0CommandMessage - Message(%s:%.1f)", message_name.CStr(), transaction_id);
        return;
    }
}

//====================================================================================================
// Chunk Message - Amf0DataMessage
//====================================================================================================
void RtmpChunkStream::RecvAmfDataMessage(std::shared_ptr<ImportMessage> & message)
{
    AmfDocument	document;
    int32_t		decode_lehgth		= 0;
    ov::String message_name;
    ov::String data_name;

    // 응답 디코딩
    decode_lehgth = document.Decode(message->body->data(), message->message_header->body_size);
    if(decode_lehgth == 0)
    {
        logte("Amf0DataMessage Document Length 0");
        return;
    }

    // Message Name 얻기
    if( document.GetProperty(0)  != nullptr && document.GetProperty(0)->GetType() == AMF_STRING )
    {
        message_name = document.GetProperty(0)->GetString();
    }

    // Data 이름 얻기
    if(document.GetProperty(1)	!= nullptr && document.GetProperty(1)->GetType() == AMF_STRING)
    {
        data_name = document.GetProperty(1)->GetString();
    }

    // 처리
    if( message_name == RTMP_CMD_DATA_SETDATAFRAME && data_name == RTMP_CMD_DATA_ONMETADATA  && 
        document.GetProperty(2) != nullptr && (document.GetProperty(2)->GetType() == AMF_OBJECT || document.GetProperty(2)->GetType() == AMF_ARRAY))
    {
        OnAmfMetaData(message->message_header, document, 2);
    }
    else
    {
        logtw("Unknown Amf0DataMessage - Message(%s)", message_name.CStr());
        return; 
    }
}

//====================================================================================================
// Amf Command - Connect
// - application name 설정
//====================================================================================================
void RtmpChunkStream::OnAmfConnect(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id)
{
    double object_encoding = 0.0;

    if( document.GetProperty(2) != nullptr &&  document.GetProperty(2)->GetType() == AMF_OBJECT )
    {
        AmfObject	* 	object = document.GetProperty(2)->GetObject();
        int32_t			index;

        // object encoding  
        if((index = object->FindName("objectEncoding")) >= 0 && object->GetType(index) == AMF_NUMBER)
        {
            object_encoding = object->GetNumber(index);
        }

        // app 설정
        if ((index = object->FindName("app")) >= 0 && object->GetType(index) == AMF_STRING)
        {
            _app_name = object->GetString(index);
        }
    }

    if(!SendWindowAcknowledgementSize())
    {
        logte("SendWindowAcknowledgementSize Fail");
        return;
    }

    if(!SendSetPeerBandwidth())
    {
        logte("SendSetPeerBandwidth Fail");
        return;
    }

    if(!SendStreamBegin())
    {
        logte("SendStreamBegin Fail");
        return;
    }

    if(!SendAmfConnectResult(message_header->chunk_stream_id, transaction_id, object_encoding))
    {
        logte("SendAmfConnectResult Fail");
        return;
    }
 }

//====================================================================================================
// Amf Command - CreateStream
//====================================================================================================
void RtmpChunkStream::OnAmfCreateStream(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id)
{
    if(!SendAmfCreateStreamResult(message_header->chunk_stream_id, transaction_id))
    {
        logte("SendAmfCreateStreamResult Fail");
        return;
    }

}

//====================================================================================================
// Amf Command - FCPublish
//====================================================================================================
void RtmpChunkStream::OnAmfFCPublish(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id)
{
    if(	_app_stream_name.IsEmpty() && document.GetProperty(3) != nullptr && document.GetProperty(3)->GetType() == AMF_STRING)
    {
        if(!SendAmfOnFCPublish(message_header->chunk_stream_id, _stream_id, _client_id))
        {
            logte("SendAmfOnFCPublish Fail");
            return;
        }
        _app_stream_name = document.GetProperty(3)->GetString();
    }
}

//====================================================================================================
// Amf Command - Publish
//====================================================================================================
void RtmpChunkStream::OnAmfPublish(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id)
{
    if(_app_stream_name.IsEmpty())
    {
        if(document.GetProperty(3) != nullptr &&  document.GetProperty(3)->GetType() == AMF_STRING)
        { 
            _app_stream_name = document.GetProperty(3)->GetString();
        }
        else
        {
            logte("OnPublish - Publish Name None");

            //Reject
            SendAmfOnStatus(message_header->chunk_stream_id, _stream_id, (char *)"error", (char *)"NetStream.Publish.Rejected", (char *)"Authentication Failed.", _client_id);
            return;
        }
    }

    _chunk_stream_id = message_header->chunk_stream_id;

    // stream begin 전송
    if(!SendStreamBegin() )
    {
        logte("SendStreamBegin Fail");
        return;
    }

    // 시작 상태 값 전송
    if(!SendAmfOnStatus((uint32_t)_chunk_stream_id, _stream_id, (char *)"status", (char *)"NetStream.Publish.Start", (char *)"Publishing", _client_id))
    {
        logte("SendAmfOnStatus Fail");
        return;
    }

}

//====================================================================================================
// Amf Command - Publish
//====================================================================================================
void RtmpChunkStream::OnAmfDeleteStream(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, double transaction_id)
{
    logtd("Delete Stream - app(%s/%u) stream(%s/%u)", _app_name.CStr(), _app_id, _app_stream_name.CStr(), _app_stream_id);

    _delete_stream = true;

    // 스트림 삭제 콜백 호출
    _stream_interface->OnChunkStreamDelete(_remote, _app_name, _app_stream_name,_app_id, _app_stream_id);
}

//====================================================================================================
// Message Packet 전송
//====================================================================================================
bool RtmpChunkStream::SendMessagePacket(std::shared_ptr<RtmpMuxMessageHeader> &message_header, std::shared_ptr<std::vector<uint8_t>> &data)
{
    if(message_header == nullptr)
    {
        return false;
    }

    auto  export_data = _export_chunk->ExportStreamData(message_header, data);

    if(export_data == nullptr || export_data->data() == nullptr)
    {
        return false;
    }

    return SendData(export_data->size(), export_data->data());
}

//====================================================================================================
// Amf0 Command 전송
//====================================================================================================
bool RtmpChunkStream::SendAmfCommand(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document)
{
    auto     body      = std::make_shared<std::vector<uint8_t>>(2048);
    uint32_t body_size  = 0;

    if(message_header == nullptr)
    {
        return false;
    }

    // body
    body_size = (uint32_t)document.Encode(body->data());
    if(body_size == 0)
    {
        return false;
    }
    message_header->body_size = body_size;
    body->resize(body_size);

    return SendMessagePacket(message_header, body);
}

//====================================================================================================
// User Control Message 전송
//====================================================================================================
bool RtmpChunkStream::SendUserControlMessage(uint8_t message, std::shared_ptr<std::vector<uint8_t>> &data)
{
    auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT, 0, RTMP_MSGID_USERCONTROL_MESSAGE, 0, data->size() + 1);

    data->insert(data->begin(), 0);
    RtmpMuxUtil::WriteInt8(data->data(), message);

    return SendMessagePacket(message_header, data);
}

//====================================================================================================
// WindowAcknowledgementSize 전송
//====================================================================================================
bool RtmpChunkStream::SendWindowAcknowledgementSize( )
{
    auto body = std::make_shared<std::vector<uint8_t>>(sizeof(int));
    auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT, 0, RTMP_MSGID_WINDOWACKNOWLEDGEMENT_SIZE, _stream_id, body->size());

    RtmpMuxUtil::WriteInt32(body->data(), RTMP_DEFAULT_ACKNOWNLEDGEMENT_SIZE);

    return SendMessagePacket(message_header, body);
}

//====================================================================================================
// AcknowledgementSize 전송 (KeepAlive)
//====================================================================================================
bool RtmpChunkStream::SendAcknowledgementSize( )
{
    auto body = std::make_shared<std::vector<uint8_t>>(sizeof(int));
    auto message_header = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT, 0, RTMP_MSGID_ACKNOWLEDGEMENT, 0, body->size());

    RtmpMuxUtil::WriteInt32(body->data(), _acknowledgement_traffic);

    return SendMessagePacket(message_header, body);
}

//====================================================================================================
// SetPeerBandwidth 전송
//====================================================================================================
bool RtmpChunkStream::SendSetPeerBandwidth( )
{
    auto body            = std::make_shared<std::vector<uint8_t>>(5);
    auto message_header  = std::make_shared<RtmpMuxMessageHeader>(RTMP_CHUNK_STREAM_ID_URGENT, 0, RTMP_MSGID_SET_PEERBANDWIDTH, _stream_id, body->size());

    RtmpMuxUtil::WriteInt32(body->data(), _peer_bandwidth);
    RtmpMuxUtil::WriteInt8(body->data() + 4, 2);

    return SendMessagePacket(message_header, body);
}

//====================================================================================================
// UCMID Stream Begin 전송
//====================================================================================================
bool RtmpChunkStream::SendStreamBegin( )
{
    auto body = std::make_shared<std::vector<uint8_t>>(4);

    RtmpMuxUtil::WriteInt32(body->data(), _stream_id);

    return SendUserControlMessage(RTMP_UCMID_STREAMBEGIN, body);
}

//====================================================================================================
// Connect Result 전송
//====================================================================================================
bool RtmpChunkStream::SendAmfConnectResult(uint32_t chunk_stream_id, double transaction_id, double object_encoding)
{
    auto	    message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id, 0, RTMP_MSGID_AMF0_COMMAND_MESSAGE, _stream_id, 0);
    AmfDocument	document;
    AmfObject *	object = nullptr;
    AmfArray * 	array 	= nullptr;

    // _result 
    document.AddProperty(RTMP_ACK_NAME_RESULT);
    document.AddProperty(transaction_id);

    // properties
    object = new AmfObject;
    object->AddProperty("fmsVer", "FMS/3,5,2,654");
    object->AddProperty("capabilities", 31.0);
    object->AddProperty("mode", 1.0);

    document.AddProperty(object);
    object = nullptr;

    // information
    object = new AmfObject;
    object->AddProperty("level", "status");
    object->AddProperty("code", "NetConnection.Connect.Success");
    object->AddProperty("description", "Connection succeeded.");
    object->AddProperty("clientid", _client_id);
    object->AddProperty("objectEncoding", object_encoding);

    array = new AmfArray;
    array->AddProperty("version",  "3,5,2,654");
    object->AddProperty("data", array);
    array = nullptr;

    document.AddProperty(object);
    object = nullptr;

    return SendAmfCommand(message_header, document);
}

//====================================================================================================
// Create Stream Result 전송
//====================================================================================================
bool RtmpChunkStream::SendAmfOnFCPublish(uint32_t chunk_stream_id, uint32_t stream_id, double client_id )
{
    auto	    message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id, 0, RTMP_MSGID_AMF0_COMMAND_MESSAGE, _stream_id, 0);
    AmfDocument	document;
    AmfObject *	object = nullptr;

    document.AddProperty(RTMP_CMD_NAME_ONFCPUBLISH);
    document.AddProperty(0.0);
    document.AddProperty(AMF_NULL);

    object = new AmfObject;
    object->AddProperty("level", "status");
    object->AddProperty("code", "NetStream.Publish.Start");
    object->AddProperty("description", "FCPublish");
    object->AddProperty("clientid", client_id);

    document.AddProperty(object);

    object = nullptr;

    return SendAmfCommand(message_header, document);
}


//====================================================================================================
// Create Stream Result 전송
//====================================================================================================
bool RtmpChunkStream::SendAmfCreateStreamResult(uint32_t chunk_stream_id, double transaction_id)
{
    auto	    message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id, 0, RTMP_MSGID_AMF0_COMMAND_MESSAGE, 0, 0);
    AmfDocument	document;

    // 스트림ID 정하기
    _stream_id = 1;

    document.AddProperty(RTMP_ACK_NAME_RESULT);
    document.AddProperty(transaction_id);
    document.AddProperty(AMF_NULL);
    document.AddProperty((double)_stream_id);

    return SendAmfCommand(message_header, document);
}

//====================================================================================================
// 상태 전송
//====================================================================================================
bool RtmpChunkStream::SendAmfOnStatus(uint32_t chunk_stream_id, uint32_t stream_id, char * level, char * code, char * description, double client_id )
{
    auto	    message_header = std::make_shared<RtmpMuxMessageHeader>(chunk_stream_id, 0, RTMP_MSGID_AMF0_COMMAND_MESSAGE, stream_id, 0);
    AmfDocument	document;
    AmfObject *	object = nullptr;

    document.AddProperty(RTMP_CMD_NAME_ONSTATUS);
    document.AddProperty(0.0);
    document.AddProperty(AMF_NULL);

    object = new AmfObject;
    object->AddProperty("level", 		level);
    object->AddProperty("code", 		code);
    object->AddProperty("description", description);
    object->AddProperty("clientid", 	client_id);

    document.AddProperty(object);

    object = nullptr;

    return SendAmfCommand(message_header, document);
}

//====================================================================================================
// Chunk Message - Audio Message
// * 패킷 구조
// 1. AAC
// - Control Header : 	1Byte(Codec/SampleRate/SampleSize/Channels)
// - Sequence Type : 	1Byte(0x00 - Config Packet/ 0x00 -Frame Packet)
// - Frame
//
// 2.Speex/MP3
// - Control Header(1Byte - Codec/SampleRate/SampleSize/Channels)
// - Frame
//
//AAC : Adts헤더 정보 추가
//Speex : 크기 정보 추가
//====================================================================================================
bool RtmpChunkStream::RecvAudioMessage(std::shared_ptr<ImportMessage> & message)
{
    if(!_media_info->has_aduio || _delete_stream)
    {
        return true;
    }

    // size check
    if(message->message_header->body_size < RTMP_AAC_AUDIO_DATA_MIN_SIZE || message->message_header->body_size > RTMP_MAX_PACKET_SIZE)
    {
        logte("Size Fail - size(%d)", message->message_header->body_size);
        return false;
    }

    // Seqence Data 처리
    if(message->body->at(RTMP_AAC_AUDIO_SEQUENCE_TYPE_INDEX) == RTMP_SEQUENCE_DATA_TYPE)
    {
        std::unique_ptr<std::vector<uint8_t>> data = std::make_unique<std::vector<uint8_t>>(message->body->begin() + 2, message->body->end());

        if(!ProcessAudioSequenceData(std::move(data)))
        {
            logte("ProcessAudioSequenceData Fail");
            return false;
        }
    }

    // 오디오  데이터 전송 콜백 호출
    _stream_interface->OnChunkStreamAudioData(_remote, _app_id, _app_stream_id, message->message_header->timestamp, message->body);

    return true;
}

//====================================================================================================
// Chunk Message - Video Message
// * 패킷 구조
// 1.Control Header(1Byte - Frame Type + Codec Type)
// 2.Type : 1Byte( 0x00 - Config Packet/ 0x00 -Frame Packet)
// 3.Composition Time Offset : 3Byte
// 4.Frame Size(4Byte)
//
// * Frame 순서
// 1. SPS/PPS 정보
// 2. 정보 + I-Frame
// 3. I-Frame/P-Frame
//====================================================================================================
bool RtmpChunkStream::RecvVideoMessage(std::shared_ptr<ImportMessage> &message)
{
   tRTMP_FRAME_TYPE frame_type = RTMP_VIDEO_P_FRAME_TYPE;

    if(!_media_info->has_video || _delete_stream)
    {
        return true;
    }

    // size check
    if(message->message_header->body_size < RTMP_VIDEO_DATA_MIN_SIZE || message->message_header->body_size > RTMP_MAX_PACKET_SIZE)
    {
        logte("Size Fail - size(%d)", message->message_header->body_size);
        return false;
    }

    // Frame Type 확인 (I/P(B) Frame)
    if		(message->body->at(RTMP_VIDEO_CONTROL_HEADER_INDEX) == RTMP_H264_I_FRAME_TYPE)frame_type = RTMP_VIDEO_I_FRAME_TYPE; //I-Frame
    else if(message->body->at(RTMP_VIDEO_CONTROL_HEADER_INDEX) == RTMP_H264_P_FRAME_TYPE)frame_type = RTMP_VIDEO_P_FRAME_TYPE; //P-Frame
    else
    {
        logte("Frame Type Fail - type(0x%x)", (uint8_t)message->body->at(RTMP_VIDEO_CONTROL_HEADER_INDEX));
        return false;
    }

    // Seqence Data 처리
   if(message->body->at(RTMP_VIDEO_SEQUENCE_TYPE_INDEX) == RTMP_SEQUENCE_DATA_TYPE)
   {
       // control header/sequence type 정보 skip
       std::unique_ptr<std::vector<uint8_t>> data = std::make_unique<std::vector<uint8_t>>(message->body->begin() + 2, message->body->end());

       if(!ProcessVideoSequenceData(std::move(data)))
       {
           logte("ProcessAudioSequenceData Fail");
           return false;
       }
   }

    // 비디오 데이터 전송 콜백 호출
    _stream_interface->OnChunkStreamVideoData(_remote, _app_id, _app_stream_id, message->message_header->timestamp, message->body);

    return true;
}

//====================================================================================================
// ProcessVideoSequenceData
// - Video Control 패킷 처리
// - SPS/PPS 정보 추출 : Bypass 시에 트랜스코딩에서 정보를 못주면 여기서 처리 필요
// - ffmpeg 재싱 시에 해상도 정보 문제 발생시 SPS에서 해상도 정보 추출 필요
// - SPS/PPS Load
//      - 00 00 00   		- 3 byte
//      - version 			- 1 byte
//      - profile			- 1 byte
//      - Compatibility		- 1 byte
//      - level				- 1 byte
//      - length size in byte 	- 1 byte
//      - number of sps		- 1 byte(0xe1/0x01)
//      - sps size			- 2 byte  *
//      - sps data			- n byte  *
//      - number of pps 	- 1 byte
//      - pps size			- 2 byte  *
//      - pps data			- n byte  *
//====================================================================================================
bool RtmpChunkStream::ProcessVideoSequenceData(std::unique_ptr<std::vector<uint8_t>> data)
{
    int sps_size = 0;
    int pps_size = 0;

    // 최소  길이/ 시작 /SPS 개수 1(0xe0 + 개수) 확인
    if(data->size() < RTMP_SPS_PPS_MIN_DATA_SIZE || data->at(0) != 0 || data->at(1) != 0)
    {
        logte("Data Size Fail - size(%d)", data->size());
        return false;
    }

    // SPS 길이
    sps_size = RtmpMuxUtil::ReadInt16(data->data() + 9);
    if(sps_size <= 0 || sps_size > (data->size() - 11))
    {
        logte("SPS Size Fail - sps(%d)", sps_size);
        return false;
    }

    // PPS 길이
    pps_size = RtmpMuxUtil::ReadInt16(data->data() + 12 + sps_size);
    if(pps_size <= 0 || pps_size > (data->size() - 14 - sps_size))
    {
        logte("PPS Size Fail - pps(%d:%d)", pps_size);
        return false;
    }

    _media_info->avc_sps->assign(data->begin() + 11, data->begin() + 11 + sps_size); // SPS
    _media_info->avc_pps->assign(data->begin() + 14 + sps_size, data->begin() + 14 + sps_size + pps_size);  // PPS

    logtd("Video Sequence Data - sps(%d) pps(%d)", _media_info->avc_sps->size(), _media_info->avc_pps->size());

    return true;
}
//====================================================================================================
// ProcessAudioSequenceData
// - Audio Control 패킷 처리
// - 오디오 samplerate/channels 등의 정보 추출 가능
// - audio frame header나 메타데이터 정보 보다 정확
//======================================================= =============================================
bool RtmpChunkStream::ProcessAudioSequenceData(std::unique_ptr<std::vector<uint8_t>> data)
{
    int sample_index = 0;
    int samplerate  = 0;
    int channels    = 0;

    // 최소  길이 2byte
    if(data->size() < 2 )
    {
        logte("Data Size Fail - size(%d)", data->size());
        return false;
    }

    sample_index += (data->at(0) & 0x07) << 1;
    sample_index += data->at(1) >> 7;
    if(sample_index >= RTMP_SAMPLERATE_TABLE_SIZE)
    {
        logte("Sampleindex Fail - index(%d)", sample_index);
        return false;
    }

    samplerate 	= g_rtmp_sample_rate_table[sample_index];
    channels  =  data->at(1)>>3 & 0x0f;

    _media_info->audio_samplerate 	= samplerate;
    _media_info->audio_sampleindex 	= sample_index;
    _media_info->audio_channels      = channels;

    logtd("Audio Sequence Data - samplerate(%d) channels(%d)", samplerate, channels);

    return true;
}

//====================================================================================================
// Amf Command - OnMetaData
//====================================================================================================
bool RtmpChunkStream::OnAmfMetaData(std::shared_ptr<RtmpMuxMessageHeader> &message_header, AmfDocument &document, int32_t object_index)
{
    tRTMP_CODEC_TYPE    video_codec_type 	= RTMP_UNKNOWN_CODEC_TYPE;
    tRTMP_CODEC_TYPE    audio_codec_type	= RTMP_UNKNOWN_CODEC_TYPE;
    double 			frame_rate 			= 30.0;
    double 			video_width			= 0;
    double 			video_height		= 0;
    double			    video_bitrate		= 0;
    double  			audio_bitrate		= 0.0;
    double 			audio_channels		= 1.0;
    double 			audio_samplerate    = 0.0;
    double             audio_samplesize    = 0.0;
    AmfObjectArray * 	object				= nullptr;
    int32_t 			index 				= 0;
    ov::String 		    bitrate_string;
    ov::String			device_type_string	= RTMP_UNKNOWN_DEVICE_TYPE_STRING;
    tRTMP_ENCODER_TYPE  encoder_type        = RTMP_CUSTOM_ENCODER_TYPE;

    // dump 정보 출력
    std::string dump_string;
    document.Dump(dump_string);
    logtd(dump_string.c_str());

    // object encoding 얻기
    if(document.GetProperty(object_index)->GetType() == AMF_OBJECT)	object = (AmfObjectArray *)(document.GetProperty(object_index)->GetObject());
    else 															object = (AmfObjectArray *)(document.GetProperty(object_index)->GetArray());

    // DeviceType
    if		    ((index = object->FindName("videodevice")) >= 0 && object->GetType(index) == AMF_STRING)	device_type_string =  object->GetString(index); 	//DeviceType - XSplit
    else if	((index = object->FindName("encoder")) >= 0 && object->GetType(index) == AMF_STRING)		device_type_string =  object->GetString(index);	//DeviceType - OBS


    // Encoder 인식
    if       (device_type_string.IndexOf("Open Broadcaster") >=  0)	    encoder_type = RTMP_OBS_ENCODER_TYPE;
    else if (device_type_string.IndexOf("obs-output") >=  0)			encoder_type = RTMP_OBS_ENCODER_TYPE;
    else if (device_type_string.IndexOf("XSplitBroadcaster") >=  0)	encoder_type = RTMP_XSPLIT_ENCODER_TYPE;
    else if (device_type_string.IndexOf("Lavf") >=  0)					encoder_type = RTMP_LAVF_ENCODER_TYPE;
    else																encoder_type = RTMP_CUSTOM_ENCODER_TYPE;

    // Vodio Codec
    if((index = object->FindName("videocodecid")) >= 0)
    {
        if      (object->GetType(index) == AMF_STRING && strcmp("avc1", object->GetString(index)) == 0)     video_codec_type = RTMP_H264_VIDEO_CODEC_TYPE;
        else if (object->GetType(index) == AMF_STRING && strcmp("H264Avc", object->GetString(index)) == 0)  video_codec_type = RTMP_H264_VIDEO_CODEC_TYPE;
        else if (object->GetType(index) == AMF_NUMBER && object->GetNumber(index) == 7.0)					video_codec_type = RTMP_H264_VIDEO_CODEC_TYPE;
    }

    // Video Framerate
    if      ((index = object->FindName("framerate")) >= 0 && object->GetType(index) == AMF_NUMBER)		frame_rate 	=  object->GetNumber(index);
    else if((index = object->FindName("videoframerate")) >= 0 && object->GetType(index) == AMF_NUMBER)	frame_rate 	=  object->GetNumber(index);

    // Video Resolution
    if((index = object->FindName("width")) >= 0 && object->GetType(index) == AMF_NUMBER)	            video_width 	=  object->GetNumber(index); 	//Width
    if((index = object->FindName("height")) >= 0 && object->GetType(index) == AMF_NUMBER)	            video_height 	=  object->GetNumber(index); 	//Height

    // Video Bitrate
    if((index = object->FindName("videodatarate")) >= 0 && object->GetType(index) == AMF_NUMBER)	    video_bitrate	= object->GetNumber(index); 	// Video Data Rate
    if((index = object->FindName("bitrate")) >= 0 && object->GetType(index) == AMF_NUMBER)			    video_bitrate	= object->GetNumber(index); 	// Video Data Rate
    if(((index = object->FindName("maxBitrate")) >= 0) && object->GetType(index) == AMF_STRING)
    {
        bitrate_string = object->GetString(index);
        video_bitrate = strtol(bitrate_string.CStr(), nullptr, 0);
    }

    // Audio Codec
    if((index = object->FindName("audiocodecid")) >= 0)
    {
        if		(object->GetType(index) == AMF_STRING && strcmp("mp4a", object->GetString(index)) == 0)	    audio_codec_type = RTMP_AAC_AUDIO_CODEC_TYPE;	//AAC
        else if(object->GetType(index) == AMF_STRING && strcmp("mp3",  object->GetString(index)) == 0)	    audio_codec_type = RTMP_MP3_AUDIO_CODEC_TYPE;	//MP3
        else if(object->GetType(index) == AMF_STRING && strcmp(".mp3", object->GetString(index)) == 0) 	audio_codec_type = RTMP_MP3_AUDIO_CODEC_TYPE;	//MP3
        else if(object->GetType(index) == AMF_STRING && strcmp("speex", object->GetString(index)) == 0)	audio_codec_type = RTMP_SPEEX_AUDIO_CODEC_TYPE;//Speex
        else if(object->GetType(index) == AMF_NUMBER && object->GetNumber(index) == 10.0) 				    audio_codec_type = RTMP_AAC_AUDIO_CODEC_TYPE;//AAC
        else if(object->GetType(index) == AMF_NUMBER && object->GetNumber(index) == 11.0) 				    audio_codec_type = RTMP_SPEEX_AUDIO_CODEC_TYPE;//Speex
        else if(object->GetType(index) == AMF_NUMBER && object->GetNumber(index) == 2.0) 				    audio_codec_type = RTMP_MP3_AUDIO_CODEC_TYPE; 	//MP3
    }

    // Audio bitreate
    if      ((index = object->FindName("audiodatarate")) >= 0 && object->GetType(index) == AMF_NUMBER)	    audio_bitrate =  object->GetNumber(index);	// Audio Data Rate
    else if((index = object->FindName("audiobitrate")) >= 0  && object->GetType(index) == AMF_NUMBER)	    audio_bitrate =  object->GetNumber(index); 	// Audio Data Rate

    // Audio Channels
    if((index = object->FindName("audiochannels")) >= 0 )
    {
        if		(object->GetType(index) == AMF_NUMBER)														audio_channels =  object->GetNumber(index);
        else if(object->GetType(index) == AMF_STRING && strcmp("stereo", object->GetString(index)) == 0)	audio_channels	 	= 2;
        else if(object->GetType(index) == AMF_STRING && strcmp("mono", object->GetString(index)) == 0)		audio_channels 		= 1;

    }

    // Audio samplerate
    if((index = object->FindName("audiosamplerate")) >= 0 )   audio_samplerate	=  object->GetNumber(index); 	// Audio Sample Rate

    // Audio samplesize
    if((index = object->FindName("audiosamplesize")) >= 0 )	  audio_samplesize	=  object->GetNumber(index); 	// Audio Sample Size

    // support codec check (H264/AAC 지원)
    if(!(video_codec_type == RTMP_H264_VIDEO_CODEC_TYPE) && !(audio_codec_type == RTMP_AAC_AUDIO_CODEC_TYPE))
    {
        logte("codec type fail - video(%s) audio(%s)", g_rtmp_codec_type_string[video_codec_type], g_rtmp_codec_type_string[audio_codec_type]);
        return false;
    }

    _media_info->has_video          = (video_codec_type == RTMP_H264_VIDEO_CODEC_TYPE);
    _media_info->has_aduio          = (audio_codec_type == RTMP_AAC_AUDIO_CODEC_TYPE);
    _media_info->video_codec_type	= video_codec_type;
    _media_info->video_width		= (int32_t)video_width;
    _media_info->video_height		= (int32_t)video_height;
    _media_info->video_framerate	= (float)frame_rate;
    _media_info->video_bitrate		= (int32_t)video_bitrate;
    _media_info->audio_codec_type	= audio_codec_type;
    _media_info->audio_bitrate		= (int32_t)audio_bitrate;
    _media_info->audio_channels		= (int32_t)audio_channels;
    _media_info->audio_bits			= (int32_t)audio_samplesize;
    _media_info->audio_samplerate	= (int32_t)audio_samplerate;
    _media_info->encoder_type       = encoder_type;

    logtd("\n======= MEDIA INFO ======= \n"\
            "encoder : %s(%s) \n"\
            "video : %s/%d*%d/%.2ffps/%dkbps \n"\
            "audio : %s/%dch/%dhz/%dkbps \n",
            g_rtmp_encoder_type_string[encoder_type],
            device_type_string.CStr(),
            g_rtmp_codec_type_string[_media_info->video_codec_type],
            _media_info->video_width,
            _media_info->video_height,
            _media_info->video_framerate,
            _media_info->video_bitrate,
            g_rtmp_codec_type_string[_media_info->audio_codec_type],
            _media_info->audio_channels,
            _media_info->audio_samplerate,
            _media_info->audio_bitrate);

    if(!(_stream_interface->OnChunkStreamReadyComplete(_remote, _app_name, _app_stream_name, _media_info, _app_id, _app_stream_id)))
    {
        logte("OnChunkStreamReadyComplete fail");
        return false;
    }

    return true;
}

/*Meta Data Info
==============================  OBS  ==============================
duration : 0.0
fileSize : 0.0
width : 1280.0
height : 720.0
videocodecid : avc1
videodatarate : 2500.0
framerate : 30.0
audiocodecid : mp4a
audiodatarate : 160.0
audiosamplerate : 44100.0
audiosamplesize : 16.0
audiochannels : 2.0
stereo : 1
2.1 : 0
3.1 : 0
4.0 : 0
4.1 : 0
5.1 : 0
7.1 : 0
encoder : obs-output module (libobs version 22.0.2)

==============================  XSplit  ==============================
author : SIZE-ZERO STRING
copyright : SIZE-ZERO STRING
description : SIZE-ZERO STRING
keywords : SIZE-ZERO STRING
rating : SIZE-ZERO STRING
title : SIZE-ZERO STRING
presetname : Custom
creationdate : Tue Sep 11 10:57:23 2018
videodevice : VHVideoCustom
framerate : 30.0
width : 1280.0
height : 720.0
videocodecid : avc1
avclevel : 31.0
avcprofile : 100.0
videodatarate : 2929.7
audiodevice : VHAudioCustom
audiosamplerate : 48000.0
audiochannels : 1.0
audioinputvolume : 100.0
audiocodecid : mp4a
audiodatarate : 93.8
aid : 2gZQW8H4TOTc4QZ7w2yXfw==
bufferSize : 3000k
maxBitrate : 3000k
pluginName : CustomRTMP
pluginVersion : 3.4.1805.1801
=> Object  End  <=

 ==============================  ffmpeg  ==============================
duration : 0.0
width : 1280.0
height : 720.0
videodatarate : 0.0
framerate : 24.0
videocodecid : 7.0
audiodatarate : 125.0
audiosamplerate : 44100.0
audiosamplesize : 16.0
stereo : 1
audiocodecid : 10.0
major_brand : mp42
minor_version : 0
compatible_brands : isommp42
encoder : Lavf58.12.100
filesize : 0.0

*/