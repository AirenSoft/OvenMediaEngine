//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong	Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtmp_server.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "RtmpProvider"

//====================================================================================================
// ~RtmpServer
//====================================================================================================
RtmpServer::~RtmpServer()
{
	OV_ASSERT2(_physical_port == nullptr);
}

//====================================================================================================
// Start
//====================================================================================================
bool RtmpServer::Start(const ov::SocketAddress &address)
{
    logtd("RtmpServer Start");

    if(_physical_port != nullptr)
    {
        logtw("RtmpServer running fail");
        return false;
    }

    // Gargabe Check Timer Setting
    _garbage_check_timer.Push([this](void *paramter) ->bool
                              {
                                OnGarbageCheck();
                                return true;
                              }, nullptr, 3000, true);

    // Gargabe Check Timer Start
    _garbage_check_timer.Start();

	_physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Tcp, address);

	if(_physical_port != nullptr)
	{
		_physical_port->AddObserver(this);
	}

	return _physical_port != nullptr;
}

//====================================================================================================
// Stop
//====================================================================================================
bool RtmpServer::Stop()
{
	if(_physical_port == nullptr)
	{
		return false;
	}

    // Gargabe Check Timer Stop
    _garbage_check_timer.Stop();

	_physical_port->RemoveObserver(this);
	PhysicalPortManager::Instance()->DeletePort(_physical_port);
	_physical_port = nullptr;

	return true;
}

//====================================================================================================
// AddObserver
// - RtmpOvserver 등록
//====================================================================================================
bool RtmpServer::AddObserver(const std::shared_ptr<RtmpObserver> &observer)
{
    logtd("RtmpServer AddObserver");
    // 기존에 등록된 observer가 있는지 확인
    for(const auto &item : _observers)
    {
        if(item == observer)
        {
            // 기존에 등록되어 있음
            logtw("%p is already observer of RtmpServer", observer.get());
            return false;
        }
    }

    _observers.push_back(observer);

    return true;
}

//====================================================================================================
// RemoveObserver
// - RtmpObserver 제거
//====================================================================================================
bool RtmpServer::RemoveObserver(const std::shared_ptr<RtmpObserver> &observer)
{
    auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<RtmpObserver> const &value) -> bool
    {
        return value == observer;
    });

    if(item == _observers.end())
    {
        // 기존에 등록되어 있지 않음
        logtw("%p is not registered observer", observer.get());
        return false;
    }

    _observers.erase(item);

    return true;
}

//====================================================================================================
// OnConnected
// - client 세션 추가
//====================================================================================================
void RtmpServer::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
    logti("Rtmp input stream connected - remote(%s)", remote->ToString().CStr());

    std::unique_lock<std::recursive_mutex> lock(_chunk_stream_list_mutex);

    _chunk_stream_list[remote.get()] = std::make_shared<RtmpChunkStream>(dynamic_cast<ov::ClientSocket *>(remote.get()), this);
}

//====================================================================================================
// Disconnect
// - stream name은 중복 가능 해서 stream id 로 검색
//====================================================================================================
bool RtmpServer::Disconnect(const ov::String &app_name, uint32_t stream_id)
{
    std::unique_lock<std::recursive_mutex> lock(_chunk_stream_list_mutex);

    for(auto item = _chunk_stream_list.begin(); item != _chunk_stream_list.end(); ++item)
    {
        auto chunk_stream = item->second;

        if(chunk_stream->GetAppName() == app_name && chunk_stream->GetStreamId()== stream_id)
        {
	        _physical_port->DisconnectClient(dynamic_cast<ov::ClientSocket *>(item->first));
            _chunk_stream_list.erase(item);
            return true;
        }
    }

    return false;
}

