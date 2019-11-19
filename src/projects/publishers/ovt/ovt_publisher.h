#pragma once

#include "relay_datastructure.h"

#include "base/common_types.h"
#include "base/publisher/publisher.h"
#include "base/media_route/media_route_application_interface.h"

#include "ovt_application.h"

class OvtPublisher : public Publisher, public PhysicalPortObserver
{
public:
	static std::shared_ptr<OvtPublisher> Create(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);

	OvtPublisher(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);
	~OvtPublisher() override;

private:
	bool Start() override;
	bool Stop() override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	cfg::PublisherType GetPublisherType() const override
	{
		return cfg::PublisherType::Ovt;
	}
	const char *GetPublisherName() const override
	{
		return "OVT";
	}

	std::shared_ptr<Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections) override;

	//--------------------------------------------------------------------

	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;
	void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error) override;
	//--------------------------------------------------------------------

	void HandleRegister(const std::shared_ptr<ov::Socket> &remote, const RelayPacket &packet);

private:
	std::shared_ptr<PhysicalPort> _server_port;
};
