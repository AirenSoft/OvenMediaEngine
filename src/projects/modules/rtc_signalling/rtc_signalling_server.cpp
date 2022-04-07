//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtc_signalling_server.h"

#include <config/config_manager.h>
#include <modules/address/address_utilities.h>
#include <modules/ice/ice.h>
#include <modules/ice/ice_port.h>
#include <publishers/webrtc/webrtc_publisher.h>

#include <utility>

#include "rtc_ice_candidate.h"
#include "rtc_signalling_server_private.h"

RtcSignallingServer::RtcSignallingServer(const cfg::Server &server_config, const cfg::bind::cmm::Webrtc &webrtc_config)
	: _server_config(server_config),
	  _webrtc_config(webrtc_config),
	  _p2p_manager(server_config)
{
}

bool RtcSignallingServer::Start(const ov::SocketAddress *address, const ov::SocketAddress *tls_address, int worker_count, std::shared_ptr<http::svr::ws::Interceptor> interceptor)
{
	//	const auto &webrtc_config = _server_config.GetBind().GetPublishers().GetWebrtc();

	if ((_http_server != nullptr) || (_https_server != nullptr))
	{
		OV_ASSERT(false, "Server is already running (%p, %p)", _http_server.get(), _https_server.get());
		return false;
	}

	if (interceptor == nullptr)
	{
		OV_ASSERT(false, "Interceptor must not be nullptr");
		return false;
	}

	bool result = true;
	auto manager = http::svr::HttpServerManager::GetInstance();
	std::shared_ptr<http::svr::HttpServer> http_server = nullptr;
	std::shared_ptr<http::svr::HttpsServer> https_server = nullptr;

	if (address != nullptr)
	{
		_http_server_address = *address;
		http_server = manager->CreateHttpServer("RtcSig", *address, worker_count);
	}

	if (tls_address != nullptr)
	{
		_https_server_address = *tls_address;
		https_server = manager->CreateHttpsServer("RtcSig", *tls_address, false, worker_count);
	}

	if (SetWebSocketHandler(interceptor) == false)
	{
		OV_ASSERT(false, "SetWebSocketHandler failed");
		return false;
	}

	result = result && ((http_server == nullptr) || http_server->AddInterceptor(interceptor));
	result = result && ((https_server == nullptr) || https_server->AddInterceptor(interceptor));

	if (result)
	{
		_ice_servers = Json::arrayValue;
		_new_ice_servers = Json::arrayValue;

		// for internal turn/tcp relay configuration
		_tcp_force = _webrtc_config.GetIceCandidates().IsTcpForce();

		bool tcp_relay_parsed = false;
		auto tcp_relay_address = _webrtc_config.GetIceCandidates().GetTcpRelay(&tcp_relay_parsed);
		if (tcp_relay_parsed)
		{
			Json::Value ice_server = Json::objectValue;
			Json::Value new_ice_server = Json::objectValue;
			Json::Value urls = Json::arrayValue;

			// <TcpRelay>IP:Port</TcpRelay>
			// <TcpRelay>*:Port</TcpRelay>
			// <TcpRelay>${PublicIP}:Port</TcpRelay>
			// Check tcp_relay_address is * or ${PublicIP}
			auto address_items = tcp_relay_address.Split(":");
			if (address_items.size() != 2)
			{
			}

			if (address_items[0] == "*")
			{
				auto ip_list = ov::AddressUtilities::GetInstance()->GetIpList();
				for (const auto &ip : ip_list)
				{
					urls.append(ov::String::FormatString("turn:%s:%s?transport=tcp", ip.CStr(), address_items[1].CStr()).CStr());
				}
			}
			else if (address_items[0] == "${PublicIP}")
			{
				auto public_ip = ov::AddressUtilities::GetInstance()->GetMappedAddress();
				urls.append(ov::String::FormatString("turn:%s:%s?transport=tcp", public_ip->GetIpAddress().CStr(), address_items[1].CStr()).CStr());
			}
			else
			{
				urls.append(ov::String::FormatString("turn:%s?transport=tcp", tcp_relay_address.CStr()).CStr());
			}

			ice_server["urls"] = urls;
			new_ice_server["urls"] = urls;

			// Embedded turn server has fixed user_name and credential. Security is provided by signed policy after this. This is because the embedded turn server does not relay other servers and only transmits the local stream to tcp when transmitting to webrtc.

			// "user_name" is out of specification. This is a bug and "username" is correct. "user_name" will be deprecated in the future.
			ice_server["user_name"] = DEFAULT_RELAY_USERNAME;
			new_ice_server["username"] = DEFAULT_RELAY_USERNAME;

			ice_server["credential"] = DEFAULT_RELAY_KEY;
			new_ice_server["credential"] = DEFAULT_RELAY_KEY;

			_ice_servers.append(ice_server);
			_new_ice_servers.append(new_ice_server);
		}

		// for external ice server configuration
		auto &ice_servers_config = _webrtc_config.GetIceServers();
		if (ice_servers_config.IsParsed())
		{
			for (auto ice_server_config : ice_servers_config.GetIceServerList())
			{
				Json::Value ice_server = Json::objectValue;
				Json::Value new_ice_server = Json::objectValue;

				// URLS
				auto &url_list = ice_server_config.GetUrls().GetUrlList();

				if (url_list.size() == 0)
				{
					logtw("There is no URL list in ICE Servers");
					continue;
				}

				Json::Value urls = Json::arrayValue;
				for (auto url : url_list)
				{
					urls.append(url.CStr());
				}
				ice_server["urls"] = urls;
				new_ice_server["urls"] = urls;

				// UserName
				if (ice_server_config.GetUserName().IsEmpty() == false)
				{
					// "user_name" is out of specification. This is a bug and "username" is correct. "user_name" will be deprecated in the future.
					ice_server["user_name"] = ice_server_config.GetUserName().CStr();
					new_ice_server["username"] = ice_server_config.GetUserName().CStr();
				}

				// Credential
				if (ice_server_config.GetCredential().IsEmpty() == false)
				{
					ice_server["credential"] = ice_server_config.GetCredential().CStr();
					new_ice_server["credential"] = ice_server_config.GetCredential().CStr();
				}

				_ice_servers.append(ice_server);
				_new_ice_servers.append(new_ice_server);
			}
		}

		if (_ice_servers.empty())
		{
			_ice_servers = Json::nullValue;
		}

		_http_server = http_server;
		_https_server = https_server;
	}
	else
	{
		// Rollback
		manager->ReleaseServer(http_server);
		manager->ReleaseServer(https_server);
	}

	return result;
}

