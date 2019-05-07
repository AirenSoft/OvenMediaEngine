//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtc_p2p_manager.h"

#include "../rtc_signalling_server_private.h"

std::shared_ptr<RtcPeerInfo> RtcP2PManager::CreatePeerInfo(peer_id_t id, const std::shared_ptr<WebSocketClient> &response)
{
	auto user_agent = response->GetRequest()->GetHeader("USER-AGENT");
	auto peer_info = RtcPeerInfo::FromUserAgent(id, user_agent, response);

	if(peer_info != nullptr)
	{
		std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);

		auto previous_peer_info = _peer_list.find(id);

		if(previous_peer_info != _peer_list.end())
		{
			// Already exists
			logtd("Already exists: %s (%s)", user_agent.CStr(), peer_info->ToString().CStr());
			peer_info = nullptr;
		}
		else
		{
			logtd("New peer: %s (%s)", user_agent.CStr(), peer_info->ToString().CStr());
			_peer_list[id] = peer_info;
		}
	}

	return peer_info;
}

std::shared_ptr<RtcPeerInfo> RtcP2PManager::FindPeer(peer_id_t peer_id)
{
	std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);

	auto peer_info = _peer_list.find(peer_id);

	if(peer_info != _peer_list.end())
	{
		return peer_info->second;
	}

	return nullptr;
}

bool RtcP2PManager::RemovePeer(const std::shared_ptr<RtcPeerInfo> &peer, size_t max_clients_per_host)
{
	std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);

	if(peer->IsHost())
	{
		// Remove client peers
		auto client_list = peer->_client_list;
	}
	else
	{
		// Remove client from host peer
		auto host_peer = peer->_host_peer;

		if(host_peer != nullptr)
		{
			host_peer->_client_list.erase(peer->GetId());
			if(host_peer->_client_list.size() < max_clients_per_host)
			{
				if(host_peer->CanAccept())
				{
					_available_list[peer->GetId()] = host_peer;
				}
			}
		}

		_total_client_count--;
	}

	_available_list.erase(peer->GetId());

	auto peer_info = _peer_list.find(peer->GetId());

	if(peer_info == _peer_list.end())
	{
		return false;
	}

	_peer_list.erase(peer->GetId());

	return true;
}

bool RtcP2PManager::RegisterAsHostPeer(const std::shared_ptr<RtcPeerInfo> &peer)
{
	if(peer == nullptr)
	{
		logtw("Invalid peer");
		OV_ASSERT2(false);
		return false;
	}

	std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);

	auto peer_info = FindPeer(peer->GetId());

	if(peer_info == nullptr)
	{
		logtw("The peer does not exists: %s", peer->ToString().CStr());
		return false;
	}

	if(peer_info->IsHost())
	{
		logtw("The peer is already host: %s", peer_info->ToString().CStr());
		return false;
	}

	peer_info->MakeAsHost();

	if(peer_info->CanAccept())
	{
		_available_list[peer->GetId()] = peer_info;
	}

	return true;
}

std::shared_ptr<RtcPeerInfo> RtcP2PManager::TryToRegisterAsClientPeer(const std::shared_ptr<RtcPeerInfo> &peer, size_t max_clients_per_host)
{
	if(peer == nullptr)
	{
		logtw("Invalid client peer");
		OV_ASSERT2(false);

		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);

	for(const auto &host : _available_list)
	{
		auto &host_peer = host.second;

		if(host_peer->IsCompatibleWith(peer))
		{
			OV_ASSERT2(host_peer->IsHost());
			OV_ASSERT2(host_peer->IsHost());

			auto &client_list = host_peer->_client_list;

			peer_id_t client_id = peer->GetId();

			auto client_peer = client_list.find(client_id);

			if(client_peer == client_list.end())
			{
				client_list[client_id] = peer;
				_total_client_count++;

				peer->_host_peer = host_peer;

				if(client_list.size() >= max_clients_per_host)
				{
					// Now, the host cannot accept another client
					_available_list.erase(host.first);
				}
			}
			else
			{
				OV_ASSERT2(false);
				logtw("Client peer %s is already exists (previous peer: %s)", peer->ToString().CStr(), client_peer->second->ToString().CStr());
			}

			return host_peer;
		}
	}

	return nullptr;
}

std::shared_ptr<RtcPeerInfo> RtcP2PManager::GetClientPeerOf(const std::shared_ptr<RtcPeerInfo> &host, peer_id_t client_id)
{
	if(host != nullptr)
	{
		std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);

		auto &client_list = host->_client_list;
		auto client = client_list.find(client_id);

		if(client == client_list.end())
		{
			return nullptr;
		}

		return client->second;
	}
	else
	{
		OV_ASSERT2(false);
	}

	return nullptr;
}

std::map<peer_id_t, std::shared_ptr<RtcPeerInfo>> RtcP2PManager::GetClientPeerList(const std::shared_ptr<RtcPeerInfo> &host)
{
	std::map<peer_id_t, std::shared_ptr<RtcPeerInfo>> list;

	if(host == nullptr)
	{
		return list;
	}

	{
		std::lock_guard<std::recursive_mutex> lock_guard(_list_mutex);
		list = host->_client_list;
	}

	return std::move(list);
}

int RtcP2PManager::GetPeerCount() const
{
	return static_cast<int>(_peer_list.size());
}

int RtcP2PManager::GetClientPeerCount() const
{
	return _total_client_count;
}