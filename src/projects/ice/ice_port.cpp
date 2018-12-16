//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "ice_port.h"
#include "ice_private.h"

#include "stun/stun_message.h"
#include "stun/attributes/stun_attributes.h"

#include <algorithm>

#include <base/ovlibrary/ovlibrary.h>

IcePort::IcePort()
{
	_timer.Push([this](void *paramter) -> bool
	            {
		            CheckTimedoutItem();
		            return true;
	            }, nullptr, 1000, true);
	_timer.Start();
}

IcePort::~IcePort()
{
	_timer.Stop();

	if(_physical_port != nullptr)
	{
		_physical_port->Close();
	}
}

bool IcePort::Create(ov::SocketType type, const ov::SocketAddress &address)
{
	if(_physical_port != nullptr)
	{
		logtw("IcePort is running");
		return false;
	}

	_physical_port = PhysicalPortManager::Instance()->CreatePort(type, address);

	if(_physical_port == nullptr)
	{
		logte("Cannot create physical port for %s (type: %d)", address.ToString().CStr(), type);
		return false;
	}

	_physical_port->AddObserver(this);

	return true;
}

bool IcePort::Close()
{
	if((_physical_port == nullptr) || _physical_port->Close())
	{
		return true;
	}

	return false;
}

ov::String IcePort::GenerateUfrag()
{
	std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

	while(true)
	{
		ov::String ufrag = ov::Random::GenerateString(6);

		if(_user_mapping_table.find(ufrag) == _user_mapping_table.end())
		{
			logtd("Generated ufrag: %s", ufrag.CStr());

			return ufrag;
		}
	}
}

bool IcePort::AddObserver(std::shared_ptr<IcePortObserver> observer)
{
	// 기존에 등록된 observer가 있는지 확인
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<IcePortObserver> const &value) -> bool
	{
		return value == observer;
	});

	if(item != _observers.end())
	{
		// 기존에 등록되어 있음
		logtw("%p is already observer", observer.get());
		return false;
	}

	_observers.push_back(observer);

	return true;
}

bool IcePort::RemoveObserver(std::shared_ptr<IcePortObserver> observer)
{
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<IcePortObserver> const &value) -> bool
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

bool IcePort::RemoveObservers()
{
	_observers.clear();

	return true;
}

void IcePort::AddSession(const std::shared_ptr<SessionInfo> &session_info, std::shared_ptr<SessionDescription> offer_sdp, std::shared_ptr<SessionDescription> peer_sdp)
{
	const ov::String &local_ufrag = offer_sdp->GetIceUfrag();
	const ov::String &remote_ufrag = peer_sdp->GetIceUfrag();

	{
		std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

		auto item = _user_mapping_table.find(local_ufrag);
		session_id_t session_id = session_info->GetId();

		if(item != _user_mapping_table.end())
		{
			OV_ASSERT(false, "Duplicated ufrag: %s:%s, session_id: %d (old session_id: %d)", local_ufrag.CStr(), remote_ufrag.CStr(), session_id, item->second->session_info->GetId());
		}

		logtd("Trying to add session: %d (ufrag: %s:%s)...", session_id, local_ufrag.CStr(), remote_ufrag.CStr());

		// 나중에 STUN Binding request를 대비하여 관련 정보들을 넣어놓음
		std::shared_ptr<IcePortInfo> info = std::make_shared<IcePortInfo>();

		info->session_info = session_info;
		info->offer_sdp = offer_sdp;
		info->peer_sdp = peer_sdp;
		info->remote = nullptr;
		info->address = ov::SocketAddress();
		info->state = IcePortConnectionState::Closed;

		info->UpdateBindingTime();

		_user_mapping_table[local_ufrag] = info;
	}

	SetIceState(_user_mapping_table[local_ufrag], IcePortConnectionState::New);
}

bool IcePort::RemoveSession(const std::shared_ptr<SessionInfo> &session_info)
{
	session_id_t session_id = session_info->GetId();

	auto item = _session_table.find(session_id);

	if(item == _session_table.end())
	{
		logtw("Could not find session: %d", session_id);

		return false;
	}

	_session_table.erase(item);
	_ice_port_info.erase(item->second->address);
	_user_mapping_table.erase(item->second->offer_sdp->GetIceUfrag());

	return true;
}

