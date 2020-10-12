#include <base/ovlibrary/url.h>
#include "ovt_private.h"
#include "ovt_publisher.h"
#include "ovt_session.h"

std::shared_ptr<OvtPublisher> OvtPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto obj = std::make_shared<OvtPublisher>(server_config, router);

	if (!obj->Start())
	{
		logte("An error occurred while creating OvtPublisher");
		return nullptr;
	}

	return obj;
}

OvtPublisher::OvtPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(server_config, router)
{

}

OvtPublisher::~OvtPublisher()
{
	logtd("OvtPublisher has been terminated finally");
}

bool OvtPublisher::Start()
{
	// Listen to localhost:<relay_port>
	auto server_config = GetServerConfig();

	const auto &origin = server_config.GetBind().GetPublishers().GetOvt();

	if (origin.IsParsed())
	{
		auto &port_config = origin.GetPort();
		int port = port_config.GetPort();

		if (port > 0)
		{
			const ov::String &ip = server_config.GetIp();
			ov::SocketAddress address = ov::SocketAddress(ip.IsEmpty() ? nullptr : ip.CStr(), static_cast<uint16_t>(port));

			_server_port = PhysicalPortManager::Instance()->CreatePort(port_config.GetSocketType(), address);
			if (_server_port != nullptr)
			{
				logti("%s is listening on %s", GetPublisherName(), address.ToString().CStr());
				_server_port->AddObserver(this);
			}
			else
			{
				logte("Could not create relay port. Origin features will not work.");
			}
		}
		else
		{
			logte("Invalid ovt port: %d", port);
		}
	}
	else
	{
		// Relay feature is disabled
	}


	return Publisher::Start();
}

bool OvtPublisher::Stop()
{
	if (_server_port != nullptr)
	{
		_server_port->RemoveObserver(this);
		_server_port->Close();
	}

	return Publisher::Stop();
}

std::shared_ptr<pub::Application> OvtPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return OvtApplication::Create(OvtPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool OvtPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}

void OvtPublisher::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	// NOTHING
	logti("OvtProvider is connected : %s", remote->ToString().CStr());
}

void OvtPublisher::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
									const ov::SocketAddress &address,
									const std::shared_ptr<const ov::Data> &data)
{
	auto packet = std::make_shared<OvtPacket>(*data);
	if(!packet->IsPacketAvailable())
	{
		// If packet is not valid, it is not necessary to response
		logte("Invalid packet received and ignored");
		return;
	}

	// Parsing Payload
	ov::String payload((const char *)packet->Payload(), packet->PayloadLength());
	ov::JsonObject object = ov::Json::Parse(payload);

	if(object.IsNull())
	{
		ResponseResult(remote, OVT_PAYLOAD_TYPE_ERROR, packet->SessionId(), 0, 404, "An invalid request : Json format");
		return;
	}

	Json::Value &json_request_id = object.GetJsonValue()["id"];
	Json::Value &json_request_url = object.GetJsonValue()["url"];

	if(json_request_id.isNull() || json_request_url.isNull() || !json_request_id.isUInt())
	{
		ResponseResult(remote, OVT_PAYLOAD_TYPE_ERROR, packet->SessionId(), 0, 404, "An invalid request : Id or Url are not valid");
		return;
	}

	uint32_t request_id = json_request_id.asUInt();
	auto url = ov::Url::Parse(json_request_url.asString());
	if(url == nullptr)
	{
		ResponseResult(remote, OVT_PAYLOAD_TYPE_ERROR, packet->SessionId(), json_request_id.asUInt(), 404, "An invalid request : Url is not valid");
		return;
	}

	switch(packet->PayloadType())
	{
		case OVT_PAYLOAD_TYPE_DESCRIBE:
			// Add session
			HandleDescribeRequest(remote, request_id, url);
			break;
		case OVT_PAYLOAD_TYPE_PLAY:
			HandlePlayRequest(remote, request_id, url);
			break;
		case OVT_PAYLOAD_TYPE_STOP:
			// Remove session
			HandleStopRequest(remote, packet->SessionId(), request_id, url);
			break;
		default:
			// Response error message and disconnect
			ResponseResult(remote, OVT_PAYLOAD_TYPE_ERROR, packet->SessionId(), request_id, 404, "An invalid request");
			break;
	}
}

// It it only called when the OVT runs over TCP or SRT

// TODO(Getroot): If the Ovt uses UDP, OME cannot know that the connection was forcibly terminated.(Ungraceful termination)
// In this case, OME should add PING/PONG function to check if the connection is broken.
// However, this version of OVT does not need to be considered because it does not use UDP but only tcp or srt.
// If the OVT is extended to use UDP in the future, then the protocol needs to be advanced.

void OvtPublisher::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
									PhysicalPortDisconnectReason reason,
									const std::shared_ptr<const ov::Error> &error)
{
	// disconnect means when the stream disconnects itself.
	if(reason != PhysicalPortDisconnectReason::Disconnect)
	{
		auto streams = _remote_stream_map.equal_range(remote->GetId());
		for(auto it = streams.first; it != streams.second; ++it)
		{
			auto stream = it->second;
			stream->RemoveSessionByConnectorId(remote->GetId());
		}
	}
	UnlinkRemoteFromStream(remote->GetId());
}

