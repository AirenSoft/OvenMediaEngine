//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "rtc_signalling_server.h"

#include <modules/ice/ice.h>
#include <publishers/webrtc/webrtc_publisher.h>

#include <utility>

#include "rtc_ice_candidate.h"
#include "rtc_signalling_server_private.h"

RtcSignallingServer::RtcSignallingServer(const cfg::Server &server_config)
	: _server_config(server_config),
	  _p2p_manager(server_config)
{
}

bool RtcSignallingServer::Start(const ov::SocketAddress *address, const ov::SocketAddress *tls_address)
{
	if ((_http_server != nullptr) || (_https_server != nullptr))
	{
		OV_ASSERT(false, "Server is already running (%p, %p)", _http_server.get(), _https_server.get());
		return false;
	}

	bool result = true;

	// Initialize HTTP server
	if (address != nullptr)
	{
		_http_server = std::make_shared<HttpServer>();
	}

	// Initialize HTTPS server
	if (tls_address != nullptr)
	{
		// TLS is enabled
		_https_server = std::make_shared<HttpsServer>();

		auto vhost_list = Orchestrator::GetInstance()->GetVirtualHostList();
		_https_server->SetVirtualHostList(vhost_list);
	}
	else
	{
		// TLS is disabled
	}

	result = result && InitializeWebSocketServer();

	result = result && ((_http_server == nullptr) || _http_server->Start(*address));
	result = result && ((_https_server == nullptr) || _https_server->Start(*tls_address));

	if (result == false)
	{
		// Rollback
		if (_http_server != nullptr)
		{
			_http_server->Stop();
			_http_server = nullptr;
		}

		if (_https_server != nullptr)
		{
			_https_server->Stop();
			_https_server = nullptr;
		}
	}

	return result;
}

