//
// Created by getroot on 19. 12. 9.
//

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>

#include <base/provider/stream.h>
#include <base/ovlibrary/semaphore.h>
#include <base/provider/application.h>
#include <modules/ovt_packetizer/ovt_packet.h>
#include <modules/ovt_packetizer/ovt_depacketizer.h>

namespace pvd
{
	class OvtStream : public pvd::Stream
	{
	public:
		enum class State
		{
			IDLE,
			CONNECTED,
			DESCRIBED,
			PLAYING,
			STOPPED,
			ERROR
		};

		static std::shared_ptr<OvtStream>
		Create(const std::shared_ptr<pvd::Application> &app, const std::shared_ptr<ov::Url> &url);

		explicit OvtStream(const std::shared_ptr<pvd::Application> &app, const StreamInfo &stream_info,
						   const std::shared_ptr<ov::Url> &url);

		~OvtStream() final;

	private:
		bool Start();
		bool Stop();
		void WorkerThread();
		bool ConnectOrigin();
		bool RequestDescribe();
		bool ReceiveDescribe(uint32_t request_id);
		bool RequestPlay();
		bool ReceivePlay(uint32_t request_id);
		bool RequestStop();
		bool ReceiveStop(uint32_t request_id);

		std::shared_ptr<OvtPacket> ReceivePacket();

		std::shared_ptr<ov::Url> _url;
		bool _stop_thread_flag;
		std::thread _worker_thread;

		ov::Socket _client_socket;

		uint32_t _last_request_id;
		uint32_t _session_id;

		OvtDepacketizer _depacketizer;

		std::shared_ptr<pvd::Application> _app;

		State	_state;
	};
}