void OvtPublisher::HandleDescribeRequest(const std::shared_ptr<ov::Socket> &remote, const uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto orchestrator = Orchestrator::GetInstance();
	auto host_name = url->Domain();
	auto app_name = url->App();
	auto vhost_app_name = Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(host_name, app_name);
	auto stream_name = url->Stream();
	ov::String msg;

	auto stream = std::static_pointer_cast<OvtStream>(GetStream(vhost_app_name, stream_name));
	if (stream == nullptr)
	{
		// If the stream does not exists, request to the provider
		if (orchestrator->RequestPullStream(vhost_app_name, host_name, app_name, stream_name) == false)
		{
			msg.Format("There is no such stream (%s/%s)", vhost_app_name.CStr(), url->Stream().CStr());
			ResponseResult(remote, OVT_PAYLOAD_TYPE_DESCRIBE, 0, request_id, 404, msg);
			return;
		}
		else
		{
			stream = std::static_pointer_cast<OvtStream>(GetStream(vhost_app_name, stream_name));
			if (stream == nullptr)
			{
				msg.Format("Could not pull the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
				ResponseResult(remote, OVT_PAYLOAD_TYPE_DESCRIBE, 0, request_id, 404, msg);
				return;
			}
		}
	}

	Json::Value description = stream->GetDescription();
	ResponseResult(remote, OVT_PAYLOAD_TYPE_DESCRIBE, 0, request_id, 200, "ok", "stream", description);
}

void OvtPublisher::HandlePlayRequest(const std::shared_ptr<ov::Socket> &remote, uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto vhost_app_name = Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(url->Domain(), url->App());
	
	auto app = std::static_pointer_cast<OvtApplication>(GetApplicationByName(vhost_app_name));
	if(app == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such app (%s)", vhost_app_name.CStr());
		ResponseResult(remote, OVT_PAYLOAD_TYPE_PLAY, 0, request_id, 404, msg);
		return;
	}

	auto stream = std::static_pointer_cast<OvtStream>(app->GetStream(url->Stream()));
	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", vhost_app_name.CStr(), url->Stream().CStr());
		ResponseResult(remote, OVT_PAYLOAD_TYPE_PLAY, 0, request_id, 404, msg);
		return;
	}

	auto session = OvtSession::Create(app, stream, stream->IssueUniqueSessionId(), remote);
	if(session == nullptr)
	{
		ov::String msg;
		msg.Format("Internal Error : Cannot create session");
		ResponseResult(remote, OVT_PAYLOAD_TYPE_PLAY, 0, request_id, 404, msg);
		return;
	}

	LinkRemoteWithStream(remote->GetId(), stream);

	ResponseResult(remote, OVT_PAYLOAD_TYPE_PLAY, session->GetId(), request_id, 200, "ok");

	stream->AddSession(session);
}

void OvtPublisher::HandleStopRequest(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto vhost_app_name = Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(url->Domain(), url->App());
	auto stream = std::static_pointer_cast<OvtStream>(GetStream(vhost_app_name, url->Stream()));

	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", vhost_app_name.CStr(), url->Stream().CStr());
		ResponseResult(remote, OVT_PAYLOAD_TYPE_STOP, 0, request_id, 404, msg);
		return;
	}

	ResponseResult(remote, OVT_PAYLOAD_TYPE_STOP, session_id, request_id, 200, "ok");

	stream->RemoveSession(session_id);
}

void OvtPublisher::ResponseResult(const std::shared_ptr<ov::Socket> &remote, uint8_t payload_type, uint32_t session_id, uint32_t request_id, uint32_t code, const ov::String &msg)
{
	Json::Value root;

	root["id"] = request_id;
	root["code"] = code;
	root["message"] = msg.CStr();

	SendResponse(remote, payload_type, session_id, ov::Json::Stringify(root));
}

void OvtPublisher::ResponseResult(const std::shared_ptr<ov::Socket> &remote, uint8_t payload_type, uint32_t session_id, uint32_t request_id,
									uint32_t code, const ov::String &msg, const ov::String &key, const Json::Value &value)
{
	Json::Value root;

	root["id"] = request_id;
	root["code"] = code;
	root["message"] = msg.CStr();
	root[key.CStr()] = value;

	SendResponse(remote, payload_type, session_id, ov::Json::Stringify(root));
}

void OvtPublisher::SendResponse(const std::shared_ptr<ov::Socket> &remote, uint8_t payload_type, uint32_t session_id, const ov::String &payload)
{
	size_t max_payload_size = OVT_DEFAULT_MAX_PACKET_SIZE - OVT_FIXED_HEADER_SIZE;
	size_t remain_payload_len = payload.GetLength();
	size_t offset = 0;
	uint32_t sn = 0;

	auto data = payload.ToData(false);
	auto buffer = data->GetDataAs<uint8_t>();

	while(remain_payload_len != 0)
	{
		// Serialize
		auto packet = std::make_shared<OvtPacket>();;
		// Session ID should be set in Session Level
		packet->SetSessionId(session_id);
		packet->SetPayloadType(payload_type);
		packet->SetMarker(0);
		packet->SetTimestampNow();

		if(remain_payload_len > max_payload_size)
		{
			packet->SetPayload(&buffer[offset], max_payload_size);
			offset += max_payload_size;
			remain_payload_len -= max_payload_size;
		}
		else
		{
			// The last packet of group has marker bit.
			packet->SetMarker(true);
			packet->SetPayload(&buffer[offset], remain_payload_len);
			remain_payload_len = 0;
		}

		packet->SetSequenceNumber(sn++);

		remote->Send(packet->GetData());
	}
}

bool OvtPublisher::LinkRemoteWithStream(int remote_id, std::shared_ptr<OvtStream> &stream)
{
	// For ungracefull disconnect
	// one remote id can be join multiple streams.
	_remote_stream_map.insert(std::pair<int, std::shared_ptr<OvtStream>>(remote_id, stream));

	return true;
}

bool OvtPublisher::UnlinkRemoteFromStream(int remote_id)
{
	_remote_stream_map.erase(remote_id);

	return true;
}

bool OvtPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return true;
}