bool RtcSignallingServer::AppendCertificate(const std::shared_ptr<const info::Certificate> &certificate)
{
	if (_https_server != nullptr && certificate != nullptr)
	{
		return _https_server->AppendCertificate(certificate) == nullptr;
	}

	return true;
}

bool RtcSignallingServer::RemoveCertificate(const std::shared_ptr<const info::Certificate> &certificate)
{
	if (_https_server != nullptr && certificate != nullptr)
	{
		return _https_server->RemoveCertificate(certificate) == nullptr;
	}

	return true;
}

bool RtcSignallingServer::SetWebSocketHandler(std::shared_ptr<http::svr::ws::Interceptor> interceptor)
{
	interceptor->SetConnectionHandler(
		[this](const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session) -> bool {
			auto request = ws_session->GetRequest();
			auto response = ws_session->GetResponse();
			auto remote = request->GetRemote();

			if (remote == nullptr)
			{
				OV_ASSERT(false, "Cannot find the client information: %s", ws_session->ToString().CStr());
				return false;
			}

			ov::String description = remote->ToString();

			logti("New client is connected: %s", description.CStr());

			auto uri = ov::Url::Parse(request->GetUri());

			if (uri == nullptr)
			{
				logtw("Invalid request from %s. Disconnecting...", description.CStr());
				return false;
			}

			// Find the "Host" header
			auto host_name = request->GetHeader("HOST").Split(":")[0];
			auto &app_name = uri->App();
			auto &stream_name = uri->Stream();

			info::VHostAppName vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, app_name);

			auto info = std::make_shared<RtcSignallingInfo>(vhost_app_name, host_name, app_name, stream_name);

			{
				auto lock_guard = std::lock_guard(_client_list_mutex);

				while (true)
				{
					peer_id_t id = ov::Random::GenerateInt32(1, INT32_MAX);

					auto client = _client_list.find(id);

					if (client == _client_list.end())
					{
						info->id = id;
						_client_list[id] = info;

						break;
					}
				}
			}

			ws_session->SetExtra(info);

			return true;
		});

	interceptor->SetMessageHandler(
		[this](const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const std::shared_ptr<const ov::Data> &message) -> bool {
			auto connection = ws_session->GetConnection();
			auto request = ws_session->GetRequest();

			logtp("The client sent a message:\n%s", message->Dump().CStr());

			auto info = ws_session->GetExtraAs<RtcSignallingInfo>();

			if (info == nullptr)
			{
				// If you enter here, there is only the following situation:
				//
				// 1. An error occurred during the connection (request was wrong)
				// 2. After the connection is lost, the callback is called late
				logtw("Could not find client information: %s", ws_session->ToString().CStr());

				return false;
			}

			ov::JsonObject object = ov::Json::Parse(message);

			if (object.IsNull())
			{
				logtw("Invalid request message from %s", ws_session->ToString().CStr());
				return false;
			}

			auto &payload = object.GetJsonValue();

			if ((payload.isObject() == false) || (payload.isMember("command") == false))
			{
				logtw("Invalid request message from %s", ws_session->ToString().CStr());
				return false;
			}

			auto &command_value = payload["command"];

			ov::String command = ov::Converter::ToString(command_value);

			logtd("Trying to dispatch command: %s...", command.CStr());

			auto error = DispatchCommand(ws_session, command, object, info, message);

			if (error != nullptr)
			{
				if (error->GetCode() == 404)
				{
					logte("Cannot find stream [%s/%s]", info->vhost_app_name.CStr(), info->stream_name.CStr());
				}
				else
				{
					logte("An error occurred while dispatch command %s for stream [%s/%s]: %s, disconnecting...", command.CStr(), info->vhost_app_name.CStr(), info->stream_name.CStr(), error->What());
				}

				ov::JsonObject response_json;
				Json::Value &value = response_json.GetJsonValue();

				value["code"] = error->GetCode();
				value["error"] = error->GetMessage().CStr();

				ws_session->GetWebSocketResponse()->Send(response_json.ToString());

				return false;
			}

			return true;
		});

	interceptor->SetErrorHandler(
		[this](const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const std::shared_ptr<const ov::Error> &error) -> void {
			logtw("An error occurred: %s", error->What());
		});

	interceptor->SetCloseHandler(
		[this](const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, PhysicalPortDisconnectReason reason) -> void {
			auto connection = ws_session->GetConnection();
			auto request = ws_session->GetRequest();

			auto info = ws_session->GetExtraAs<RtcSignallingInfo>();

			if (info != nullptr)
			{
				if (info->id != P2P_INVALID_PEER_ID)
				{
					// The client is disconnected without send "close" command

					// Forces the session to be cleaned up by sending a stop command
					DispatchStop(ws_session, info);
				}

				logti("Client is disconnected: %s (%s / %s, ufrag: local: %s, remote: %s)",
					  ws_session->ToString().CStr(),
					  info->vhost_app_name.CStr(), info->stream_name.CStr(),
					  (info->offer_sdp != nullptr) ? info->offer_sdp->GetIceUfrag().CStr() : "(N/A)",
					  (info->peer_sdp != nullptr) ? info->peer_sdp->GetIceUfrag().CStr() : "(N/A)");
			}
			else
			{
				// The client is disconnected before websocket negotiation
			}
		});

	return true;
}

