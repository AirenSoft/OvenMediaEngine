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

#include "modules/ice/stun/attributes/stun_attributes.h"
#include "modules/ice/stun/stun_message.h"

#include <algorithm>

#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <modules/rtc_signalling/rtc_ice_candidate.h>
#include <base/info/stream.h>
#include <base/info/application.h>

IcePort::IcePort()
{
	_timer.Push(
		[this](void *paramter) -> ov::DelayQueueAction {
			CheckTimedoutItem();
			return ov::DelayQueueAction::Repeat;
		},
		1000);
	_timer.Start();
}

IcePort::~IcePort()
{
	_timer.Stop();

	Close();
}

bool IcePort::Create(std::vector<RtcIceCandidate> ice_candidate_list)
{
	std::lock_guard<std::recursive_mutex> lock_guard(_physical_port_list_mutex);

	if (_physical_port_list.empty() == false)
	{
		logtw("IcePort is running");
		return false;
	}

	bool succeeded = true;
	std::map<int, bool> bounded;

	for (auto &ice_candidate : ice_candidate_list)
	{
		auto transport = ice_candidate.GetTransport().UpperCaseString();
		auto address = ice_candidate.GetAddress();
		ov::SocketType socket_type = ov::SocketType::Udp;

		if (transport == "TCP")
		{
			socket_type = ov::SocketType::Tcp;
		}

		{
			auto port = address.Port();
			auto item = bounded.find(port);

			if (item != bounded.end())
			{
				// Already opened
				continue;
			}

			bounded[port] = true;
		}

		// Bind to 0.0.0.0
		address.SetHostname(nullptr);

		// Create an ICE port using candidate information
		auto physical_port = CreatePhysicalPort(address, socket_type);

		if (physical_port == nullptr)
		{
			logte("Could not create physical port for %s/%s", address.ToString().CStr(), transport.CStr());
			succeeded = false;
			break;
		}

		logti("ICE port is bound to %s/%s (%p)", address.ToString().CStr(), transport.CStr(), physical_port.get());
		_physical_port_list.push_back(physical_port);
	}

	if (succeeded)
	{
		_ice_candidate_list = std::move(ice_candidate_list);
	}
	else
	{
		Close();
	}

	return succeeded;
}

const std::vector<RtcIceCandidate> &IcePort::GetIceCandidateList() const
{
	return _ice_candidate_list;
}

std::shared_ptr<PhysicalPort> IcePort::CreatePhysicalPort(const ov::SocketAddress &address, ov::SocketType type)
{
	auto physical_port = PhysicalPortManager::GetInstance()->CreatePort(type, address);

	if (physical_port != nullptr)
	{
		if (physical_port->AddObserver(this))
		{
			return physical_port;
		}

		logte("Cannot add a observer %p to %p", this, physical_port.get());

		PhysicalPortManager::GetInstance()->DeletePort(physical_port);
	}
	else
	{
		logte("Cannot create physical port for %s (type: %d)", address.ToString().CStr(), type);
	}

	return nullptr;
}

bool IcePort::Close()
{
	std::lock_guard<std::recursive_mutex> lock_guard(_physical_port_list_mutex);

	bool result = true;

	for (auto &physical_port : _physical_port_list)
	{
		result = result && physical_port->RemoveObserver(this);
		result = result && PhysicalPortManager::GetInstance()->DeletePort(physical_port);

		if (result == false)
		{
			logtd("Cannot close ICE port");
			break;
		}
	}

	_ice_candidate_list.clear();
	_timer.Stop();

	return result;
}

ov::String IcePort::GenerateUfrag()
{
	std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

	while (true)
	{
		ov::String ufrag = ov::Random::GenerateString(6);

		if (_user_mapping_table.find(ufrag) == _user_mapping_table.end())
		{
			logtd("Generated ufrag: %s", ufrag.CStr());

			return ufrag;
		}
	}
}

bool IcePort::AddObserver(std::shared_ptr<IcePortObserver> observer)
{
	// 기존에 등록된 observer가 있는지 확인
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<IcePortObserver> const &value) -> bool {
		return value == observer;
	});

	if (item != _observers.end())
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
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<IcePortObserver> const &value) -> bool {
		return value == observer;
	});

	if (item == _observers.end())
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

