#include "rtp_receive_statistics.h"

RtpReceiveStatistics::RtpReceiveStatistics(uint8_t payload_type, uint32_t media_ssrc, uint32_t clock_rate, uint32_t receiver_ssrc)
{
	_payload_type = payload_type;
	_media_ssrc = media_ssrc;
	_clock_rate = clock_rate;

	if(receiver_ssrc == 0)
	{
		_receiver_ssrc = ov::Random::GenerateUInt32();
	}

	_report_block_timer.Start();
}

bool RtpReceiveStatistics::AddReceivedRtpPacket(const std::shared_ptr<RtpPacket> &packet)
{
	if(_first == true)
	{
		_first = false;
		_media_ssrc = packet->Ssrc();
		InitSeq(packet->SequenceNumber());
	}
	else
	{
		UpdateSeq(packet->SequenceNumber());
	}

	UpdateStat(packet);

	return true;
}

bool RtpReceiveStatistics::AddReceivedRtcpSenderReport(const std::shared_ptr<SenderReport> &report)
{
	_last_sr_ntp = ov::Clock::GetNtpTime();
	_last_sr_msw = report->GetMsw();
	_last_sr_lsw = report->GetLsw();

	return true;
}

uint8_t RtpReceiveStatistics::GetPayloadType()
{
	return _payload_type;
}

uint32_t RtpReceiveStatistics::GetMediaSSRC()
{
	return _media_ssrc;
}

uint32_t RtpReceiveStatistics::GetReceiverSSRC()
{
	return _receiver_ssrc;
}

uint64_t RtpReceiveStatistics::GetNumberOfFirRequests()
{
	return _fir_request_count;
}

void RtpReceiveStatistics::OnFirRequested()
{
	_fir_request_count ++;
}

void RtpReceiveStatistics::InitSeq(uint16_t seq)
{
	_base_seq = seq;
	_max_seq = seq;
	_cycles = 0;
	_received_packets = 0;
	_received_prior = 0;
	_expected_prior = 0;
}

bool RtpReceiveStatistics::UpdateSeq(uint16_t seq)
{
	// Wrapped
	if(seq < _max_seq)
	{
		_cycles += RTP_SEQ_MOD;
	}

	// TODO(Getroot): Check "large jump", "probation" and "reordered/duplicate"
	_max_seq = seq;

	return true;
}

bool RtpReceiveStatistics::UpdateStat(const std::shared_ptr<RtpPacket> &packet)
{
	_received_packets += 1;
	_received_bytes += packet->GetData()->GetLength();

	// Calc dropped packet for fraction lost of RR
	

	// Calc Jitter
	uint32_t transit = ((ov::Clock::GetNtpTime() * (double)_clock_rate) - packet->Timestamp());
	int32_t delta = transit - _transit;
	_transit = transit;

	if(delta < 0)
	{
		delta = delta * -1;
	}

	_jitter += (1. / 16.) * ((double)delta - _jitter);


	return true;
}

bool RtpReceiveStatistics::HasElapsedSinceLastReportBlock(uint32_t milliseconds)
{
	return _report_block_timer.IsElapsed(milliseconds);
}

std::shared_ptr<ReportBlock> RtpReceiveStatistics::GenerateReportBlock()
{
	// fraction lost
	int8_t fraction_lost = 0;
	
	int32_t extented_max = _cycles + _max_seq;
	int32_t expected = extented_max - _base_seq + 1;

	int32_t expected_interval = expected - _expected_prior;
	_expected_prior = expected;
	int32_t received_interval = _received_packets - _received_prior;
	_received_prior = _received_packets;

	int32_t lost_interval = expected_interval - received_interval;
	if(expected_interval == 0 || lost_interval <= 0)
	{
		fraction_lost = 0;
	}
	else
	{
		fraction_lost = (lost_interval << 8) / expected_interval;
	}

	// cumulative lost
	int32_t lost = expected - _received_packets;
	// Since this signed number is carried in 24 bits, it should be clamped
	// at 0x7fffff for positive loss or 0x800000 for negative loss rather
	// than wrapping around.
	lost = std::min(lost, 0x7fffff);
	lost = std::max(lost, -0x800000);

	// max sequence number
	uint32_t last_seq = _cycles + _max_seq;

	// jitter
	uint32_t jitter = _jitter;

	// last sr (LSR)
	// The middle 32 bits out of 64 in the NTP timestamp
	uint32_t lsr = ((_last_sr_msw & 0x0000ffff) << 16) | ((_last_sr_lsw & 0xffff0000) >> 16);

	// delay since last SR (DLSR)
	uint32_t dlsr = (ov::Clock::GetNtpTime()  - _last_sr_ntp)*(double)65536.0;
	
	logd("DEBUG", "%lf %lf %lf %u", ov::Clock::GetNtpTime(), _last_sr_ntp, (ov::Clock::GetNtpTime()  - _last_sr_ntp)*(double)65536.0, dlsr);

	_report_block_timer.Update();

	return std::make_shared<ReportBlock>(_media_ssrc, fraction_lost, lost, last_seq, jitter, lsr, dlsr);
}