//====================================================================================================
// OnDataReceived
// - 데이터 수신
//====================================================================================================
void RtmpServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
                                const ov::SocketAddress &address,
                                const std::shared_ptr<const ov::Data> &data)
{
    std::unique_lock<std::recursive_mutex> lock(_chunk_stream_list_mutex);

	auto item = _chunk_stream_list.find(remote.get());

	// clinet 세션 확인
	if(item != _chunk_stream_list.end())
	{
        // client 접속 상태 확인
        if(remote->GetState() != ov::SocketState::Connected)
        {
            logte("Rtmp input stream erase - remote(%s)", remote->ToString().CStr());
            _chunk_stream_list.erase(item);
            return;
        }

        // 데이터 전달
        auto process_data = std::make_unique<std::vector<uint8_t>>((uint8_t *)data->GetData(),
                                                                   (uint8_t *)data->GetData() + data->GetLength());

        if(item->second->OnDataReceived(std::move(process_data)) < 0)
        {
            // Stream Close
            if(item->second->GetAppId() != 0 && item->second->GetStreamId() != 0)
            {
                OnDeleteStream(item->second->GetRemoteSocket(),
                               item->second->GetAppName(),
                               item->second->GetStreamName(),
                               item->second->GetAppId(),
                               item->second->GetStreamId());
            }

            // Socket Close
            _physical_port->DisconnectClient(dynamic_cast<ov::ClientSocket *>(item->first));

            logti("Rtmp input stream disconnect - stream(%s/%s) id(%u/%u) remote(%s)",
                  item->second->GetAppName().CStr(),
                  item->second->GetStreamName().CStr(),
                  item->second->GetAppId(),
                  item->second->GetStreamId(),
                  remote->ToString().CStr());

            _chunk_stream_list.erase(item);

            return;
        }
    }
}

//====================================================================================================
// OnDisconnected
// - client(Encoder) 문제로 접속 종료 이벤트 발생
// - socket 세션은 호출한 ServerSocket에서 스스로 정리
//====================================================================================================
void RtmpServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
                                PhysicalPortDisconnectReason reason,
                                const std::shared_ptr<const ov::Error> &error)
{
	std::unique_lock<std::recursive_mutex> lock(_chunk_stream_list_mutex);

    auto item = _chunk_stream_list.find(remote.get());

    if(item != _chunk_stream_list.end())
    {
        // Stream Delete
        if(item->second->GetAppId() != 0 && item->second->GetStreamId() != 0)
        {
            OnDeleteStream(item->second->GetRemoteSocket(),
                           item->second->GetAppName(),
                           item->second->GetStreamName(),
                           item->second->GetAppId(),
                           item->second->GetStreamId());
        }

        logti("Rtmp input stream disconnected - stream(%s/%s) id(%u/%u) remote(%s)",
              item->second->GetAppName().CStr(),
              item->second->GetStreamName().CStr(),
              item->second->GetAppId(),
              item->second->GetStreamId(),
              remote->ToString().CStr());

        _chunk_stream_list.erase(item);
    }
}

//====================================================================================================
// OnChunkStreamReadyComplete
// - IRtmpChunkStream 구현
// - 스트림 준비 완료
// - app/stream id 획득 및 저장
//====================================================================================================
bool RtmpServer::OnChunkStreamReadyComplete(ov::ClientSocket *remote,
                                            ov::String &app_name,
                                            ov::String &stream_name,
                                            std::shared_ptr<RtmpMediaInfo> &media_info,
                                            info::application_id_t &application_id,
                                            uint32_t &stream_id)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnStreamReadyComplete(app_name, stream_name, media_info, application_id, stream_id))
        {
            logte("Rtmp input stream ready fail - app(%s) stream(%s) remote(%s)",
                    app_name.CStr(),
                    stream_name.CStr(),
                    remote->ToString().CStr());
            return false;
        }
    }

    logti("Rtmp input stream create completed - strem(%s/%s) id(%u/%u) device(%s) remote(%s)",
            app_name.CStr(),
            stream_name.CStr(),
            application_id,
            stream_id,
            RtmpChunkStream::GetEncoderTypeString(media_info->encoder_type).CStr(),
            remote->ToString().CStr());

    return true;
}

