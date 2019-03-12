//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <http_server/http_server.h>
#include <http_server/https_server.h>
#include <http_server/interceptors/http_request_interceptors.h>

// Peer ID of OME
#define P2P_OME_PEER_ID                             0
#define P2P_INVALID_PEER_ID                         -1
#define P2P_DEFAULT_MAX_CLIENTS_PER_HOST            3

typedef int peer_id_t;

class RtcP2PManager
{
public:
	peer_id_t FindHost(const std::shared_ptr<WebSocketClient> &response);

protected:
	enum class OsType : char
	{
		// PC

		Windows,
		macOS,
		Ubuntu,

		// Mobile

		Android,
		iOS,

		Other
	};

	enum class BrowserType : uint16_t
	{
		Chrome,
		FireFox,
		Safari,
		Edge,
		Other
	};

	struct Version
	{
		int first = 0;
		int second = 0;
		int third = 0;
		int fourth = 0;

		ov::String version;

		explicit Version(int first)
			: first(first)
		{
			version.Format("%d", first);
		}

		Version(int first, int second)
			: first(first),
			  second(second)
		{
			version.Format("%d.%d", first, second);
		}

		Version(int first, int second, int third)
			: first(first),
			  second(second),
			  third(third)
		{
			version.Format("%d.%d.%d", first, second, third);
		}

		Version(int first, int second, int third, int fourth)
			: first(first),
			  second(second),
			  third(third),
			  fourth(fourth)
		{
			version.Format("%d.%d.%d.%d", first, second, third, fourth);
		}

		explicit Version(const ov::String &version)
		{
			this->version = version;

			auto tokens = version.Split(".");

			switch(tokens.size())
			{
				case 4:
					fourth = ov::Converter::ToInt32(tokens[3]);

				case 3:
					third = ov::Converter::ToInt32(tokens[2]);

				case 2:
					second = ov::Converter::ToInt32(tokens[1]);

				case 1:
					first = ov::Converter::ToInt32(tokens[0]);

				default:
					break;
			}
		}
	};

	struct PeerType
	{
		OsType os_type = OsType::Other;
		Version os_version;

		BrowserType browser_type = BrowserType::Other;
		Version browser_version;

		PeerType()
			: os_version(0),
			  browser_version(0)
		{
		}
	};

	struct HostInfo
	{
		// host id
		peer_id_t id = 0;

		// host type
		PeerType type;

		// total client count
		int peer_count = 0;
	};

	struct ClientInfo
	{
	};

	PeerType ParseUserAgent(ov::String user_agent);
	std::shared_ptr<RtcP2PManager::HostInfo> FindHost(PeerType client_type);

	bool CheckCompatible(const PeerType &host, const PeerType &client);

	// List of hosts that cannot accept client
	// key: host id, value: host info
	std::map<ov::String, std::shared_ptr<HostInfo>> _host_list;

	// List of hosts that can accept client
	// key: host id, value: host info
	std::map<ov::String, std::shared_ptr<HostInfo>> _available_list;

	// Client list
	// key: host id, value: client info
	std::map<ov::String, std::shared_ptr<ClientInfo>> _client_list;
};