bool RtcSignallingServer::AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer)
{
	for (const auto &item : _observers)
	{
		if (item == observer)
		{
			logtw("%p is already observer of RtcSignallingServer", observer.get());
			return false;
		}
	}

	_observers.push_back(observer);

	return true;
}

bool RtcSignallingServer::RemoveObserver(const std::shared_ptr<RtcSignallingObserver> &observer)
{
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<RtcSignallingObserver> const &value) -> bool {
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

bool RtcSignallingServer::Disconnect(const info::VHostAppName &vhost_app_name, const ov::String &stream_name, const std::shared_ptr<const SessionDescription> &peer_sdp)
{
	bool disconnected = false;

	if ((disconnected == false) && (_http_server != nullptr))
	{
		disconnected = _http_server->DisconnectIf(
			[vhost_app_name, stream_name, peer_sdp](const std::shared_ptr<http::svr::HttpConnection> &connection) -> bool {
				
				if(connection->GetConnectionType() != http::ConnectionType::WebSocket)
				{
					return false;
				}

				auto websocket_session = connection->GetWebSocketSession();
				if (websocket_session == nullptr)
				{
					return false;
				}

				auto info = websocket_session->GetExtraAs<RtcSignallingInfo>();
				if (info == nullptr)
				{
					return false;
				}
				
				return (info->vhost_app_name == vhost_app_name) &&
						(info->stream_name == stream_name) &&
						((info->peer_sdp != nullptr) && (*(info->peer_sdp) == *peer_sdp));
				
			});
	}

	if ((disconnected == false) && (_https_server != nullptr))
	{
		disconnected = _https_server->DisconnectIf(
			[vhost_app_name, stream_name, peer_sdp](const std::shared_ptr<http::svr::HttpConnection> &connection) -> bool {
				
				if(connection->GetConnectionType() != http::ConnectionType::WebSocket)
				{
					return false;
				}
				
				auto websocket_session = connection->GetWebSocketSession();
				if (websocket_session == nullptr)
				{
					return false;
				}

				auto info = websocket_session->GetExtraAs<RtcSignallingInfo>();
				if (info == nullptr)
				{
					return false;
				}
				
				return (info->vhost_app_name == vhost_app_name) &&
						(info->stream_name == stream_name) &&
						((info->peer_sdp != nullptr) && (*(info->peer_sdp) == *peer_sdp));
			});
	}

	if (disconnected == false)
	{
		// May fail after http::svr::HttpServer::OnDisconnected() is handled 
		// because the WebSocket connection is disconnected at the timing immediately 
		// after Disconnect() is called due to ICE disconnection, 
		// but before _http_server->Disconnect() is executed
	}

	return disconnected;
}

int RtcSignallingServer::GetTotalPeerCount() const
{
	return _p2p_manager.GetPeerCount();
}

int RtcSignallingServer::GetClientPeerCount() const
{
	return _p2p_manager.GetClientPeerCount();
}

bool RtcSignallingServer::Stop()
{
	auto http_result = (_http_server != nullptr) ? _http_server->Stop() : true;
	auto https_result = (_https_server != nullptr) ? _https_server->Stop() : true;

	return http_result && https_result;
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchCommand(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const ov::String &command, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info, const std::shared_ptr<const ov::Data> &message)
{
	if (command == "request_offer")
	{
		return DispatchRequestOffer(ws_session, info);
	}

	if (info->id != object.GetInt64Value("id"))
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid ID");
	}
	else if (command == "answer")
	{
		return DispatchAnswer(ws_session, object, info);
	}
	else if (command == "candidate")
	{
		return DispatchCandidate(ws_session, object, info);
	}
	else if (command == "offer_p2p")
	{
		return DispatchOfferP2P(ws_session, object, info);
	}
	else if (command == "candidate_p2p")
	{
		return DispatchCandidateP2P(ws_session, object, info);
	}
	else if (command == "stop")
	{
		return DispatchStop(ws_session, info);
	}

	// Unknown command
	return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Unknown command: %s", command.CStr());
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchRequestOffer(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto request = ws_session->GetRequest();
	auto vhost_app_name = info->vhost_app_name;
	auto stream_name = info->stream_name;

	std::shared_ptr<const SessionDescription> sdp = nullptr;
	std::shared_ptr<const ov::Error> error = nullptr;

	std::shared_ptr<RtcPeerInfo> host_peer = nullptr;

	std::shared_ptr<RtcPeerInfo> peer_info = _p2p_manager.CreatePeerInfo(info->id, ws_session);

	if (peer_info == nullptr)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::InternalServerError, "Cannot parse peer info from user agent: %s", request->GetHeader("USER-AGENT").CStr());
	}

	info->peer_info = peer_info;

	if (_p2p_manager.IsEnabled())
	{
		logtd("Trying to find p2p host for client %s...", ws_session->ToString().CStr());

		if (info->peer_was_client == false)
		{
			if (info->peer_info != nullptr)
			{
				// Check if there is a host that can accept this client
				host_peer = _p2p_manager.TryToRegisterAsClientPeer(peer_info);
			}
			else
			{
				// If client is stopped or disconnected and DispatchStop is executed, it enters here
			}
		}
		else
		{
			// make the peer as host
		}
	}
	else
	{
		// P2P is disabled - All peers are host
	}

	if (host_peer == nullptr)
	{
		if (_p2p_manager.IsEnabled())
		{
			logtd("peer %s became a host peer because there is no p2p host for client %s.", peer_info->ToString().CStr(), ws_session->ToString().CStr());
		}

		bool tcp_relay = false;
		// None of the hosts can accept this client, so the peer will be connectioned to OME
		std::find_if(_observers.begin(), _observers.end(), [ws_session, info, &sdp, vhost_app_name, stream_name, &tcp_relay](auto &observer) -> bool {
			// Ask observer to fill local_candidates
			sdp = observer->OnRequestOffer(ws_session, vhost_app_name, info->host_name, stream_name, &(info->local_candidates), tcp_relay);
			return sdp != nullptr;
		});

		if (sdp != nullptr)
		{
			logtd("SDP is generated successfully");

			if (_p2p_manager.IsEnabled())
			{
				if (_p2p_manager.RegisterAsHostPeer(peer_info) == false)
				{
					OV_ASSERT2(false);
					return std::make_shared<http::HttpError>(http::StatusCode::InternalServerError, "Could not add peer as host");
				}
				else
				{
					// The peer is registered as host
				}
			}
			else
			{
				// P2P manager is disabled
			}

			ov::JsonObject response_json;

			Json::Value &value = response_json.GetJsonValue();

			// Create a "SDP" object into value
			Json::Value &sdp_value = value["sdp"];

			// Generate offer_sdp string from SessionDescription
			ov::String offer_sdp = sdp->ToString();
			if (offer_sdp.IsEmpty() == false)
			{
				value["command"] = "offer";
				value["id"] = info->id;
				value["peer_id"] = P2P_OME_PEER_ID;
				sdp_value["sdp"] = offer_sdp.CStr();
				sdp_value["type"] = "offer";

				// candidates: [ <candidate>, <candidate>, ... ]
				Json::Value candidates(Json::ValueType::arrayValue);

				// candiate:
				// {
				//     "candidate":"candidate:0 1 UDP 50 192.168.0.183 10000 typ host generation 0",
				//     "sdpMLineIndex":0,
				//     "sdpMid":"video"
				// }

				// Send local candidate list to client
				for (const auto &candidate : info->local_candidates)
				{
					Json::Value item;

					item["candidate"] = candidate.GetCandidateString().CStr();
					item["sdpMLineIndex"] = candidate.GetSdpMLineIndex();
					if (candidate.GetSdpMid().IsEmpty() == false)
					{
						item["sdpMid"] = candidate.GetSdpMid().CStr();
					}

					candidates.append(item);
				}
				value["candidates"] = candidates;
				value["code"] = static_cast<int>(http::StatusCode::OK);

				if (_tcp_force == true)
				{
					tcp_relay = true;
				}

				if (tcp_relay == true)
				{
					if (_ice_servers.isNull() == false)
					{
						// "ice_servers" is out of specification. This is a bug and "iceServers" is correct. "ice_servers" will be deprecated in the future.
						value["ice_servers"] = _ice_servers;
					}

					if (_new_ice_servers.isNull() == false)
					{
						value["iceServers"] = _new_ice_servers;
					}
				}

				info->offer_sdp = sdp;

				ws_session->GetWebSocketResponse()->Send(response_json.ToString());
			}
			else
			{
				logtw("Could not create SDP for stream %s", info->stream_name.CStr());
				error = std::make_shared<http::HttpError>(http::StatusCode::NotFound, "Cannot create offer");
			}
		}
		else
		{
			// cannot create offer
			error = std::make_shared<http::HttpError>(http::StatusCode::NotFound, "Cannot create offer");
		}
	}
	else
	{
		// Found a host that can accept this client

		info->peer_was_client = true;
		logtd("[Client -> Host] A host found for the client\n    Host: %s\n    Client: %s",
			  host_peer->ToString().CStr(),
			  peer_info->ToString().CStr());

		// Send 'request_offer_p2p' command to the host
		Json::Value value;

		value["command"] = "request_offer_p2p";
		value["id"] = host_peer->GetId();
		value["peer_id"] = peer_info->GetId();

		host_peer->GetSession()->GetWebSocketResponse()->Send(value);

		// Wait for 'offer_p2p' command from the host

		// TODO(dimiden): Timeout is required because host peer may not give offer for too long
	}

	return error;
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchAnswer(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &peer_info = info->peer_info;

	if (peer_info == nullptr)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Could not find peer id: %d", info->id);
	}

	const Json::Value &sdp_value = object.GetJsonValue("sdp");

	// Validate SDP
	if (sdp_value.isNull() || (sdp_value.isObject() == false))
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "There is no SDP");
	}

	const Json::Value &sdp_type = sdp_value["type"];

	if ((sdp_type.isString() == false) || (sdp_type != "answer"))
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid SDP type");
	}

	if (sdp_value["sdp"].isString() == false)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "SDP must be a string");
	}

	if ((_p2p_manager.IsEnabled() == false) || peer_info->IsHost())
	{
		logtd("[Host -> OME] The host peer sents a answer: %s", object.ToString().CStr());

		auto peer_sdp = std::make_shared<SessionDescription>();

		if (peer_sdp->FromString(sdp_value["sdp"].asCString()))
		{
			info->peer_sdp = peer_sdp;

			for (auto &observer : _observers)
			{
				logtd("Trying to callback OnAddRemoteDescription to %p (%s / %s)...", observer.get(), info->vhost_app_name.CStr(), info->stream_name.CStr());

				// TODO : Improved to return detailed error cause
				if (observer->OnAddRemoteDescription(ws_session, info->vhost_app_name, info->host_name, info->stream_name, info->offer_sdp, info->peer_sdp) == false)
				{
					return std::make_shared<http::HttpError>(http::StatusCode::Forbidden, "Forbidden");
				}
			}
		}
		else
		{
			return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Could not parse SDP");
		}
	}
	else
	{
		logtd("[Client -> Host] The client peer sents a answer: %s", object.ToString().CStr());

		// The client peer sents this answer
		auto peer_id = object.GetIntValue("peer_id");
		auto host_peer = peer_info->GetHostPeer();

		if (host_peer == nullptr)
		{
			OV_ASSERT2(false);
			return std::make_shared<http::HttpError>(http::StatusCode::InternalServerError, "Could not find host information");
		}

		if (host_peer->GetId() != peer_id)
		{
			return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid peer id: %d", peer_id);
		}

		Json::Value value;

		value["command"] = "answer_p2p";
		value["id"] = host_peer->GetId();
		value["peer_id"] = peer_info->GetId();
		value["sdp"] = sdp_value;

		host_peer->GetSession()->GetWebSocketResponse()->Send(value);
	}

	return nullptr;
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchCandidate(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	const Json::Value &candidates_value = object.GetJsonValue("candidates");

	if (candidates_value.isNull())
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "There is no candidate list");
	}

	if (candidates_value.isArray() == false)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Candidates must be array");
	}

	auto peer_id = object.GetIntValue("peer_id");
	auto peer_info = _p2p_manager.FindPeer(peer_id);

	if (peer_info == nullptr)
	{
		logtd("[Host -> OME] The host peer sents candidates: %s", object.ToString().CStr());

		for (const auto &candidate_iterator : candidates_value)
		{
			ov::String candidate = ov::Converter::ToString(candidate_iterator["candidate"]);

			if (candidate.IsEmpty())
			{
				// Even if the player does not send candidates, this does not affect the OME, so it changes the log level.
				logtd("[Host -> OME] The host peer sents an empty candidate");
				continue;
			}

			uint32_t sdp_m_line_index = ov::Converter::ToUInt32(candidate_iterator["sdpMLineIndex"]);
			ov::String sdp_mid = ov::Converter::ToString(candidate_iterator["sdpMid"]);
			ov::String username_fragment = ov::Converter::ToString(candidate_iterator["usernameFragment"]);

			auto ice_candidate = std::make_shared<RtcIceCandidate>(sdp_m_line_index, sdp_mid);

			if (ice_candidate->ParseFromString(candidate) == false)
			{
				return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid candidate: %s", candidate.CStr());
			}

			for (auto &observer : _observers)
			{
				observer->OnIceCandidate(ws_session, info->vhost_app_name, info->host_name, info->stream_name, ice_candidate, username_fragment);
			}
		}
	}
	else
	{
		logtd("[Client -> Host] The client peer sents candidates: %s", object.ToString().CStr());

		// Client -> (OME) -> Host
		Json::Value value;

		value["command"] = "candidate_p2p";
		value["id"] = peer_info->GetId();
		value["peer_id"] = info->id;
		value["candidates"] = candidates_value;

		peer_info->GetSession()->GetWebSocketResponse()->Send(value);
	}

	return nullptr;
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchOfferP2P(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &host = info->peer_info;

	if (host == nullptr)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::InternalServerError, "Peer %d does not exists", info->id);
	}

	auto peer_id = object.GetIntValue("peer_id");
	auto client_peer = _p2p_manager.GetClientPeerOf(host, peer_id);

	if (client_peer == nullptr)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid peer_id: %d", peer_id);
	}

	auto sdp_value = object.GetJsonValue("sdp");
	auto candidates = object.GetJsonValue("candidates");

	// Validate SDP
	if (sdp_value.isNull() || (sdp_value.isObject() == false))
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid SDP: %s", sdp_value.asCString());
	}

	const Json::Value &sdp_type = sdp_value["type"];

	if ((sdp_type.isString() == false) || (sdp_type != "offer"))
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid SDP type");
	}

	if (sdp_value["sdp"].isString() == false)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "SDP must be a string");
	}

	if ((candidates.isNull() == false) && (candidates.isArray() == false))
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Candidates must be array, but: %d", candidates.type());
	}

	logtd("[Host -> Client] The host peer sents an offer: %s", object.ToString().CStr());

	Json::Value value;

	value["command"] = "offer";
	value["id"] = client_peer->GetId();
	value["peer_id"] = host->GetId();
	value["sdp"] = sdp_value;
	if (candidates.isNull() == false)
	{
		value["candidates"] = candidates;
	}

	client_peer->GetSession()->GetWebSocketResponse()->Send(value);

	return nullptr;
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchCandidateP2P(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &host = info->peer_info;

	if (host == nullptr)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::InternalServerError, "Peer %d does not exists", info->id);
	}

	auto peer_id = object.GetIntValue("peer_id");
	auto client_peer = _p2p_manager.GetClientPeerOf(host, peer_id);

	if (client_peer == nullptr)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Invalid peer_id: %d", peer_id);
	}

	const Json::Value &candidates_value = object.GetJsonValue("candidates");

	if (candidates_value.isNull())
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "There is no candidate list");
	}

	if (candidates_value.isArray() == false)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::BadRequest, "Candidates must be array");
	}

	logtd("[Host -> Client] The host peer sents candidates: %s", object.ToString().CStr());

	Json::Value candidates;

	candidates["command"] = "candidate";
	candidates["id"] = client_peer->GetId();
	candidates["peer_id"] = info->id;
	candidates["candidates"] = candidates_value;

	logtd("[Host -> Client] JSON: %s", ov::Converter::ToString(candidates).CStr());

	client_peer->GetSession()->GetWebSocketResponse()->Send(candidates);

	return nullptr;
}