bool IcePort::Send(const std::shared_ptr<SessionInfo> &session_info, std::unique_ptr<RtpPacket> packet)
{
	return Send(session_info, packet->GetData());
}

bool IcePort::Send(const std::shared_ptr<SessionInfo> &session_info, std::unique_ptr<RtcpPacket> packet)
{
	return Send(session_info, packet->GetData());
}

bool IcePort::Send(const std::shared_ptr<SessionInfo> &session_info, const std::shared_ptr<const ov::Data> &data)
{
	// logtd("Finding socket from session #%d...", session_info->GetId());

	auto item = _session_table.find(session_info->GetId());

	if(item == _session_table.end())
	{
		// logtw("ClientSocket not found for session #%d", session_info->GetId());
		return false;
	}

	// logtd("Sending data to remote for session #%d", session_info->GetId());
	return item->second->remote->SendTo(item->second->address, data) >= 0;
}

void IcePort::OnConnected(ov::Socket *remote)
{
	// TODO: 일단은 UDP만 처리하므로, 비워둠. 나중에 TCP 지원할 때 구현해야 함
}

void IcePort::OnDataReceived(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	// 데이터를 수신했음

	// TODO: 지금은 data 안에 하나의 STUN 메시지만 있을 것으로 간주하고 작성되어 있음
	// TODO: TCP의 경우, 데이터가 많이 들어올 수 있기 때문에 별도 처리 필요

	ov::ByteStream stream(data.get());
	StunMessage message;

	if(message.Parse(stream))
	{
		// STUN 패킷이 맞음
		logtd("Received message:\n%s", message.ToString().CStr());

		if(message.GetMethod() == StunMethod::Binding)
		{
			switch(message.GetClass())
			{
				case StunClass::Request:
					if(ProcessBindingRequest(remote, address, message) == false)
					{
						ResponseError(remote);
					}
					break;

				case StunClass::SuccessResponse:
					if(ProcessBindingResponse(remote, address, message) == false)
					{
						ResponseError(remote);
					}
					break;

				case StunClass::ErrorResponse:
					// TODO: 구현 예정
					logtw("Error Response received");
					break;

				case StunClass::Indication:
					// indication은 언제/어떻게 사용하는지 spec을 더 봐야함
					logtw("Indication - not implemented");
					break;
			}
		}
		else
		{
			// binding 이외의 method는 구현되어 있지 않음
			OV_ASSERT(false, "Not implemented method: %d", message.GetMethod());
			logtw("Unknown method: %d", message.GetMethod());
			ResponseError(remote);
		}
	}
	else
	{
		logtd("Not Stun packet. Passing data to observer...");

		auto item = _ice_port_info.find(address);

		if(item == _ice_port_info.end())
		{
			// 포트 정보가 없음
			// 이전 단계에서 관련 정보가 저장되어 있어야 함
			logtw("Could not find client information. Dropping...");
			return;
		}

		// TODO: 이걸 IcePort에서 할 것이 아니라 PhysicalPort에서 하는 것이 좋아보임

		// observer들에게 알림
		for(auto &observer : _observers)
		{
			logtd("Trying to callback OnDataReceived() to %p...", observer.get());
			observer->OnDataReceived(*this, item->second->session_info, data);
			logtd("OnDataReceived() is returned (%p)", observer.get());
		}
	}
}

void IcePort::CheckTimedoutItem()
{
	std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

	for(auto item = _user_mapping_table.begin(); item != _user_mapping_table.end();)
	{
		if(item->second->IsExpired())
		{
			logtd("Client %s(session id: %d) is expired", item->second->address.ToString().CStr(), item->second->session_info->GetId());
			SetIceState(item->second, IcePortConnectionState::Disconnected);

			_session_table.erase(item->second->session_info->GetId());
			_ice_port_info.erase(item->second->address);

			item = _user_mapping_table.erase(item);
		}
		else
		{
			++item;
		}
	}
}