void IcePort::AddSession(const std::shared_ptr<info::Session> &session_info, std::shared_ptr<const SessionDescription> offer_sdp, std::shared_ptr<const SessionDescription> peer_sdp)
{
	const ov::String &local_ufrag = offer_sdp->GetIceUfrag();
	const ov::String &remote_ufrag = peer_sdp->GetIceUfrag();

	{
		std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

		auto item = _user_mapping_table.find(local_ufrag);

		[[maybe_unused]] session_id_t session_id = session_info->GetId();

		if (item != _user_mapping_table.end())
		{
			OV_ASSERT(false, "Duplicated ufrag: %s:%s, session_id: %d (old session_id: %d)", local_ufrag.CStr(), remote_ufrag.CStr(), session_id, item->second->session_info->GetId());
		}

		logtd("Trying to add session: %d (ufrag: %s:%s)...", session_id, local_ufrag.CStr(), remote_ufrag.CStr());

		// 나중에 STUN Binding request를 대비하여 관련 정보들을 넣어놓음
		auto expire_after_ms = session_info->GetStream().GetApplicationInfo().GetConfig().GetPublishers().GetWebrtcPublisher().GetTimeout();

		std::shared_ptr<IcePortInfo> info = std::make_shared<IcePortInfo>(expire_after_ms);

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

bool IcePort::RemoveSession(const session_id_t session_id)
{
	std::shared_ptr<IcePortInfo> ice_port_info;

	{
		std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

		auto item = _session_table.find(session_id);

		if (item == _session_table.end())
		{
			logtw("Could not find session: %d", session_id);

			return false;
		}

		ice_port_info = item->second;

		_session_table.erase(item);
		_ice_port_info.erase(ice_port_info->address);
	}

	{
		std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);
		_user_mapping_table.erase(ice_port_info->offer_sdp->GetIceUfrag());
	}

	return true;
}

bool IcePort::RemoveSession(const std::shared_ptr<info::Session> &session_info)
{
	session_id_t session_id = session_info->GetId();
	return RemoveSession(session_id);
}

bool IcePort::Send(const std::shared_ptr<info::Session> &session_info, std::unique_ptr<RtpPacket> packet)
{
	return Send(session_info, packet->GetData());
}

bool IcePort::Send(const std::shared_ptr<info::Session> &session_info, std::unique_ptr<RtcpPacket> packet)
{
	return Send(session_info, packet->GetData());
}

bool IcePort::Send(const std::shared_ptr<info::Session> &session_info, const std::shared_ptr<const ov::Data> &data)
{
	// logtd("Finding socket from session #%d...", session_info->GetId());

	std::shared_ptr<IcePortInfo> ice_port_info;

	{
		std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

		auto item = _session_table.find(session_info->GetId());

		if (item == _session_table.end())
		{
			// logtw("ClientSocket not found for session #%d", session_info->GetId());
			return false;
		}

		ice_port_info = item->second;
	}

	// logtd("Sending data to remote for session #%d", session_info->GetId());
	return ice_port_info->remote->SendTo(ice_port_info->address, data) >= 0;
}

void IcePort::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	// TODO: 일단은 UDP만 처리하므로, 비워둠. 나중에 TCP 지원할 때 구현해야 함
}