//====================================================================================================
// OnChunkStreamVideoData
// - IRtmpChunkStream 구현
// - Video 스트림 전달
//====================================================================================================
bool RtmpServer::OnChunkStreamVideoData(ov::ClientSocket *remote,
                                        info::application_id_t application_id,
                                        uint32_t stream_id,
                                        uint32_t timestamp,
                                        RtmpFrameType frame_type,
                                        std::shared_ptr<std::vector<uint8_t>> &data)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnVideoData(application_id, stream_id, timestamp, frame_type, data))
        {
            logte("Rtmp input stream video data fail - id(%u/%u) remote(%s)",
                  application_id,
                  stream_id,
                  remote->ToString().CStr());
            return false;
        }
    }

    return true;
}

//====================================================================================================
// OnChunkStreamAudioData
// - IRtmpChunkStream 구현
// - Audio 스트림 전달
//====================================================================================================
bool RtmpServer::OnChunkStreamAudioData(ov::ClientSocket *remote,
                                        info::application_id_t application_id,
                                        uint32_t stream_id,
                                        uint32_t timestamp,
                                        RtmpFrameType frame_type,
                                        std::shared_ptr<std::vector<uint8_t>> &data)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnAudioData(application_id, stream_id, timestamp, frame_type, data))
        {
            logte("Rtmp input stream audio data fail - id(%u/%u) remote(%s)",
                    application_id,
                    stream_id,
                    remote->ToString().CStr());
            return false;
        }
    }

    return true;
}

//====================================================================================================
// OnDeleteStream
// - IRtmpChunkStream 구현
// - 스트림만 삭제
// - 세션 연결은 호출한 곳에서 처리되거나 Garbge Check에서 처리
//====================================================================================================
bool RtmpServer::OnDeleteStream(ov::ClientSocket *remote,
                                ov::String &app_name,
                                ov::String &stream_name,
                                info::application_id_t application_id,
                                uint32_t stream_id)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnDeleteStream(application_id, stream_id))
        {
            logte("Rtmp input stream delete fail - stream(%s/%s) id(%u/%u) remote(%s)",
                    app_name.CStr(),
                    stream_name.CStr(),
                    application_id,
                    stream_id,
                    remote->ToString().CStr());
            return false;
        }
    }

    logtd("Rtmp input stream delete - stream(%s/%s) id(%u/%u) remote(%s)",
            app_name.CStr(),
            stream_name.CStr(),
            application_id,
            stream_id,
            remote->ToString().CStr());

    return true;
}

//====================================================================================================
// Gerbage Check
// - Last Packet Time Check
//====================================================================================================
#define MAX_STREAM_PACKET_GAP (10)
void RtmpServer::OnGarbageCheck()
{
    time_t current_time = time(nullptr);

    std::unique_lock<std::recursive_mutex> lock(_chunk_stream_list_mutex);

    for(auto item = _chunk_stream_list.begin(); item != _chunk_stream_list.end();)
    {
        auto chunk_stream = item->second;

        // 10초 Stream Packet 체크
        if(current_time - chunk_stream->GetLastPacketTime() > MAX_STREAM_PACKET_GAP)
        {
            // Stream Close
            if(item->second->GetAppId() != 0 && item->second->GetStreamId() != 0)
            {
                OnDeleteStream(item->second->GetRemoteSocket(),
                                item->second->GetAppName(),
                                item->second->GetStreamName(),
                                item->second->GetAppId(),
                                item->second->GetStreamId());
            }

            // Socket Close
	        _physical_port->DisconnectClient(dynamic_cast<ov::ClientSocket *>(item->first));

            logtw("Rtmp input stream  timeout remove - stream(%s/%s) id(%u/%u) time(%d/%d)",
                  item->second->GetAppName().CStr(),
                  item->second->GetStreamName().CStr(),
                  item->second->GetAppId(),
                  item->second->GetStreamId(),
                  current_time - chunk_stream->GetLastPacketTime(),
                  MAX_STREAM_PACKET_GAP);

            _chunk_stream_list.erase(item++);
        }
        else
        {
            item++;
        }
    }
}