bool IcePort::ProcessBindingRequest(ov::Socket *remote, const ov::SocketAddress &address, const StunMessage &request_message)
{
	// Binding Request
	ov::String local_ufrag;
	ov::String remote_ufrag;

	if(request_message.GetUfrags(&local_ufrag, &remote_ufrag) == false)
	{
		logtw("Could not process user name attribute");
		return false;
	}

	logtd("Binding request for user: %s:%s", local_ufrag.CStr(), remote_ufrag.CStr());

	auto info = _user_mapping_table.find(local_ufrag);
	if(info == _user_mapping_table.end())
	{
		logtd("User not found: %s (AddSession() needed)", local_ufrag.CStr());
		return false;
	}

	if(info->second->peer_sdp->GetIceUfrag() != remote_ufrag)
	{
		// SDP에 명시된 ufrag와, 실제 STUN으로 들어온 ufrag가 다름
		logtw("Mismatched ufrag: %s (ufrag in peer SDP: %s)", remote_ufrag.CStr(), info->second->peer_sdp->GetIceUfrag().CStr());

		// TODO: SDP 파싱 기능이 완료되면 처리해야 함
		// return false;
	}

	// SDP의 password로 무결성 검사를 한 뒤
	if(request_message.CheckIntegrity(info->second->offer_sdp->GetIcePwd()) == false)
	{
		// 무결성 검사 실패
		logtw("Failed to check integrity");

		SetIceState(info->second, IcePortConnectionState::Failed);

		{
			std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

			_user_mapping_table.erase(info);
			_ice_port_info.erase(info->second->address);
			_session_table.erase(info->second->session_info->GetId());

			// TODO(dimiden): _ice_port_info 및 _session_table 도 같이 정리
		}

		return false;
	}

	info->second->UpdateBindingTime();

	if(info->second->state == IcePortConnectionState::New)
	{
		// 다음 Binding Request까지 Checking 상태 유지
		SetIceState(info->second, IcePortConnectionState::Checking);
		info->second->remote = remote;
		info->second->address = address;
	}

	return SendBindingResponse(remote, address, request_message, info->second);
}

bool IcePort::SendBindingResponse(ov::Socket *remote, const ov::SocketAddress &address, const StunMessage &request_message, const std::shared_ptr<IcePortInfo> &info)
{
	// Binding response 준비
	StunMessage response_message;

	response_message.SetClass(StunClass::SuccessResponse);
	response_message.SetMethod(StunMethod::Binding);
	response_message.SetTransactionId(request_message.GetTransactionId());

	std::unique_ptr<StunAttribute> attribute;

	// XOR-MAPPED-ADDRESS attribute 추가
	const sockaddr_in *addr_in = address.AddressForIPv4();
	attribute = std::make_unique<StunXorMappedAddressAttribute>();
	auto *mapped_attribute = dynamic_cast<StunXorMappedAddressAttribute *>(attribute.get());

	mapped_attribute->SetParameters(address);
	response_message.AddAttribute(std::move(attribute));

	// Integrity 계산을 위한 password 생성
	ov::String key = info->offer_sdp->GetIcePwd();

	// Integrity & Fingerprint attribute는 Serialize()할 때 자동 생성됨
	std::shared_ptr<ov::Data> serialized = response_message.Serialize(key);

	logtd("Generated STUN response:\n%s\n%s", response_message.ToString().CStr(), serialized->Dump().CStr());

	remote->SendTo(address, serialized);

	// client mapping 정보를 저장해놓음
	_ice_port_info[address] = info;
	_session_table[info->session_info->GetId()] = info;

	SendBindingRequest(remote, address, info);

	return true;
}

