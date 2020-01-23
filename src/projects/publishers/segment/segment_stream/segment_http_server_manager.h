//
// Created by getroot on 20. 1. 23.
//

#pragma once

class SegmentHttpServerManager
{
public:
	SegmentHttpServerManager();
	~SegmentHttpServerManager();
/*
	std::share_ptr<HttpServer>&		GetHttpServer(int port);
	void 							AddServer(int port, const std::share_ptr<HttpServer> &server);
	void 							RemoveServer(int port);
*/
private:
	// Port Num : Server
	//std::map<int, std::shared_ptr<HttpServer>> _http_server_map;
};
