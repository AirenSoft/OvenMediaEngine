//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <iostream>
// #include <stdio.h>
#include <map>
#include <thread>

#include <base/ovlibrary/ovlibrary.h>

#include "rtmp_connector.h"

using namespace std;

// 임시로 사용하는 TCP 서버 임
class RtmpTcpServer
{
	// 인터페이스
public:
	virtual int OnConnect(RtmpConnector *conn, ov::String app, ov::String flashVer, ov::String swfUrl, ov::String tcUrl)
	{
		logd("RtmpTcpServer", "onConnect rtmp(%p), app(%s), flashVer(%s), swfUrl(%s), tcUrl(%s)", conn, app.CStr(), flashVer.CStr(), swfUrl.CStr(), tcUrl.CStr());
		return 0;
	}

	virtual void OnReleaseStream(RtmpConnector *conn, ov::String stream)
	{
		logd("RtmpTcpServer", "onReleaseStream rtmp(%p), stream(%s)", conn, stream.CStr());
	}

	virtual void OnFCPublish(RtmpConnector *conn, ov::String stream)
	{
		logd("RtmpTcpServer", "onFCPublish rtmp(%p), stream(%s)", conn, stream.CStr());
	}

	virtual void OnCreateStream(RtmpConnector *conn)
	{
		logd("RtmpTcpServer", "onCreateStream rtmp(%p)", conn);
	}

	virtual void OnPublish(RtmpConnector *conn, ov::String stream, ov::String type)
	{
		logd("RtmpTcpServer", "onPublish rtmp(%p), stream(%s), type(%s)", conn, stream.CStr(), type.CStr());
	}

	virtual void OnMetaData(RtmpConnector *conn)
	{
		logd("RtmpTcpServer", "onMetaData rtmp(%p)", conn);
	}

	virtual void OnFCUnpublish(RtmpConnector *conn, ov::String stream)
	{
		logd("RtmpTcpServer", "onFCUnpublish rtmp(%p)", conn);
	}

	virtual void OnDeleteStream(RtmpConnector *conn)
	{
		logd("RtmpTcpServer", "onDeleteStream rtmp(%p)", conn);
	}

	virtual void OnVideoPacket(RtmpConnector *conn, int8_t has_abs_time, uint32_t time, int8_t *body, uint32_t body_size)
	{
		logd("RtmpTcpServer", "OnVideoPacket. rtmp(%p), abs(%d) time %u size %u bytes", conn, has_abs_time, time, body_size);
	}

	virtual void OnAudioPacket(RtmpConnector *conn, int8_t has_abs_time, uint32_t time, int8_t *body, uint32_t body_size)
	{
		logd("RtmpTcpServer", "OnAudioPacket. rtmp(%p), abs(%d) time %u size %u bytes", conn, has_abs_time, time, body_size);
	}

	virtual void OnDisconnect(RtmpConnector *conn)
	{
		logd("RtmpTcpServer", "OnDisconnect. rtmp(%p)", conn);
	}

public:
	void Start()
	{
		// TODO : Physical port 모듈을 사용하도록 변경해야함.
		_listen_port = 1935;

		_server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

		memset(&_server_address, 0, sizeof(_server_address));

		_server_address.sin_family = AF_INET;

		_server_address.sin_addr.s_addr = htonl(INADDR_ANY);

		_server_address.sin_port = htons(_listen_port);

		int enable = 1;
		if(setsockopt(_server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		{
			loge("RtmpTcpServer", "setsockopt(SO_REUSEADDR) failed");
		}

		enable = 1;
		if(setsockopt(_server_sock_fd, SOL_SOCKET, TCP_NODELAY, &enable, sizeof(int)) < 0)
		{
			loge("RtmpTcpServer", "setsockopt(TCP_NODELAY) failed");
		}


		while(1)
		{
			int ret = bind(_server_sock_fd, (struct sockaddr *)&_server_address, sizeof(_server_address));
			if(ret < 0)
			{
				loge("RtmpTcpServer", "socket bind failed. retry 3 second...");
				sleep(3);
				continue;
			}
			else
			{
				break;
			}
		}

		listen(_server_sock_fd, 5);

		_thread_server_task_kill_flag = false;
		_thread_server_task = std::thread(&RtmpTcpServer::ServerTask, this);
	}

	void Stop()
	{
		_thread_server_task_kill_flag = true;
		_thread_server_task.join();

		if(_server_sock_fd > 0)
		{
			close(_server_sock_fd);
		}
	}

private:

	void ClientTask(int32_t socket_fd)
	{
		logd("RtmpTcpServer", "started rtmp client thread");

		// TCP NO Delay
		// TODO(SOULK) : 클라이언트 소켓에 NODELAY 옵션을 적용하면 반영이 되는건가?
		int enable = 1;
		if(setsockopt(socket_fd, SOL_SOCKET, TCP_NODELAY, &enable, sizeof(int)) < 0)
		{
			loge("RtmpTcpServer", "setsockopt(TCP_NODELAY) failed");
		}


		RtmpConnector *rtmp_conn = new RtmpConnector(socket_fd, (void *)this);

		while(1)
		{
			int ret = rtmp_conn->do_cycle();
			if(ret != 0)
			{
				// 접속이 종료가 되면...  loop를 빠져나옴.
				break;
			}
		}

		OnDisconnect(rtmp_conn);

		//TODO: thread safe
		FD_SET(socket_fd, &_read_fds);

		delete rtmp_conn;
		rtmp_conn = nullptr;

		close(socket_fd);

		logd("RtmpTcpServer", "terminated rtmp client thread");
	}

	void StartClient(int32_t socket_fd)
	{
		std::thread(&RtmpTcpServer::ClientTask, this, socket_fd);
	}

	void ServerTask()
	{
		logd("RtmpTcpServer", "started rtmp server thread");

		FD_ZERO(&_read_fds);
		FD_SET(_server_sock_fd, &_read_fds);

		while(!_thread_server_task_kill_flag)
		{
			_all_fds = _read_fds;

			// tineout 0.1 second
			struct timeval tv = { 0, 100000 };
			int state = select(_server_sock_fd + 1, &_all_fds, NULL, NULL, &tv);
			if(state > 0)
			{
				// 서버 소켓에서 이벤트가 발생하는 거라면 Accept
				if(FD_ISSET(_server_sock_fd, &_all_fds))
				{
					struct sockaddr_in client_address;

					socklen_t sosize = sizeof(struct sockaddr_in);
					int new_socket_fd = accept(_server_sock_fd, (struct sockaddr *)&client_address, &sosize);

					logd("RtmpTcpServer", "accept new client rtmp(%d), addr(%s), port(%d)", new_socket_fd, inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

					FD_SET(new_socket_fd, &_read_fds);

					std::thread(&RtmpTcpServer::ClientTask, this, new_socket_fd).detach();
				}
			}
			else if(state < 0 && (errno != EINTR))
			{
				loge("RtmpTcpServer", "error %d", state);
			}
			else
			{
				// accept timeout. state = 0
			}
		}

		logd("RtmpTcpServer", "terminated rtmp server thread");
	}

	// 수신 포트
	int32_t _listen_port;

	// 서버 소켓 디스크립션
	int _server_sock_fd;

	// 서버 주소 정보
	struct sockaddr_in _server_address;

	// 소켓 디스크립터
	fd_set _read_fds;
	fd_set _all_fds;

	// 스레드
	volatile bool _thread_server_task_kill_flag;
	std::thread _thread_server_task;
};

