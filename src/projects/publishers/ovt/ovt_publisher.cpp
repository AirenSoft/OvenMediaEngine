#include "ovt_private.h"
#include "ovt_publisher.h"

std::shared_ptr<OvtPublisher> OvtPublisher::Create(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto ovt = std::make_shared<OvtPublisher>(host_info, router);

	if (!ovt->Start())
	{
		logte("An error occurred while creating OvtPublisher");
		return nullptr;
	}

	return ovt;
}

OvtPublisher::OvtPublisher(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
		: Publisher(host_info, router)
{

}

OvtPublisher::~OvtPublisher()
{
	logtd("OvtPublisher has been terminated finally");
}

bool OvtPublisher::Start()
{
	// Listen to localhost:<relay_port>
	auto host_info = GetHostInfo();
	auto &origin = host_info.GetPorts().GetOriginPort();

	if (origin.IsParsed())
	{
		int port = origin.GetPort();

		if (port > 0)
		{
			const ov::String &ip = host_info.GetIp();
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
			logte("Invalid relay port: %d", port);
		}
	}
	else
	{
		// Relay feature is disabled
	}


	return true;
}

bool OvtPublisher::Stop()
{
	if (_server_port != nullptr)
	{
		_server_port->RemoveObserver(this);
	}

	return true;
}

std::shared_ptr<Application> OvtPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return OvtApplication::Create(application_info);
}

bool OvtPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections)
{
	return true;
}

void OvtPublisher::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	// 접속하면 이 함수가 호출되지만 여기서는 알바 아니다.
	// OnDataReceived에서 register를 하면 정식 Client로 처리한다.
}

void OvtPublisher::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
									const ov::SocketAddress &address,
									const std::shared_ptr<const ov::Data> &data)
{
	// ovt_client가 register를 할 때 본 함수가 호출된다.
	// 여기서 seesion을 만들어서 요청한 stream에 등록해야 한다.
}
void OvtPublisher::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
									PhysicalPortDisconnectReason reason,
									const std::shared_ptr<const ov::Error> &error)
{
	// seesion을 제거한다.
}

void OvtPublisher::HandleRegister(const std::shared_ptr<ov::Socket> &remote, const RelayPacket &packet)
{

}