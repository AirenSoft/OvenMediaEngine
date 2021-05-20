#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "rtp_packet.h"
#include "rtcp_info/sender_report.h"

#define RTP_SEQ_MOD	(1<<16)
#define MAX_DROPOUT	3000
#define	MAX_MISORDER 100

class RtpReceiveStatistics
{
public:
	// There is no ssrc if the receiver does not transmit a packet.
	// So if receiver_ssrc is 0, it will be generated randomly. It can be used to create RR packet
	RtpReceiveStatistics(uint8_t payload_type, uint32_t media_ssrc, uint32_t clock_rate, uint32_t receiver_ssrc = 0);

	bool AddReceivedRtpPacket(const std::shared_ptr<RtpPacket> &packet);
	bool AddReceivedRtcpSenderReport(const std::shared_ptr<SenderReport> &report);

	bool HasElapsedSinceLastReportBlock(uint32_t milliseconds);
	std::shared_ptr<ReportBlock> GenerateReportBlock();

	uint8_t GetPayloadType();
	uint32_t GetMediaSSRC();
	uint32_t GetReceiverSSRC();

	uint64_t GetNumberOfFirRequests();
	void OnFirRequested();

	void InitSeq(uint16_t seq);
	bool UpdateSeq(uint16_t seq);
	bool UpdateStat(const std::shared_ptr<RtpPacket> &packet);

private:
	uint8_t 	_payload_type = 0;
	uint32_t	_media_ssrc = 0;
	uint32_t	_receiver_ssrc = 0;		// random generated local ssrc, for ReceiverReport, FIR ... 
	uint32_t	_clock_rate = 0;

	// From SR
	double		_last_sr_ntp = 0;
	uint32_t	_last_sr_msw = 0;
	uint32_t 	_last_sr_lsw = 0;

	// For Receiver Report
	bool		_first = true;
	
	// https://tools.ietf.org/html/rfc3550#page-75
	uint16_t	_max_seq = 0;
	uint32_t	_cycles = 0;
	uint32_t	_base_seq = 0;
	uint32_t	_probation = 0;
	uint32_t	_received_packets = 0;
	uint32_t	_expected_prior = 0;
	uint32_t	_received_prior = 0;
	uint32_t	_transit = 0;
	uint32_t	_jitter = 0;

	uint32_t	_received_bytes = 0;

	uint64_t	_initial_ntp = 0;
	uint32_t	_initial_rtp_timestamp = 0;

	uint64_t	_fir_request_count = 0;

	ov::StopWatch	_report_block_timer;
};