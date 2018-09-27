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
        logtw("Server is running");
        return false;
    }

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
            logtw("%p is already observer of RtcSignallingServer", observer.get());
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
void RtmpServer::OnConnected(ov::Socket *remote)
{
    logtd("Client Connected - remote(%s)", remote->ToString().CStr());

    _client_list[remote] = std::make_shared<RtmpChunkStream>(dynamic_cast<ov::ClientSocket *>(remote), this);
}

//====================================================================================================
// OnDataReceived
// - 데이터 수신
//====================================================================================================
void RtmpServer::OnDataReceived(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	auto item = _client_list.find(remote);

	// clinet 세션 확인
	if(item == _client_list.end())
	{
		// OnConnected()에서 추가했으므로, 반드시 client list에 있어야 함
		OV_ASSERT2(item != _client_list.end());
		return;
	}

    // client 접속 상태 확인
	if(remote->GetState() != ov::SocketState::Connected)
	 {
         logte("Client erase - remote(%s)", remote->ToString().CStr());
		_client_list.erase(item);
		return;
	 }

	 // 데이터 전달
	 auto process_data = std::make_unique<std::vector<uint8_t>>((uint8_t *)data->GetData(), (uint8_t *)data->GetData() + data->GetLength());
	 if(item->second->OnDataReceived(std::move(process_data)) < 0)
     {
	     // TODO: 스트림 종료 및 세션 정리
         logte("Receiven Process Fail - remote(%s", remote->ToString().CStr());
	     return;
     }

}

//====================================================================================================
// OnDisconnected
// - client 세션 제거
//====================================================================================================
void RtmpServer::OnDisconnected(ov::Socket *remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	logtd("Client Disconnected - remote(%s)", remote->ToString().CStr());

    auto item = _client_list.find(remote);

    if(item != _client_list.end())
    {
        logtd("Client erase - remote(%s)", remote->ToString().CStr());
        _client_list.erase(item);
    }
}

//====================================================================================================
// OnChunkStreamReadyComplete
// - IRtmpChunkStream 구현
// - 스트림 준비 완료
// - app/stream id 획득 및 저장
//====================================================================================================
bool RtmpServer::OnChunkStreamReadyComplete(ov::ClientSocket *remote, ov::String &app_name, ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, uint32_t &app_id, uint32_t &stream_id)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnStreamReadyComplete(app_name, stream_name, media_info, app_id, stream_id))
        {
            logte("Ready Complete Fail - app(%s) stream(%s) client(%s)", app_name.CStr(), stream_name.CStr(), remote->ToString().CStr());
            return false;
        }
    }

    logtd("Create Stream - app(%s/%u) stream(%s/%u) client(%s)", app_name.CStr(), app_id, stream_name.CStr(), stream_id, remote->ToString().CStr());

    return true;
}

//====================================================================================================
// OnChunkStreamVideoData
// - IRtmpChunkStream 구현
// - Video 스트림 전달
//====================================================================================================
bool RtmpServer::OnChunkStreamVideoData(ov::ClientSocket *remote, uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnVideoData(app_id, stream_id, timestamp, data))
        {
            logte("Video Data Fail - app(%u) stream(%u) client(%s)", app_id, stream_id, remote->ToString().CStr());
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
bool RtmpServer::OnChunkStreamAudioData(ov::ClientSocket *remote, uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnAudioData(app_id, stream_id, timestamp, data))
        {
            logte("Audio Data Fail - app(%u) stream(%u) client(%s)", app_id, stream_id, remote->ToString().CStr());
            return false;
        }
    }

    return true;
}

//====================================================================================================
// OnDeleteStream
// - IRtmpChunkStream 구현
//====================================================================================================
bool RtmpServer::OnChunkStreamDelete(ov::ClientSocket *remote, ov::String &app_name, ov::String &stream_name, uint32_t app_id, uint32_t stream_id)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnDeleteStream(app_id, stream_id))
        {
            logte("Delete Stream Fail - app(%s/%u) stream(%s/%u) client(%s)", app_name.CStr(), app_id, stream_name.CStr(), stream_id, remote->ToString().CStr());
            return false;
        }
    }

    logtd("Delete Stream - app(%s/%u) stream(%s/%u) client(%s)", app_name.CStr(), app_id, stream_name.CStr(), stream_id, remote->ToString().CStr());

    return true;
}
