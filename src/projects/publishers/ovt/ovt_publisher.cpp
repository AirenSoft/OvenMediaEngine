#include <base/ovlibrary/url.h>
#include "ovt_private.h"
#include "ovt_publisher.h"
#include "ovt_session.h"

std::shared_ptr<OvtPublisher> OvtPublisher::Create(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto ovt = std::make_shared<OvtPublisher>(server_config, host_info, router);

	if (!ovt->Start())
	{
		logte("An error occurred while creating OvtPublisher");
		return nullptr;
	}

	return ovt;
}

OvtPublisher::OvtPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(server_config, host_info, router)
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
		int port = origin.GetPort();

		if (port > 0)
		{
			const ov::String &ip = server_config.GetIp();
			ov::SocketAddress address = ov::SocketAddress(ip.IsEmpty() ? nullptr : ip.CStr(), static_cast<uint16_t>(port));

			_server_port = PhysicalPortManager::Instance()->CreatePort(origin.GetSocketType(), address);
			if (_server_port != nullptr)
			{
				logti("Trying to start relay server on %s", address.ToString().CStr());
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
	}

	return Publisher::Stop();
}

std::shared_ptr<Application> OvtPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return OvtApplication::Create(application_info);
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
	if(!packet->IsValid())
	{
		// If packet is not valid, it is not necessary to response
		return;
	}

	// Parsing Payload
	ov::String payload((const char *)packet->Payload(), packet->PayloadLength());
	ov::JsonObject object = ov::Json::Parse(payload);

	if(object.IsNull())
	{
		ResponseResult(remote, packet->SessionId(), 0, 404, "An invalid request : Json format");
		return;
	}

	Json::Value &json_request_id = object.GetJsonValue()["id"];
	Json::Value &json_request_url = object.GetJsonValue()["url"];

	if(json_request_id.isNull() || json_request_url.isNull() || !json_request_id.isUInt())
	{
		ResponseResult(remote, packet->SessionId(), 0, 404, "An invalid request : Id or Url are not valid");
		return;
	}

	uint32_t request_id = json_request_id.asUInt();
	auto url = ov::Url::Parse(json_request_url.asString());
	if(url == nullptr)
	{
		ResponseResult(remote, packet->SessionId(), json_request_id.asUInt(), 404, "An invalid request : Url is not valid");
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
			ResponseResult(remote, packet->SessionId(), request_id, 404, "An invalid request");
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
	// Remove session
	auto streams = _remote_stream_map[remote->GetId()];
	if(streams == nullptr)
	{
		return;
	}

	for(const auto &stream : *streams)
	{
		stream->RemoveSessionByConnectorId(remote->GetId());
	}

	UnlinkRemoteFromStream(remote->GetId());
}

void OvtPublisher::HandleDescribeRequest(const std::shared_ptr<ov::Socket> &remote, const uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto stream = std::static_pointer_cast<OvtStream>(GetStream(url->App(), url->Stream()));
	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", url->App().CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, request_id, 404, msg);
		return;
	}

	Json::Value description = stream->GetDescription();
	ResponseResult(remote, 0, request_id, 200, "ok", "stream", description);
}

void OvtPublisher::HandlePlayRequest(const std::shared_ptr<ov::Socket> &remote, uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto app = std::static_pointer_cast<OvtApplication>(GetApplicationByName(url->App().CStr()));
	if(app == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such app (%s)", url->App().CStr());
		ResponseResult(remote, 0, request_id, 404, msg);
		return;
	}

	auto stream = std::static_pointer_cast<OvtStream>(GetStream(url->App(), url->Stream()));
	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", url->App().CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, request_id, 404, msg);
		return;
	}

	auto session = OvtSession::Create(app, stream, stream->IssueUniqueSessionId(), remote);
	if(session == nullptr)
	{
		ov::String msg;
		msg.Format("Internal Error : Cannot create session");
		ResponseResult(remote, 0, request_id, 404, msg);
		return;
	}

	//LinkRemoteWithStream(remote->GetId(), stream);

	ResponseResult(remote, session->GetId(), request_id, 200, "ok");

	stream->AddSession(session);
}

void OvtPublisher::HandleStopRequest(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto stream = std::static_pointer_cast<OvtStream>(GetStream(url->App(), url->Stream()));
	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", url->App().CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, request_id, 404, msg);
		return;
	}

	stream->RemoveSession(session_id);

	ResponseResult(remote, session_id, request_id, 200, "ok");
}

void OvtPublisher::ResponseResult(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, uint32_t request_id, uint32_t code, const ov::String &msg)
{
	Json::Value root;

	root["id"] = request_id;
	root["code"] = code;
	root["message"] = msg.CStr();

	SendResponse(remote, session_id, ov::Json::Stringify(root));
}

void OvtPublisher::ResponseResult(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, uint32_t request_id,
									uint32_t code, const ov::String &msg, const ov::String &key, const Json::Value &value)
{
	Json::Value root;

	root["id"] = request_id;
	root["code"] = code;
	root["message"] = msg.CStr();
	root[key.CStr()] = value;

	SendResponse(remote, session_id, ov::Json::Stringify(root));
}

void OvtPublisher::SendResponse(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, const ov::String &payload)
{
	OvtPacket	packet;

	packet.SetSessionId(session_id);
	packet.SetPayloadType(OVT_PAYLOAD_TYPE_RESPONSE);
	packet.SetMarker(0);
	packet.SetTimestampNow();

	if(!payload.IsEmpty())
	{
		auto payload_data = payload.ToData(false);
		packet.SetPayload(payload_data->GetDataAs<uint8_t>(), payload_data->GetLength());
	}

	remote->Send(packet.GetData());
}

bool OvtPublisher::LinkRemoteWithStream(int remote_id, std::shared_ptr<OvtStream> &stream)
{
	// For ungracefull disconnect
	// one remote id can be join multiple streams.

	if(_remote_stream_map.find(remote_id) == _remote_stream_map.end())
	{
		auto stream_vector = std::make_shared<std::vector<std::shared_ptr<OvtStream>>>();
		stream_vector->push_back(stream);
		_remote_stream_map[remote_id] = stream_vector;
	}
	else
	{
		_remote_stream_map[remote_id]->push_back(stream);
	}

	return true;
}

bool OvtPublisher::UnlinkRemoteFromStream(int remote_id)
{
	_remote_stream_map.erase(remote_id);
}

bool OvtPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections)
{
	return true;
}











