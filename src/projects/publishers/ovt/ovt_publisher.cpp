#include <base/ovlibrary/url.h>
#include "ovt_private.h"
#include "ovt_publisher.h"
#include "ovt_session.h"

std::shared_ptr<OvtPublisher> OvtPublisher::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto obj = std::make_shared<OvtPublisher>(server_config, router);

	if (!obj->Start())
	{
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

	const auto &ovt_config = server_config.GetBind().GetPublishers().GetOvt();

	if (ovt_config.IsParsed() == false)
	{
		logti("%s is disabled by configuration", GetPublisherName());
		return true;
	}

	auto &port_config = ovt_config.GetPort();
	int port = port_config.GetPort();

	if (port > 0)
	{
		const ov::String &ip = server_config.GetIp();
		ov::SocketAddress address = ov::SocketAddress(ip.IsEmpty() ? nullptr : ip.CStr(), static_cast<uint16_t>(port));

		bool is_parsed;

		auto worker_count = ovt_config.GetWorkerCount(&is_parsed);
		worker_count = is_parsed ? worker_count : PHYSICAL_PORT_USE_DEFAULT_COUNT;

		_server_port = PhysicalPortManager::GetInstance()->CreatePort("OvtPub", port_config.GetSocketType(), address, worker_count);
		if (_server_port != nullptr)
		{
			logti("%s is listening on %s/%s", GetPublisherName(), address.ToString().CStr(), ov::StringFromSocketType(port_config.GetSocketType()));
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
	if(IsModuleAvailable() == false)
	{
		return nullptr;
	}

	return OvtApplication::Create(OvtPublisher::GetSharedPtrAs<pub::Publisher>(), application_info);
}

bool OvtPublisher::OnDeletePublisherApplication(const std::shared_ptr<pub::Application> &application)
{
	return true;
}

std::shared_ptr<OvtDepacketizer> OvtPublisher::GetDepacketizer(int remote_id)
{
	std::lock_guard<std::mutex> guard(_depacketizers_lock);
	std::shared_ptr<OvtDepacketizer> depacketizer;

	// if there is no depacketizer, create
	if(_depacketizers.find(remote_id) == _depacketizers.end())
	{
		depacketizer = std::make_shared<OvtDepacketizer>();
		_depacketizers[remote_id] = depacketizer;
	}
	else
	{
		return _depacketizers[remote_id];
	}

	return depacketizer;
}

bool OvtPublisher::RemoveDepacketizer(int remote_id)
{
	std::lock_guard<std::mutex> guard(_depacketizers_lock);
	_depacketizers.erase(remote_id);
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
	auto depacketizer = GetDepacketizer(remote->GetNativeHandle());
	
	if(depacketizer->AppendPacket(data) == false)
	{
		ResponseResult(remote, 0, "unknown", 0, 500, "Server Internals Error");
		return;
	}

	if(!depacketizer->IsAvailableMessage())
	{
		logtc("Unavailable message");
	}

	while(depacketizer->IsAvailableMessage())
	{
		auto message = depacketizer->PopMessage();
		
		// Parsing Payload
		ov::String payload(message->GetDataAs<char>(), message->GetLength());
		ov::JsonObject object = ov::Json::Parse(payload);

		if(object.IsNull())
		{
			ResponseResult(remote, 0, "unknown", 0, 404, "An invalid request : Json format");
			return;
		}

		Json::Value &json_request_id = object.GetJsonValue()["id"];
		Json::Value &json_request_app = object.GetJsonValue()["application"];
		Json::Value &json_request_target = object.GetJsonValue()["target"];

		if(json_request_id.isNull() || !json_request_id.isUInt() ||
			json_request_app.isNull() || !json_request_app.isString() ||
			json_request_target.isNull() || !json_request_target.isString())
		{
			ResponseResult(remote, 0, "unknown", 0, 404, "An invalid request : id or target or application are invalid");
			return;
		}

		uint32_t request_id = json_request_id.asUInt();
		ov::String app = json_request_app.asString().c_str();
		auto url = ov::Url::Parse(json_request_target.asString().c_str());
		if(url == nullptr)
		{
			ResponseResult(remote, 0, "unknown", json_request_id.asUInt(), 404, "An invalid request : Target is not valid");
			return;
		}

		if(app.UpperCaseString() == "DESCRIBE")
		{
			HandleDescribeRequest(remote, request_id, url);
		}
		else if(app.UpperCaseString() == "PLAY")
		{
			HandlePlayRequest(remote, request_id, url);
		}
		else if(app.UpperCaseString() == "STOP")
		{
			HandleStopRequest(remote, 0, request_id, url);
		}
		else
		{
			ResponseResult(remote, 0, app.CStr(), request_id, 404, "Unknown application");
		}
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
	logti("OvtProvider is disconnected : %s", remote->ToString().CStr());
	// disconnect means when the stream disconnects itself.
	if(reason != PhysicalPortDisconnectReason::Disconnect)
	{
		auto streams = _remote_stream_map.equal_range(remote->GetNativeHandle());
		for(auto it = streams.first; it != streams.second; ++it)
		{
			auto stream = it->second;
			stream->RemoveSessionByConnectorId(remote->GetNativeHandle());
		}
	}
	UnlinkRemoteFromStream(remote->GetNativeHandle());
	RemoveDepacketizer(remote->GetNativeHandle());
}

void OvtPublisher::HandleDescribeRequest(const std::shared_ptr<ov::Socket> &remote, const uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto orchestrator = ocst::Orchestrator::GetInstance();

	auto host_name = url->Host();
	auto app_name = url->App();
	auto vhost_app_name = orchestrator->ResolveApplicationNameFromDomain(host_name, app_name);
	auto stream_name = url->Stream();
	ov::String msg;

	auto stream = std::static_pointer_cast<OvtStream>(GetStream(vhost_app_name, stream_name));
	if (stream == nullptr)
	{
		// If the stream does not exists, request to the provider
		if (orchestrator->RequestPullStream(url, vhost_app_name, stream_name) == false)
		{
			msg.Format("There is no such stream (%s/%s)", vhost_app_name.CStr(), url->Stream().CStr());
			ResponseResult(remote, 0, "describe", request_id, 404, msg);
			return;
		}
		else
		{
			stream = std::static_pointer_cast<OvtStream>(GetStream(vhost_app_name, stream_name));
			if (stream == nullptr)
			{
				msg.Format("Could not pull the stream: [%s/%s]", vhost_app_name.CStr(), stream_name.CStr());
				ResponseResult(remote, 0, "describe", request_id, 404, msg);
				return;
			}
		}
	}

	if(stream->WaitUntilStart(3000) == false)
	{
		msg.Format("(%s/%s) stream has not started.", vhost_app_name.CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, "describe", request_id, 202, msg);
		return;
	}

	Json::Value description;
	if(stream->GetDescription(description) == false)
	{
		msg.Format("(%s/%s) stream doesn't have description.", vhost_app_name.CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, "describe", request_id, 404, msg);
		return;
	}

	ResponseResult(remote, 0, "describe", request_id, 200, "ok", description);
}

void OvtPublisher::HandlePlayRequest(const std::shared_ptr<ov::Socket> &remote, uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(url->Host(), url->App());
	
	auto app = std::static_pointer_cast<OvtApplication>(GetApplicationByName(vhost_app_name));
	if(app == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such app (%s)", vhost_app_name.CStr());
		ResponseResult(remote, 0, "play", request_id, 404, msg);
		return;
	}

	auto stream = std::static_pointer_cast<OvtStream>(app->GetStream(url->Stream()));
	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", vhost_app_name.CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, "play", request_id, 404, msg);
		return;
	}

	// Session ID is remote socket's ID
	auto session = OvtSession::Create(app, stream, remote->GetNativeHandle(), remote);
	if(session == nullptr)
	{
		ov::String msg;
		msg.Format("Internal Error : Cannot create session");
		ResponseResult(remote, 0, "play", request_id, 404, msg);
		return;
	}

	LinkRemoteWithStream(remote->GetNativeHandle(), stream);

	ResponseResult(remote, session->GetId(), "play", request_id, 200, "ok");

	stream->AddSession(session);
}

void OvtPublisher::HandleStopRequest(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, uint32_t request_id, const std::shared_ptr<const ov::Url> &url)
{
	auto vhost_app_name = ocst::Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(url->Host(), url->App());
	auto stream = std::static_pointer_cast<OvtStream>(GetStream(vhost_app_name, url->Stream()));

	if(stream == nullptr)
	{
		ov::String msg;
		msg.Format("There is no such stream (%s/%s)", vhost_app_name.CStr(), url->Stream().CStr());
		ResponseResult(remote, 0, "stop", request_id, 404, msg);
		return;
	}

	ResponseResult(remote, session_id, "stop", request_id, 200, "ok");

	// Session ID is remote socket's ID
	stream->RemoveSession(remote->GetNativeHandle());
}

void OvtPublisher::ResponseResult(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, const ov::String app, uint32_t request_id, uint32_t code, const ov::String &msg)
{
	Json::Value root;

	root["id"] = request_id;
	root["application"] = app.CStr();
	root["code"] = code;
	root["message"] = msg.CStr();

	SendResponse(remote, session_id, ov::Json::Stringify(root));
}

void OvtPublisher::ResponseResult(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, ov::String app, uint32_t request_id, uint32_t code, const ov::String &msg, const Json::Value &contents)
{
	Json::Value root;

	root["id"] = request_id;
	root["application"] = app.CStr();
	root["code"] = code;
	root["message"] = msg.CStr();
	root["contents"] = contents;

	SendResponse(remote, session_id, ov::Json::Stringify(root));
}

void OvtPublisher::SendResponse(const std::shared_ptr<ov::Socket> &remote, uint32_t session_id, const ov::String &payload)
{
	OvtPacketizer packetizer;

	if(packetizer.PacketizeMessage(OVT_PAYLOAD_TYPE_MESSAGE_RESPONSE, ov::Clock::NowMSec(), payload.ToData(false)) == false)
	{
		return;
	}

	while(packetizer.IsAvailablePackets())
	{
		auto packet = packetizer.PopPacket();
		if(packet == nullptr)
		{
			return;
		}

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