void IcePort::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	// 데이터를 수신했음

	// TODO: 지금은 data 안에 하나의 STUN 메시지만 있을 것으로 간주하고 작성되어 있음
	// TODO: TCP의 경우, 데이터가 많이 들어올 수 있기 때문에 별도 처리 필요

	ov::ByteStream stream(data.get());
	StunMessage message;

	if (message.Parse(stream))
	{
		// STUN 패킷이 맞음
		logtd("Received message:\n%s", message.ToString().CStr());

		if (message.GetMethod() == StunMethod::Binding)
		{
			switch (message.GetClass())
			{
				case StunClass::Request:
					if (ProcessBindingRequest(remote, address, message) == false)
					{
						ResponseError(remote);
					}
					break;

				case StunClass::SuccessResponse:
					if (ProcessBindingResponse(remote, address, message) == false)
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

		std::shared_ptr<IcePortInfo> ice_port_info;

		{
			std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

			auto item = _ice_port_info.find(address);

			if (item != _ice_port_info.end())
			{
				ice_port_info = item->second;
			}
		}

		if (ice_port_info == nullptr)
		{
			// 포트 정보가 없음
			// 이전 단계에서 관련 정보가 저장되어 있어야 함
			logtd("Could not find client information. Dropping...");
			return;
		}

		// TODO: 이걸 IcePort에서 할 것이 아니라 PhysicalPort에서 하는 것이 좋아보임

		// observer들에게 알림
		for (auto &observer : _observers)
		{
			logtd("Trying to callback OnDataReceived() to %p...", observer.get());
			observer->OnDataReceived(*this, ice_port_info->session_info, data);
			logtd("OnDataReceived() is returned (%p)", observer.get());
		}
	}
}

void IcePort::CheckTimedoutItem()
{
	std::vector<std::shared_ptr<IcePortInfo>> delete_list;

	{
		std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

		for (auto item = _user_mapping_table.begin(); item != _user_mapping_table.end();)
		{
			if (item->second->IsExpired())
			{
				logtd("Client %s(session id: %d) is expired", item->second->address.ToString().CStr(), item->second->session_info->GetId());
				SetIceState(item->second, IcePortConnectionState::Disconnected);

				delete_list.push_back(item->second);

				item = _user_mapping_table.erase(item);
			}
			else
			{
				++item;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

		for (auto &deleted_ice_port : delete_list)
		{
			_session_table.erase(deleted_ice_port->session_info->GetId());
			_ice_port_info.erase(deleted_ice_port->address);
		}
	}
}

bool IcePort::ProcessBindingRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const StunMessage &request_message)
{
	// Binding Request
	ov::String local_ufrag;
	ov::String remote_ufrag;

	if (request_message.GetUfrags(&local_ufrag, &remote_ufrag) == false)
	{
		logtw("Could not process user name attribute");
		return false;
	}

	logtd("Client %s sent STUN binding request: %s:%s", address.ToString().CStr(), local_ufrag.CStr(), remote_ufrag.CStr());

	std::shared_ptr<IcePortInfo> ice_port_info;

	{
		std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

		auto info = _user_mapping_table.find(local_ufrag);

		if (info == _user_mapping_table.end())
		{
			logtd("User not found: %s (AddSession() needed)", local_ufrag.CStr());
			return false;
		}

		ice_port_info = info->second;
	}

	if (ice_port_info->peer_sdp->GetIceUfrag() != remote_ufrag)
	{
		// SDP에 명시된 ufrag와, 실제 STUN으로 들어온 ufrag가 다름
		logtw("Mismatched ufrag: %s (ufrag in peer SDP: %s)", remote_ufrag.CStr(), ice_port_info->peer_sdp->GetIceUfrag().CStr());

		// TODO: SDP 파싱 기능이 완료되면 처리해야 함
		// return false;
	}

	// SDP의 password로 무결성 검사를 한 뒤
	if (request_message.CheckIntegrity(ice_port_info->offer_sdp->GetIcePwd()) == false)
	{
		// 무결성 검사 실패
		logtw("Failed to check integrity");

		SetIceState(ice_port_info, IcePortConnectionState::Failed);

		{
			std::lock_guard<std::mutex> lock_guard(_user_mapping_table_mutex);

			_user_mapping_table.erase(local_ufrag);
		}

		{
			std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

			_ice_port_info.erase(ice_port_info->address);
			_session_table.erase(ice_port_info->session_info->GetId());
		}

		return false;
	}

	ice_port_info->UpdateBindingTime();

	if (ice_port_info->state == IcePortConnectionState::New)
	{
		// 다음 Binding Request까지 Checking 상태 유지
		SetIceState(ice_port_info, IcePortConnectionState::Checking);
		ice_port_info->remote = remote;
		ice_port_info->address = address;
	}

	return SendBindingResponse(remote, address, request_message, ice_port_info);
}

bool IcePort::SendBindingResponse(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const StunMessage &request_message, const std::shared_ptr<IcePortInfo> &info)
{
	// Binding response 준비
	StunMessage response_message;

	response_message.SetClass(StunClass::SuccessResponse);
	response_message.SetMethod(StunMethod::Binding);
	response_message.SetTransactionId(request_message.GetTransactionId());

	std::unique_ptr<StunAttribute> attribute;

	// XOR-MAPPED-ADDRESS attribute 추가
	attribute = std::make_unique<StunXorMappedAddressAttribute>();
	auto *mapped_attribute = dynamic_cast<StunXorMappedAddressAttribute *>(attribute.get());

	mapped_attribute->SetParameters(address);
	response_message.AddAttribute(std::move(attribute));

	// Integrity 계산을 위한 password 생성
	ov::String key = info->offer_sdp->GetIcePwd();

	// Integrity & Fingerprint attribute는 Serialize()할 때 자동 생성됨
	std::shared_ptr<ov::Data> serialized = response_message.Serialize(key);

	logtd("Trying to send STUN binding response to %s\n%s\n%s", address.ToString().CStr(), response_message.ToString().CStr(), serialized->Dump().CStr());

	remote->SendTo(address, serialized);

	// client mapping 정보를 저장해놓음
	{
		std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

		if (_session_table.find(info->session_info->GetId()) == _session_table.end())
		{
			logtd("Add the client to the port list: %s", address.ToString().CStr());

			_ice_port_info[address] = info;
			_session_table[info->session_info->GetId()] = info;
		}
		else
		{
			// Updated
		}
	}

	SendBindingRequest(remote, address, info);

	return true;
}

bool IcePort::SendBindingRequest(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<IcePortInfo> &info)
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
	for (int index = 0; index < OV_STUN_TRANSACTION_ID_LENGTH; index++)
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
	StunUnknownAttribute *unknown_attribute = nullptr;
	// https://tools.ietf.org/html/draft-thatcher-ice-network-cost-00
	// https://www.ietf.org/mail-archive/web/ice/current/msg00247.html
	// attribute = std::make_unique<StunUnknownAttribute>(0xC057, 4);
	// auto *unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	// uint8_t unknown_data[] = { 0x00, 0x02, 0x00, 0x00 };
	// unknown_attribute->SetData(&(unknown_data[0]), 4);
	// request_message.AddAttribute(std::move(attribute));

	// ICE-CONTROLLING 추가 (hash 테스트용)
	attribute = std::make_unique<StunUnknownAttribute>(0x802A, 8);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	uint8_t unknown_data2[] = {0x1C, 0xF5, 0x1E, 0xB1, 0xB0, 0xCB, 0xE3, 0x49};
	unknown_attribute->SetData(&(unknown_data2[0]), 8);
	request_message.AddAttribute(std::move(attribute));

	// USE-CANDIDATE 추가 - Required (hash 테스트용)
	attribute = std::make_unique<StunUnknownAttribute>(0x0025, 0);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	request_message.AddAttribute(std::move(attribute));

	// PRIORITY 추가 - Required (hash 테스트용)
	attribute = std::make_unique<StunUnknownAttribute>(0x0024, 4);
	unknown_attribute = dynamic_cast<StunUnknownAttribute *>(attribute.get());
	uint8_t unknown_data3[] = {0x6E, 0x7F, 0x1E, 0xFF};
	unknown_attribute->SetData(&(unknown_data3[0]), 4);
	request_message.AddAttribute(std::move(attribute));

	// Integrity 계산을 위한 password 생성
	ov::String key = info->peer_sdp->GetIcePwd();

	// Integrity & Fingerprint attribute는 Serialize()할 때 자동 생성됨
	std::shared_ptr<ov::Data> serialized = request_message.Serialize(key);

	logtd("Trying to send STUN binding request to %s\n%s\n%s", address.ToString().CStr(), request_message.ToString().CStr(), serialized->Dump().CStr());

	remote->SendTo(address, serialized);

	return true;
}

bool IcePort::ProcessBindingResponse(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const StunMessage &response_message)
{
	// TODO: state가 checking 상태인지 확인

	std::shared_ptr<IcePortInfo> ice_port_info;

	{
		std::lock_guard<std::mutex> lock_guard(_ice_port_info_mutex);

		auto item = _ice_port_info.find(address);

		if (item == _ice_port_info.end())
		{
			// 포트 정보가 없음
			// 이전 단계에서 관련 정보가 저장되어 있어야 함

			// 같은 ufrag에 대해 서로 다른 ICE candidate로 부터 동시에 접속 요청이 왔다면, 첫 번째로 도착한 ICE candidate가 저장됨
			// 따라서 두 번째 address는 처리하지 않으므로, 없다고 간주
			return false;
		}

		ice_port_info = item->second;
	}

	// SDP의 password로 무결성 검사를 한 뒤
	if (response_message.CheckIntegrity(ice_port_info->offer_sdp->GetIcePwd()) == false)
	{
		// 무결성 검사 실패
		logtw("Failed to check integrity");
		return false;
	}

	logtd("Client %s sent STUN binding response", address.ToString().CStr());

	if (ice_port_info->state != IcePortConnectionState::Connected)
	{
		// 다음 Binding Request까지 Checking 상태 유지
		SetIceState(ice_port_info, IcePortConnectionState::Connected);
	}

	return true;
}

void IcePort::OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
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
void IcePort::ResponseError(const std::shared_ptr<ov::Socket> &remote)
{
	// TOOD: 구현 필요 - chrome에서는 오류가 발생했을 때, 별다른 조치를 취하지 않는 것 같음
}

ov::String IcePort::ToString() const
{
	return ov::String::FormatString("<IcePort: %p, %zu ports>", this, _physical_port_list.size());
}