std::shared_ptr<const ov::Error> RtcSignallingServer::DispatchStop(const std::shared_ptr<http::svr::ws::WebSocketSession> &ws_session, std::shared_ptr<RtcSignallingInfo> &info)
{
	bool result = true;

	if (info->peer_sdp != nullptr)
	{
		for (auto &observer : _observers)
		{
			logtd("Trying to callback OnStopCommand to %p for client %d (%s / %s)...", observer.get(), info->id, info->vhost_app_name.CStr(), info->stream_name.CStr());

			if (observer->OnStopCommand(ws_session, info->vhost_app_name, info->host_name, info->stream_name, info->offer_sdp, info->peer_sdp) == false)
			{
				result = false;
			}
		}
	}

	{
		std::shared_ptr<RtcPeerInfo> peer_info = info->peer_info;

		info->peer_info = nullptr;

		if (info->id != P2P_INVALID_PEER_ID)
		{
			auto lock_guard = std::lock_guard(_client_list_mutex);

			_client_list.erase(info->id);
			info->id = P2P_INVALID_PEER_ID;
		}

		if (peer_info != nullptr)
		{
			logtd("Deleting a peer from p2p manager...: %s", peer_info->ToString().CStr());

			_p2p_manager.RemovePeer(peer_info);

			if (peer_info->IsHost())
			{
				logtd("[Host -> OME] The host peer is requested stop: %s", peer_info->ToString().CStr());

				// Broadcast to client peers
				auto client_list = _p2p_manager.GetClientPeerList(peer_info);

				for (auto &client : client_list)
				{
					auto &client_info = client.second;

					Json::Value value;

					value["command"] = "stop";
					value["id"] = client_info->GetId();
					value["peer_id"] = peer_info->GetId();

					client_info->GetSession()->GetWebSocketResponse()->Send(value);

					// remove client from peer
					_p2p_manager.RemovePeer(client_info);
				}
			}
			else
			{
				// Client peer -> OME
				logtd("[Client -> OME] The client peer is requested stop: %s", peer_info->ToString().CStr());

				// Send to host peer
				auto host_info = peer_info->GetHostPeer();

				if (host_info != nullptr)
				{
					Json::Value value;

					value["command"] = "stop";
					value["id"] = host_info->GetId();
					value["peer_id"] = peer_info->GetId();

					host_info->GetSession()->GetWebSocketResponse()->Send(value);
				}
				else
				{
					// The peer disconnected before dispatch request_offer
				}
			}
		}
		else
		{
			// It enters here when if the peer has never requested a request_offer or DispatchRequestOffer() is in progress on another thread
		}
	}

	if (result == false)
	{
		return std::make_shared<http::HttpError>(http::StatusCode::InternalServerError, "Cannot dispatch stop command");
	}

	return nullptr;
}