bool RtcSignallingServer::InitializeWebSocketServer()
{
	auto web_socket = std::make_shared<WebSocketInterceptor>();

	web_socket->SetConnectionHandler(
		[this](const std::shared_ptr<WebSocketClient> &ws_client) -> HttpInterceptorResult {
			auto &client = ws_client->GetClient();
			auto request = client->GetRequest();
			auto response = client->GetResponse();
			auto remote = request->GetRemote();

			if (remote == nullptr)
			{
				OV_ASSERT(false, "Cannot find the client information: %s", ws_client->ToString().CStr());
				return HttpInterceptorResult::Disconnect;
			}

			ov::String description = remote->ToString();

			logti("New client is connected: %s", description.CStr());

			auto tokens = request->GetRequestTarget().Split("/");

			// "/<app>/<pub::Stream>"
			if (tokens.size() < 3)
			{
				logtw("Invalid request from %s. Disconnecting...", description.CStr());
				return HttpInterceptorResult::Disconnect;
			}

			// Find the "Host" header
			auto host_name = request->GetHeader("HOST").Split(":")[0];
			auto &app_name = tokens[1];
			auto &stream_name = tokens[2];

			info::VHostAppName vhost_app_name = Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, tokens[1]);

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

			request->SetExtra(info);

			return HttpInterceptorResult::Keep;
		});

	web_socket->SetMessageHandler(
		[this](const std::shared_ptr<WebSocketClient> &ws_client, const std::shared_ptr<const WebSocketFrame> &message) -> HttpInterceptorResult {
			auto &client = ws_client->GetClient();
			auto request = client->GetRequest();

			logtp("The client sent a message:\n%s", message->GetPayload()->Dump().CStr());

			auto info = request->GetExtraAs<RtcSignallingInfo>();

			if (info == nullptr)
			{
				// If you enter here, there is only the following situation:
				//
				// 1. An error occurred during the connection (request was wrong)
				// 2. After the connection is lost, the callback is called late
				logtw("Could not find client information: %s", ws_client->ToString().CStr());

				return HttpInterceptorResult::Disconnect;
			}

			ov::JsonObject object = ov::Json::Parse(message->GetPayload());

			if (object.IsNull())
			{
				logtw("Invalid request message from %s", ws_client->ToString().CStr());
				return HttpInterceptorResult::Disconnect;
			}

			// TODO(dimiden): 이렇게 호출하면 "command": null 이 추가되어버림. 개선 필요
			Json::Value &command_value = object.GetJsonValue()["command"];

			if (command_value.isNull())
			{
				logtw("Invalid request message from %s", ws_client->ToString().CStr());
				return HttpInterceptorResult::Disconnect;
			}

			ov::String command = ov::Converter::ToString(command_value);

			logtd("Trying to dispatch command: %s...", command.CStr());

			auto error = DispatchCommand(ws_client, command, object, info, message);

			if (error != nullptr)
			{
				if (error->GetCode() == 404)
				{
					logte("Cannot find stream [%s/%s]", info->vhost_app_name.CStr(), info->stream_name.CStr());
				}
				else
				{
					logte("An error occurred while dispatch command %s for stream [%s/%s]: %s, disconnecting...", command.CStr(), info->vhost_app_name.CStr(), info->stream_name.CStr(), error->ToString().CStr());
				}

				ov::JsonObject response_json;
				Json::Value &value = response_json.GetJsonValue();

				value["code"] = error->GetCode();
				value["error"] = error->GetMessage().CStr();

				ws_client->Send(response_json.ToString());

				return HttpInterceptorResult::Disconnect;
			}

			return HttpInterceptorResult::Keep;
		});

	web_socket->SetErrorHandler(
		[this](const std::shared_ptr<WebSocketClient> &ws_client, const std::shared_ptr<const ov::Error> &error) -> void {
			logtw("An error occurred: %s", error->ToString().CStr());
		});

	web_socket->SetCloseHandler(
		[this](const std::shared_ptr<WebSocketClient> &ws_client) -> void {
			auto &client = ws_client->GetClient();
			auto request = client->GetRequest();

			auto info = request->GetExtraAs<RtcSignallingInfo>();

			if (info != nullptr)
			{
				if (info->id != P2P_INVALID_PEER_ID)
				{
					// The client is disconnected without send "close" command

					// Forces the session to be cleaned up by sending a stop command
					DispatchStop(ws_client, info);
				}

				logti("Client is disconnected: %s (%s / %s, ufrag: local: %s, remote: %s)",
					  ws_client->ToString().CStr(),
					  info->vhost_app_name.CStr(), info->stream_name.CStr(),
					  (info->offer_sdp != nullptr) ? info->offer_sdp->GetIceUfrag().CStr() : "(N/A)",
					  (info->peer_sdp != nullptr) ? info->peer_sdp->GetIceUfrag().CStr() : "(N/A)");
			}
			else
			{
				// The client is disconnected before websocket negotiation
			}
		});

	bool result = true;

	result = result && ((_http_server == nullptr) || _http_server->AddInterceptor(web_socket));
	result = result && ((_https_server == nullptr) || _https_server->AddInterceptor(web_socket));

	return result;
}

bool RtcSignallingServer::AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer)
{
	// 기존에 등록된 observer가 있는지 확인
	for (const auto &item : _observers)
	{
		if (item == observer)
		{
			// 기존에 등록되어 있음
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
			[vhost_app_name, stream_name, peer_sdp](const std::shared_ptr<HttpClient> &client) -> bool {
				auto request = client->GetRequest();
				auto info = request->GetExtraAs<RtcSignallingInfo>();

				if (info == nullptr)
				{
					// Client disconnected while Connect() is being processed
				}
				else
				{
					return (info->vhost_app_name == vhost_app_name) &&
						   (info->stream_name == stream_name) &&
						   ((info->peer_sdp != nullptr) && (*(info->peer_sdp) == *peer_sdp));
				}

				return false;
			});
	}

	if ((disconnected == false) && (_https_server != nullptr))
	{
		disconnected = _https_server->DisconnectIf(
			[vhost_app_name, stream_name, peer_sdp](const std::shared_ptr<HttpClient> &client) -> bool {
				auto request = client->GetRequest();
				auto info = request->GetExtraAs<RtcSignallingInfo>();

				if (info == nullptr)
				{
					// Client disconnected while Connect() is being processed
				}
				else
				{
					return (info->vhost_app_name == vhost_app_name) &&
						   (info->stream_name == stream_name) &&
						   ((info->peer_sdp != nullptr) && (*(info->peer_sdp) == *peer_sdp));
				}

				return false;
			});
	}

	if (disconnected == false)
	{
		// ICE 연결이 끊어져 Disconnect()이 호출 된 직후, _http_server->Disconnect()이 실행되기 전 타이밍에
		// WebSocket 연결이 끊어져서 HttpServer::OnDisconnected() 이 처리되고 나면 실패 할 수 있음
	}

	return disconnected;
}

