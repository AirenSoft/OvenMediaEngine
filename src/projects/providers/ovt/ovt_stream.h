//
// Created by getroot on 19. 12. 9.
//

#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/url.h>
#include <base/ovlibrary/semaphore.h>
#include <modules/ovt_packetizer/ovt_packet.h>
#include <modules/ovt_packetizer/ovt_depacketizer.h>
#include <monitoring/monitoring.h>

#include <base/provider/pull_provider/application.h>
#include <base/provider/pull_provider/stream.h>

#define OVT_TIMEOUT_MSEC		3000
namespace pvd
{
	class OvtStream : public pvd::PullStream
	{
	public:
		static std::shared_ptr<OvtStream> Create(const std::shared_ptr<pvd::PullApplication> &application, const uint32_t stream_id, const ov::String &stream_name,	const std::vector<ov::String> &url_list);

		OvtStream(const std::shared_ptr<pvd::PullApplication> &application, const info::Stream &stream_info, const std::vector<ov::String> &url_list);
		~OvtStream() final;

		int GetFileDescriptorForDetectingEvent() override;
		// If this stream belongs to the Pull provider, 
		// this function is called periodically by the StreamMotor of application. 
		// Media data has to be processed here.
		PullStream::ProcessMediaResult ProcessMediaPacket() override;

	private:

		enum class ReceivePacketResult : uint8_t
		{
			COMPLETE,
			IMCOMPLETE,
			DISCONNECTED,
			ERROR, 
			TIMEOUT,
			ALREADY_COMPLETED,
		};

		bool Start() override;
		bool Play() override;
		bool Stop() override;
		bool ConnectOrigin();
		bool RequestDescribe();
		bool ReceiveDescribe(uint32_t request_id);
		bool RequestPlay();
		bool ReceivePlay(uint32_t request_id);
		bool RequestStop();
		bool ReceiveStop(uint32_t request_id, const std::shared_ptr<OvtPacket> &packet);

		void ResetRecvBuffer();
		ReceivePacketResult ProceedToReceivePacket(bool non_block = false);
		std::shared_ptr<OvtPacket> GetPacket();

		// Get fragmented packets and parse to get message
		// it returns only payload
		std::shared_ptr<ov::Data> ReceiveMessage();

		std::vector<std::shared_ptr<const ov::Url>> _url_list;
		std::shared_ptr<const ov::Url>				_curr_url;

		ov::Socket _client_socket;
		ov::Data _recv_buffer;
		off_t _recv_buffer_offset;
		std::shared_ptr<OvtPacket> _packet_mold;

		uint32_t _last_request_id;
		uint32_t _session_id;

		int64_t _origin_request_time_msec = 0;
		int64_t _origin_response_time_msec = 0;

		OvtDepacketizer _depacketizer;
		std::shared_ptr<mon::StreamMetrics> _stream_metrics;
	};
}