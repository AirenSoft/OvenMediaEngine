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
	std::share_ptr<http::svr::Server>&		GetHttpServer(int port);
	void 							AddServer(int port, const std::share_ptr<http::svr::Server> &server);
	void 							RemoveServer(int port);
*/
private:
	// Port Num : Server
	//std::map<int, std::shared_ptr<http::svr::Server>> _http_server_map;
};
