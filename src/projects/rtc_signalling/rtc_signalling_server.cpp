//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtc_signalling_server.h"
#include "rtc_ice_candidate.h"
#include "rtc_signalling_server_private.h"

#include <ice/ice.h>

RtcSignallingServer::RtcSignallingServer(const info::Application &application_info, std::shared_ptr<MediaRouteApplicationInterface> application)
	: _application_info(application_info),
	  _application(application)
{
	_sdp_timer.Start();
}

bool RtcSignallingServer::Start(const ov::SocketAddress &address, const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate)
{
	if(_http_server != nullptr)
	{
		OV_ASSERT(false, "Server is already running");
		return false;
	}

	if(certificate != nullptr)
	{
		auto https_server = std::make_shared<HttpsServer>();

		https_server->SetLocalCertificate(certificate);
		https_server->SetChainCertificate(chain_certificate);

		_http_server = https_server;
	}
	else
	{
		_http_server = std::make_shared<HttpServer>();
	}

	auto web_socket = std::make_shared<WebSocketInterceptor>();

	web_socket->SetConnectionHandler(
		[this](const std::shared_ptr<WebSocketResponse> &response) -> void
		{
			logti("New client is connected");

			auto tokens = response->GetRequest()->GetUri().Split("/");

			// "/<app>/<stream>"
			if(tokens.size() < 3)
			{
				// 잘못된 요청
				response->Close();
				return;
			}

			_client_list[response] = (RtcSignallingInfo){
				.application_name = tokens[1],
				.stream_name = tokens[2],

				// TODO: 새로운 ID 생성 (UUID 등)
				.id = ov::String::FormatString("%d", rand()),
				.offer_sdp = nullptr,
				.peer_sdp = nullptr,
				.local_candidates = std::vector<RtcIceCandidate>(),
				.remote_candidates = std::vector<RtcIceCandidate>()
			};
		});

	web_socket->SetMessageHandler(
		[this](const std::shared_ptr<WebSocketResponse> &response, const std::shared_ptr<const WebSocketFrame> &message) -> void
		{
			logtd("The client sent a message:\n%s", message->GetPayload()->Dump().CStr());

			auto client = _client_list.find(response);

			if(client == _client_list.end())
			{
				OV_ASSERT2(false);
				logtw("Could not find client: %s", response->ToString().CStr());
				response->Close();
				return;
			}

			ov::JsonObject object = ov::Json::Parse(message->GetPayload());

			if(object.IsNull())
			{
				logtw("Invalid request message from %s", response->ToString().CStr());
				response->Close();
				return;
			}

			// TODO(dimiden): 이렇게 호출하면 "command": null 이 추가되어버림. 개선 필요
			Json::Value &command_value = object.GetJsonValue()["command"];

			if(command_value.isNull())
			{
				logtw("Invalid request message from %s", response->ToString().CStr());
				response->Close();
				return;
			}

			ov::String command = ov::Converter::ToString(command_value);

			logtd("Trying to process command: %s...", command.CStr());

			ProcessCommand(command, object, client->second, response, message);
		});

	web_socket->SetErrorHandler(
		[this](const std::shared_ptr<WebSocketResponse> &response, const std::shared_ptr<const ov::Error> &error) -> void
		{
			logtw("An error occurred: %s", error->ToString().CStr());
		});

	web_socket->SetCloseHandler(
		[this](const std::shared_ptr<WebSocketResponse> &response) -> void
		{
			auto client = _client_list.find(response);

			if(client == _client_list.end())
			{
				OV_ASSERT2(false);
				logtw("Could not find client: %s", response->ToString().CStr());
				return;
			}

			auto &info = client->second;

			logti("Client is disconnected: %s (%s / %s, ufrag: local: %s, remote: %s)",
			      response->ToString().CStr(),
			      info.application_name.CStr(), info.stream_name.CStr(),
			      (info.offer_sdp != nullptr) ? info.offer_sdp->GetIceUfrag().CStr() : "(N/A)",
			      (info.peer_sdp != nullptr) ? info.peer_sdp->GetIceUfrag().CStr() : "(N/A)"
			);

			_client_list.erase(client);
		});

	_http_server->AddInterceptor(web_socket);

	return _http_server->Start(address);
}