//====================================================================================================
// monitoring data pure virtual function
// - collections vector must be insert processed
//====================================================================================================
bool RtcSignallingServer::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &stream_collections)
{
	// TODO(Getroot): Need to refactoring
	/*
	std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();

	ov::String alias = _host_info.GetOrigin().GetAlias();
	ov::String app_name = _application_info->GetName();
	ov::String stream_name;
	uint32_t bitrate = 0;

	// TODO : 임시 코드 차후에 p2p manager에서 실제 정보 처리
	// - 1개의 스트림명과 비트레이트  확인
	std::lock_guard<std::mutex> lock_guard(_client_list_mutex);

	for (const auto &client_item : _client_list)
	{
		stream_name = client_item.second->stream_name;

		std::find_if(_observers.begin(), _observers.end(), [&bitrate, app_name, stream_name](auto &observer) -> bool {
			// Ask observer to fill bitrate
			bitrate = observer->OnGetBitrate(app_name, stream_name);
			return bitrate != 0;
		});

		if (bitrate != 0)
		{
			break;
		}
	}

	uint32_t p2p_connection_count = GetClientPeerCount();
	uint32_t edeg_connection_count = GetTotalPeerCount() - p2p_connection_count;

	// p2p
	for (uint32_t index = 0; index < p2p_connection_count; index++)
	{
		auto collection = std::make_shared<pub::MonitoringCollectionData>(MonitroingCollectionType::Stream,
																	 alias,
																	 app_name,
																	 stream_name);
		collection->edge_connection = 0;
		collection->edge_bitrate = 0;
		collection->p2p_connection = 1;
		collection->p2p_bitrate = bitrate;
		collection->check_time = current_time;

		stream_collections.push_back(collection);
	}

	// edge connection
	for (uint32_t index = 0; index < edeg_connection_count; index++)
	{
		auto collection = std::make_shared<pub::MonitoringCollectionData>(MonitroingCollectionType::Stream,
																	 alias,
																	 app_name,
																	 stream_name);
		collection->edge_connection = 1;
		collection->edge_bitrate = bitrate;
		collection->p2p_connection = 0;
		collection->p2p_bitrate = 0;
		collection->check_time = current_time;

		stream_collections.push_back(collection);
	}
	*/

	return true;
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

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchCommand(const std::shared_ptr<WebSocketClient> &ws_client, const ov::String &command, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info, const std::shared_ptr<const WebSocketFrame> &message)
{
	if (command == "request_offer")
	{
		return DispatchRequestOffer(ws_client, info);
	}

	if (info->id != object.GetInt64Value("id"))
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid ID");
	}
	else if (command == "answer")
	{
		return DispatchAnswer(ws_client, object, info);
	}
	else if (command == "candidate")
	{
		return DispatchCandidate(ws_client, object, info);
	}
	else if (command == "offer_p2p")
	{
		return DispatchOfferP2P(ws_client, object, info);
	}
	else if (command == "candidate_p2p")
	{
		return DispatchCandidateP2P(ws_client, object, info);
	}
	else if (command == "stop")
	{
		return DispatchStop(ws_client, info);
	}

	// Unknown command
	return ov::Error::CreateError(HttpStatusCode::BadRequest, "Unknown command: %s", command.CStr());
}

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &client = ws_client->GetClient();
	auto request = client->GetRequest();

	auto vhost_app_name = info->vhost_app_name;
	auto stream_name = info->stream_name;

	std::shared_ptr<const SessionDescription> sdp = nullptr;
	std::shared_ptr<ov::Error> error = nullptr;

	std::shared_ptr<RtcPeerInfo> host_peer = nullptr;

	std::shared_ptr<RtcPeerInfo> peer_info = _p2p_manager.CreatePeerInfo(info->id, ws_client);

	if (peer_info == nullptr)
	{
		return ov::Error::CreateError(HttpStatusCode::InternalServerError, "Cannot parse peer info from user agent: %s", request->GetHeader("USER-AGENT").CStr());
	}

	info->peer_info = peer_info;

	if (_p2p_manager.IsEnabled())
	{
		logtd("Trying to find p2p host for client %s...", ws_client->ToString().CStr());

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
			logtd("peer %s became a host peer because there is no p2p host for client %s.", peer_info->ToString().CStr(), ws_client->ToString().CStr());
		}

		// None of the hosts can accept this client, so the peer will be connectioned to OME
		std::find_if(_observers.begin(), _observers.end(), [ws_client, info, &sdp, vhost_app_name, stream_name](auto &observer) -> bool {
			// Ask observer to fill local_candidates
			sdp = observer->OnRequestOffer(ws_client, vhost_app_name, info->host_name, info->app_name, stream_name, &(info->local_candidates));
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
					return ov::Error::CreateError(HttpStatusCode::InternalServerError, "Could not add peer as host");
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
				value["code"] = static_cast<int>(HttpStatusCode::OK);

				info->offer_sdp = sdp;

				ws_client->Send(response_json.ToString());
			}
			else
			{
				logtw("Could not create SDP for stream %s", info->stream_name.CStr());
				error = ov::Error::CreateError(HttpStatusCode::NotFound, "Cannot create offer");
			}
		}
		else
		{
			// cannot create offer
			error = ov::Error::CreateError(HttpStatusCode::NotFound, "Cannot create offer");
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

		host_peer->GetResponse()->Send(value);

		// Wait for 'offer_p2p' command from the host

		// TODO(dimiden): Timeout is required because host peer may not give offer for too long
	}

	return error;
}

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchAnswer(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &peer_info = info->peer_info;

	if (peer_info == nullptr)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not find peer id: %d", info->id);
	}

	const Json::Value &sdp_value = object.GetJsonValue("sdp");

	// Validate SDP
	if (sdp_value.isNull() || (sdp_value.isObject() == false))
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "There is no SDP");
	}

	const Json::Value &sdp_type = sdp_value["type"];

	if ((sdp_type.isString() == false) || (sdp_type != "answer"))
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid SDP type");
	}

	if (sdp_value["sdp"].isString() == false)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "SDP must be a string");
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
				// TODO(Getroot): Add param "request->GetRequestTarget()"
				observer->OnAddRemoteDescription(ws_client, info->vhost_app_name, info->host_name, info->app_name, info->stream_name, info->offer_sdp, info->peer_sdp);
			}
		}
		else
		{
			return ov::Error::CreateError(HttpStatusCode::BadRequest, "Could not parse SDP");
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
			return ov::Error::CreateError(HttpStatusCode::InternalServerError, "Could not find host information");
		}

		if (host_peer->GetId() != peer_id)
		{
			return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid peer id: %d", peer_id);
		}

		Json::Value value;

		value["command"] = "answer_p2p";
		value["id"] = host_peer->GetId();
		value["peer_id"] = peer_info->GetId();
		value["sdp"] = sdp_value;

		host_peer->GetResponse()->Send(value);
	}

	return nullptr;
}

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchCandidate(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	const Json::Value &candidates_value = object.GetJsonValue("candidates");

	if (candidates_value.isNull())
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "There is no candidate list");
	}

	if (candidates_value.isArray() == false)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Candidates must be array");
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
				logtw("[Host -> OME] The host peer sents an empty candidate");
				continue;
			}

			uint32_t sdp_m_line_index = ov::Converter::ToUInt32(candidate_iterator["sdpMLineIndex"]);
			ov::String sdp_mid = ov::Converter::ToString(candidate_iterator["sdpMid"]);
			ov::String username_fragment = ov::Converter::ToString(candidate_iterator["usernameFragment"]);

			auto ice_candidate = std::make_shared<RtcIceCandidate>(sdp_m_line_index, sdp_mid);

			if (ice_candidate->ParseFromString(candidate) == false)
			{
				return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid candidate: %s", candidate.CStr());
			}

			for (auto &observer : _observers)
			{
				observer->OnIceCandidate(ws_client, info->vhost_app_name, info->host_name, info->app_name, info->stream_name, ice_candidate, username_fragment);
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

		peer_info->GetResponse()->Send(value);
	}

	return nullptr;
}

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchOfferP2P(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &host = info->peer_info;

	if (host == nullptr)
	{
		return ov::Error::CreateError(HttpStatusCode::InternalServerError, "Peer %d does not exists", info->id);
	}

	auto peer_id = object.GetIntValue("peer_id");
	auto client_peer = _p2p_manager.GetClientPeerOf(host, peer_id);

	if (client_peer == nullptr)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid peer_id: %d", peer_id);
	}

	auto sdp_value = object.GetJsonValue("sdp");
	auto candidates = object.GetJsonValue("candidates");

	// Validate SDP
	if (sdp_value.isNull() || (sdp_value.isObject() == false))
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid SDP: %s", sdp_value.asCString());
	}

	const Json::Value &sdp_type = sdp_value["type"];

	if ((sdp_type.isString() == false) || (sdp_type != "offer"))
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid SDP type");
	}

	if (sdp_value["sdp"].isString() == false)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "SDP must be a string");
	}

	if ((candidates.isNull() == false) && (candidates.isArray() == false))
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Candidates must be array, but: %d", candidates.type());
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

	client_peer->GetResponse()->Send(value);

	return nullptr;
}

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchCandidateP2P(const std::shared_ptr<WebSocketClient> &ws_client, const ov::JsonObject &object, std::shared_ptr<RtcSignallingInfo> &info)
{
	auto &host = info->peer_info;

	if (host == nullptr)
	{
		return ov::Error::CreateError(HttpStatusCode::InternalServerError, "Peer %d does not exists", info->id);
	}

	auto peer_id = object.GetIntValue("peer_id");
	auto client_peer = _p2p_manager.GetClientPeerOf(host, peer_id);

	if (client_peer == nullptr)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Invalid peer_id: %d", peer_id);
	}

	const Json::Value &candidates_value = object.GetJsonValue("candidates");

	if (candidates_value.isNull())
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "There is no candidate list");
	}

	if (candidates_value.isArray() == false)
	{
		return ov::Error::CreateError(HttpStatusCode::BadRequest, "Candidates must be array");
	}

	logtd("[Host -> Client] The host peer sents candidates: %s", object.ToString().CStr());

	Json::Value candidates;

	candidates["command"] = "candidate";
	candidates["id"] = client_peer->GetId();
	candidates["peer_id"] = info->id;
	candidates["candidates"] = candidates_value;

	logtd("[Host -> Client] JSON: %s", ov::Converter::ToString(candidates).CStr());

	client_peer->GetResponse()->Send(candidates);

	return nullptr;
}

std::shared_ptr<ov::Error> RtcSignallingServer::DispatchStop(const std::shared_ptr<WebSocketClient> &ws_client, std::shared_ptr<RtcSignallingInfo> &info)
{
	bool result = true;

	if (info->peer_sdp != nullptr)
	{
		for (auto &observer : _observers)
		{
			logtd("Trying to callback OnStopCommand to %p for client %d (%s / %s)...", observer.get(), info->id, info->vhost_app_name.CStr(), info->stream_name.CStr());

			if (observer->OnStopCommand(ws_client, info->vhost_app_name, info->host_name, info->app_name, info->stream_name, info->offer_sdp, info->peer_sdp) == false)
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

					client_info->GetResponse()->Send(value);

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

					host_info->GetResponse()->Send(value);
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
		return ov::Error::CreateError(HttpStatusCode::InternalServerError, "Cannot dispatch stop command");
	}

	return nullptr;
}