bool IcePort::SendBindingRequest(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<IcePortInfo> &info)
{
	// Binding request 준비
	StunMessage request_message;

	request_message.SetClass(StunClass::Request);
	request_message.SetMethod(StunMethod::Binding);
	// logtw("TEST PURPOSE TRANSACTION_ID");
	// TODO: transaction_id가 겹치지 않게 처리 해야 함
	uint8_t transaction_id[OV_STUN_TRANSACTION_ID_LENGTH];
	uint8_t charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	// random 한 transaction id 생성
	for(int index = 0; index < OV_STUN_TRANSACTION_ID_LENGTH; index++)
	{
		transaction_id[index] = charset[rand() % OV_COUNTOF(charset)];
	}
	request_message.SetTransactionId(&(transaction_id[0]));

	std::unique_ptr<StunAttribute> attribute;

	// USERNAME attribute 추가
	attribute = std::make_unique<StunUserNameAttribute>();
	auto *user_name_attribute = dynamic_cast<StunUserNameAttribute *>(attribute.get());
	user_name_attribute->SetUserName(ov::String::FormatString("%s:%s", info->peer_sdp->GetIceUfrag().CStr(), info->offer_sdp->GetIceUfrag().CStr()));
	request_message.AddAttribute(std::move(attribute));

	// Unknown attribute 추가 (hash 테스트용)
	// https://tools.ietf.org/html/draft-thatcher-ice-network-cost-00
	// https://www.ietf.org/mail-archive/web/ice/current/msg00247.html
	attribute = std::make_unique<StunUnknownAttribute>(0xC057, 4);
	auto *unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	uint8_t unknown_data[] = { 0x00, 0x02, 0x00, 0x00 };
	unknown_attribute->SetData(&(unknown_data[0]), 4);
	request_message.AddAttribute(std::move(attribute));

	// ICE-CONTROLLING 추가 (hash 테스트용)
	attribute = std::make_unique<StunUnknownAttribute>(0x802A, 8);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	uint8_t unknown_data2[] = { 0x1C, 0xF5, 0x1E, 0xB1, 0xB0, 0xCB, 0xE3, 0x49 };
	unknown_attribute->SetData(&(unknown_data2[0]), 8);
	request_message.AddAttribute(std::move(attribute));

	// USE-CANDIDATE 추가 - Required (hash 테스트용)
	attribute = std::make_unique<StunUnknownAttribute>(0x0025, 0);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	request_message.AddAttribute(std::move(attribute));

	// PRIORITY 추가 - Required (hash 테스트용)
	attribute = std::make_unique<StunUnknownAttribute>(0x0024, 4);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	uint8_t unknown_data3[] = { 0x6E, 0x7F, 0x1E, 0xFF };
	unknown_attribute->SetData(&(unknown_data3[0]), 4);
	request_message.AddAttribute(std::move(attribute));

	// Integrity 계산을 위한 password 생성
	ov::String key = info->peer_sdp->GetIcePwd();

	// Integrity & Fingerprint attribute는 Serialize()할 때 자동 생성됨
	std::shared_ptr<ov::Data> serialized = request_message.Serialize(key);

	logtd("Generated STUN response:\n%s\n%s", request_message.ToString().CStr(), serialized->Dump().CStr());

	remote->SendTo(address, serialized);

	return true;
}

bool IcePort::ProcessBindingResponse(ov::Socket *remote, const ov::SocketAddress &address, const StunMessage &response_message)
{
	// TODO: state가 checking 상태인지 확인

	auto item = _ice_port_info.find(address);

	if(item == _ice_port_info.end())
	{
		// 포트 정보가 없음
		// 이전 단계에서 관련 정보가 저장되어 있어야 함
		logtw("Could not find client information");
		return false;
	}

	// SDP의 password로 무결성 검사를 한 뒤
	if(response_message.CheckIntegrity(item->second->offer_sdp->GetIcePwd()) == false)
	{
		// 무결성 검사 실패
		logtw("Failed to check integrity");
		return false;
	}

	if(item->second->state != IcePortConnectionState::Connected)
	{
		// 다음 Binding Request까지 Checking 상태 유지
		SetIceState(item->second, IcePortConnectionState::Connected);
	}

	return true;
}

void IcePort::OnDisconnected(ov::Socket *remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	// TODO: TCP 연결이 해제되었을 때 처리. 일단은 UDP만 처리하므로, 비워둠
}

void IcePort::SetIceState(std::shared_ptr<IcePortInfo> &info, IcePortConnectionState state)
{
	info->state = state;

	auto func = std::bind(&IcePortObserver::OnStateChanged, std::placeholders::_1, std::ref(*this), std::ref(info->session_info), state);
	std::for_each(_observers.begin(), _observers.end(), func);
}

// STUN 오류를 반환함
void IcePort::ResponseError(ov::Socket *remote)
{
	// TOOD: 구현 필요 - chrome에서는 오류가 발생했을 때, 별다른 조치를 취하지 않는 것 같음
}

ov::String IcePort::ToString() const
{
	if(_physical_port == nullptr)
	{
		return ov::String::FormatString("<IcePort: %p, [Invalid physical port]>", this);
	}

	return ov::String::FormatString("<IcePort: %p, %s - %s>",
	                                this,
	                                (_physical_port->GetType() == ov::SocketType::Tcp) ? "TCP" : ((_physical_port->GetType() == ov::SocketType::Udp) ? "UDP" : "Unknown"),
	                                _physical_port->GetAddress().ToString().CStr()
	);
}