bool RtcSignallingServer::AddObserver(const std::shared_ptr<RtcSignallingObserver> &observer)
{
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

bool RtcSignallingServer::RemoveObserver(const std::shared_ptr<RtcSignallingObserver> &observer)
{
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<RtcSignallingObserver> const &value) -> bool
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

bool RtcSignallingServer::Disconnect(const ov::String &application_name, const ov::String &stream_name, const std::shared_ptr<SessionDescription> &peer_sdp)
{
	std::map<std::shared_ptr<WebSocketResponse>, RtcSignallingInfo>::iterator item;

	for(item = _client_list.begin(); item != _client_list.end(); ++item)
	{
		RtcSignallingInfo &info = item->second;

		if(
			(info.application_name == application_name) &&
			(info.stream_name == stream_name) &&
			((info.peer_sdp != nullptr) && (*(info.peer_sdp) == *peer_sdp))
			)
		{
			break;
		}
	}

	if(item == _client_list.end())
	{
		// 기존에 등록되어 있지 않음
		logtw("Cannot find SDP for session id: %d", peer_sdp->GetSessionId());

		return false;
	}

	item->first->Close();

	_client_list.erase(item);

	return true;
}

bool RtcSignallingServer::Stop()
{
	_sdp_timer.Stop();

	if(_http_server == nullptr)
	{
		return false;
	}

	if(_http_server->Stop())
	{
		_http_server = nullptr;

		return true;
	}

	return false;
}

void RtcSignallingServer::ProcessCommand(const ov::String &command, const ov::JsonObject &object, RtcSignallingInfo &info, const std::shared_ptr<WebSocketResponse> &response, const std::shared_ptr<const WebSocketFrame> &message)
{
	auto response_error = [&](std::shared_ptr<ov::Error> error) -> void
	{
		ov::JsonObject response_json;
		Json::Value &value = response_json.GetJsonValue();

		value["error"] = error->GetMessage().CStr();

		response->Send(response_json.ToString());
	};

	if(command == "request_offer")
	{
		ProcessRequestOffer(info, response, message, [&, response_error](std::shared_ptr<SessionDescription> sdp, std::shared_ptr<ov::Error> error) -> void
		{
			if((sdp != nullptr) && (error == nullptr))
			{
				ov::JsonObject response_json;

				Json::Value &value = response_json.GetJsonValue();

				// SDP를 계산함
				Json::Value &sdp_value = value["sdp"];

				ov::String offer_sdp;

				if(sdp->ToString(offer_sdp))
				{
					value["id"] = info.id.CStr();
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
					// local candidate 목록을 client로 보냄
					for(const auto &candidate : info.local_candidates)
					{
						Json::Value item;

						item["candidate"] = candidate.GetCandidateString().CStr();
						item["sdpMLineIndex"] = candidate.GetSdpMLineIndex();
						if(candidate.GetSdpMid().IsEmpty() == false)
						{
							item["sdpMid"] = candidate.GetSdpMid().CStr();
						}

						candidates.append(item);
					}
					value["candidates"] = candidates;

					info.offer_sdp = sdp;

					response->Send(response_json.ToString());
				}
				else
				{
					//TODO(dimiden): ERROR 처리 할 것
					logtw("Could not obtain SDP for stream %s", info.stream_name.CStr());

					OV_ASSERT2(false);

					error = std::make_shared<ov::Error>(100, "Cannot create offer");
				}
			}

			if(error != nullptr)
			{
				response_error(error);
			}
		});
	}
	else if(command == "answer")
	{
		// sdp가 있는지 확인
		const Json::Value &sdp_value = object.GetJsonValue("sdp");

		if(sdp_value.isNull())
		{
			response_error(std::make_shared<ov::Error>(101, "There is no SDP"));
		}

		// client가 SDP를 보냈으므로, 처리함
		const Json::Value &sdp_type = sdp_value["type"];

		if((sdp_type.isString() == false) || (sdp_type != "answer"))
		{
			// 반드시 answer sdp여야 함
			response_error(std::make_shared<ov::Error>(102, "Invalid SDP type"));
		}

		if(sdp_value["sdp"].isString() == false)
		{
			// 반드시 문자열 이어야 함
			response_error(std::make_shared<ov::Error>(103, "SDP must be string"));
		}

		logtd("The client sent a answer: %s", object.ToString().CStr());

		info.peer_sdp = std::make_shared<SessionDescription>();

		if(info.peer_sdp->FromString(sdp_value["sdp"].asCString()))
		{
			for(auto &observer : _observers)
			{
				logtd("Trying to callback OnAddRemoteDescription to %p (%s / %s)...", observer.get(), info.application_name.CStr(), info.stream_name.CStr());

				observer->OnAddRemoteDescription(info.application_name, info.stream_name, info.offer_sdp, info.peer_sdp);
			};
		}
		else
		{
			// SDP를 파싱할 수 없음
			response_error(std::make_shared<ov::Error>(104, "Could not parse SDP"));
		}
	}
	else if(command == "candidate")
	{
		// candidates가 있는지 확인
		const Json::Value &candidates_value = object.GetJsonValue("candidates");

		if(candidates_value.isNull())
		{
			response_error(std::make_shared<ov::Error>(104, "There is no candidate list"));
		}

		if(candidates_value.isArray() == false)
		{
			response_error(std::make_shared<ov::Error>(105, "Candidates must be array"));
		}

		// client가 candidate를 보냈으므로, 처리함
		for(auto candidate_iterator = candidates_value.begin(); candidate_iterator != candidates_value.end(); ++candidate_iterator)
		{
			ov::String candidate = ov::Converter::ToString((*candidate_iterator)["candidate"]);
			uint32_t sdp_m_line_index = ov::Converter::ToUInt32((*candidate_iterator)["sdpMLineIndex"]);
			ov::String sdp_mid = ov::Converter::ToString((*candidate_iterator)["sdpMid"]);
			ov::String username_fragment = ov::Converter::ToString((*candidate_iterator)["usernameFragment"]);

			auto ice_candidate = std::make_shared<RtcIceCandidate>(sdp_m_line_index, sdp_mid);

			if(ice_candidate->ParseFromString(candidate) == false)
			{
				response_error(std::make_shared<ov::Error>(106, "Invalid candidate: %s", candidate.CStr()));
			}

			for(auto &observer : _observers)
			{
				observer->OnIceCandidate(info.application_name, info.stream_name, ice_candidate, username_fragment);
			};
		}
	}
	else if(command == "stop")
	{
		if(info.peer_sdp != nullptr)
		{
			for(auto &observer : _observers)
			{
				logtd("Trying to callback OnStopCommand to %p (%s / %s)...", observer.get(), info.application_name.CStr(), info.stream_name.CStr());

				observer->OnStopCommand(info.application_name, info.stream_name, info.offer_sdp, info.peer_sdp);
			};
		}
	}
}

void RtcSignallingServer::ProcessRequestOffer(RtcSignallingInfo &info, const std::shared_ptr<WebSocketResponse> &response, const std::shared_ptr<const WebSocketFrame> &message, SdpCallback callback)
{
	ov::String application_name = info.application_name;
	ov::String stream_name = info.stream_name;

	// 여기에 request offer가 있음.
	std::shared_ptr<SessionDescription> sdp = nullptr;

	std::find_if(_observers.begin(), _observers.end(), [&info, &sdp, &application_name, &stream_name](auto &observer) -> bool
	{
		// observer에 local_candidates를 채워달라고 요청함
		sdp = observer->OnRequestOffer(application_name, stream_name, &(info.local_candidates));
		return sdp != nullptr;
	});

	if(sdp != nullptr)
	{
		callback(sdp, nullptr);
	}
	else
	{
		if(_application_info.GetType() == cfg::ApplicationType::LiveEdge)
		{
			// Request a SDP to origin
			auto origin = _application->GetOriginConnector();

			// TODO: need to change asyncronous without timer
			if(origin != nullptr)
			{
				ov::String identifier = ov::String::FormatString("%s/%s", application_name.CStr(), stream_name.CStr());
				int count = 0;

				_sdp_timer.Push(
					[&, count, sdp, application_name, stream_name, response, callback](void *paramter) -> bool
					{
						std::shared_ptr<SessionDescription> sdp = nullptr;

						// check again
						std::find_if(_observers.begin(), _observers.end(), [&info, &sdp, &application_name, &stream_name](auto &observer) -> bool
						{
							// observer에 local_candidates를 채워달라고 요청함
							sdp = observer->OnRequestOffer(application_name, stream_name, &(info.local_candidates));
							return sdp != nullptr;
						});

						if(sdp != nullptr)
						{
							callback(sdp, nullptr);

							// Stop the timer
							return false;
						}

						if((time(nullptr) - reinterpret_cast<time_t>(paramter)) > 5)
						{
							// timeout
							callback(nullptr, std::make_shared<ov::Error>(100, "Cannot create offer"));

							// Stop the timer
							return false;
						}

						// wait for response
						return true;
					}, reinterpret_cast<void *>(time(nullptr)), 100, true);

			}
			else
			{
				OV_ASSERT2(origin != nullptr);
			}
		}
		else
		{
			logte("Get offer sdp failed. Cannot find stream (%s/%s)", application_name.CStr(), stream_name.CStr());
			logtw("Could not obtain sdp for client: %s", response->ToString().CStr());

			callback(sdp, std::make_shared<ov::Error>(100, "Cannot create offer"));
		}
	}
}
