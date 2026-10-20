//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "mpegts_datagram_reorder_buffer.h"

#include <base/ovlibrary/clock.h>

#include <algorithm>
#include <string_view>

#include "mpegts_packet.h"

namespace mpegts
{
	DatagramReorderBuffer::DatagramReorderBuffer(size_t max_datagrams, int64_t timeout_msec, std::function<int64_t()> clock)
		: _max_datagrams(max_datagrams < 1 ? 1 : max_datagrams),
		  _timeout_msec(timeout_msec),
		  _clock(std::move(clock))
	{
	}

	int64_t DatagramReorderBuffer::Now() const
	{
		return _clock ? _clock() : static_cast<int64_t>(ov::Clock::NowMSec());
	}

	uint8_t DatagramReorderBuffer::Displacement(uint8_t cc, uint8_t expected)
	{
		return static_cast<uint8_t>((cc - expected) & 0x0F);
	}

	uint16_t DatagramReorderBuffer::PacketPid(const uint8_t *packet)
	{
		return static_cast<uint16_t>(((packet[1] & 0x1F) << 8) | packet[2]);
	}

	DatagramReorderBuffer::Summary DatagramReorderBuffer::Parse(const std::shared_ptr<const ov::Data> &datagram)
	{
		Summary summary;

		if (datagram == nullptr)
		{
			return summary;
		}

		const uint8_t *data = datagram->GetDataAs<uint8_t>();
		const size_t length = datagram->GetLength();

		// A UDP datagram must be a whole number of TS packets to be trusted for CC-based ordering.
		if ((data == nullptr) || (length < MPEGTS_MIN_PACKET_SIZE) || ((length % MPEGTS_MIN_PACKET_SIZE) != 0))
		{
			return summary;
		}

		for (size_t offset = 0; offset < length; offset += MPEGTS_MIN_PACKET_SIZE)
		{
			if (data[offset] != MPEGTS_SYNC_BYTE)
			{
				// Not aligned; the summary is left un-aligned so the caller bypasses reordering.
				return summary;
			}

			const bool transport_error			   = (data[offset + 1] & 0x80) != 0;
			const uint16_t pid					   = PacketPid(data + offset);
			const uint8_t adaptation_field_control = (data[offset + 3] >> 4) & 0x03;
			const uint8_t continuity_counter	   = data[offset + 3] & 0x0F;
			const bool has_adaptation			   = (adaptation_field_control & 0b10) != 0;
			const bool has_payload				   = (adaptation_field_control & 0b01) != 0;

			if (has_adaptation)
			{
				const uint8_t adaptation_field_length	  = data[offset + 4];

				// A valid adaptation field cannot exceed the packet body: at most 183 bytes when
				// adaptation-only (AFC=10), and 182 when a payload must also follow (AFC=11).
				// A larger value means the packet is corrupt; treat the whole datagram as untrusted
				// so it bypasses reordering rather than being sequenced on possibly-bogus counters.
				const uint8_t max_adaptation_field_length = has_payload
																? (MPEGTS_MIN_PACKET_SIZE - MPEGTS_HEADER_SIZE - 2)
																: (MPEGTS_MIN_PACKET_SIZE - MPEGTS_HEADER_SIZE - 1);
				if (adaptation_field_length > max_adaptation_field_length)
				{
					return summary;
				}

				if ((adaptation_field_length > 0) && ((data[offset + 5] & 0x80) != 0))
				{
					summary.discontinuity = true;
				}
			}

			// Only payload-bearing packets advance the continuity counter.
			// Transport-error and null packets carry no usable continuity information.
			if (transport_error || (has_payload == false) || (pid == 0x1FFF))
			{
				continue;
			}

			if (summary.first_cc.find(pid) == summary.first_cc.end())
			{
				summary.first_cc[pid] = continuity_counter;
				summary.pid_order.push_back(pid);
			}
			summary.last_cc[pid] = continuity_counter;
		}

		summary.aligned = true;
		return summary;
	}

	DatagramReorderBuffer::Placement DatagramReorderBuffer::Classify(const Summary &summary) const
	{
		bool has_tracked = false;
		bool all_exact	 = true;
		bool any_behind	 = false;

		for (uint16_t pid : summary.pid_order)
		{
			const auto it = _expected_cc.find(pid);
			if (it == _expected_cc.end())
			{
				// A PID we have never seen carries no ordering constraint on its own.
				continue;
			}

			has_tracked				   = true;
			const uint8_t displacement = Displacement(summary.first_cc.at(pid), it->second);
			if (displacement != 0)
			{
				all_exact = false;
			}
			if (displacement > AHEAD_LIMIT)
			{
				any_behind = true;
			}
		}

		if (has_tracked == false)
		{
			return Placement::Init;
		}
		if (any_behind)
		{
			return Placement::Stale;
		}
		if (all_exact)
		{
			return Placement::InOrder;
		}
		return Placement::Ahead;
	}

	void DatagramReorderBuffer::Deliver(const std::shared_ptr<const ov::Data> &data, const Summary &summary,
										std::vector<std::shared_ptr<const ov::Data>> *out)
	{
		out->push_back(data);
		for (uint16_t pid : summary.pid_order)
		{
			_expected_cc[pid] = static_cast<uint8_t>((summary.last_cc.at(pid) + 1) & 0x0F);
		}
	}

	void DatagramReorderBuffer::Cascade(std::vector<std::shared_ptr<const ov::Data>> *out)
	{
		bool progress = true;
		while (progress)
		{
			progress = false;

			for (auto it = _buffer.begin(); it != _buffer.end();)
			{
				const Placement placement = Classify(it->summary);
				if (placement == Placement::InOrder)
				{
					Deliver(it->data, it->summary, out);
					it		 = _buffer.erase(it);
					progress = true;
				}
				else if (placement == Placement::Stale)
				{
					// Expected advanced past it. It is NOT necessarily a duplicate:
					// with multi-packet datagrams the 4-bit counter can make a still-needed datagram look behind after
					// a flush picked a different victim.
					// Never erase it (that would silently drop a received datagram); pass it through out of order
					// without advancing the baseline and let the downstream continuity check handle any residual disorder.
					out->push_back(it->data);
					it		 = _buffer.erase(it);
					progress = true;
				}
				else
				{
					++it;
				}
			}
		}
	}

	std::deque<DatagramReorderBuffer::Pending>::iterator DatagramReorderBuffer::SelectClosest()
	{
		// Rank each buffered datagram by its smallest per-PID displacement.
		// Across datagrams with disjoint PID sets this compares different counters,
		// so it is a best-effort pick, not a total order - but it only runs on the flush (loss) path,
		// and Cascade never drops the losers (it passes them through),
		// so at worst it adds some out-of-order at a loss boundary.
		auto best		  = _buffer.begin();

		uint8_t best_rank = 0xFF;
		for (auto it = _buffer.begin(); it != _buffer.end(); ++it)
		{
			uint8_t rank = 0xFF;
			for (uint16_t pid : it->summary.pid_order)
			{
				const auto expected = _expected_cc.find(pid);
				if (expected == _expected_cc.end())
				{
					continue;
				}

				const uint8_t displacement = Displacement(it->summary.first_cc.at(pid), expected->second);
				if (displacement < rank)
				{
					rank = displacement;
				}
			}

			if (rank < best_rank)
			{
				best_rank = rank;
				best	  = it;
			}
		}
		return best;
	}

	size_t DatagramReorderBuffer::FlushIfNeeded(std::vector<std::shared_ptr<const ov::Data>> *out, int64_t now)
	{
		// The head gap is presumed lost once the buffer is full (depth) or the oldest arrival has
		// waited too long (timeout, evaluated on each arrival - there is no timer thread).
		// Force the datagram nearest the expected counters out (declaring the gap before it lost) and cascade.
		size_t declared_lost = 0;

		while ((_buffer.empty() == false) &&
			   ((_buffer.size() >= _max_datagrams) ||
				((now - _buffer.front().arrival_msec) >= _timeout_msec)))
		{
			auto victim	   = SelectClosest();

			Pending picked = *victim;

			_buffer.erase(victim);

			Deliver(picked.data, picked.summary, out);
			Cascade(out);

			declared_lost++;
		}
		return declared_lost;
	}

	void DatagramReorderBuffer::DrainInOrder(std::vector<std::shared_ptr<const ov::Data>> *out)
	{
		Cascade(out);

		while (_buffer.empty() == false)
		{
			auto victim	   = SelectClosest();
			Pending picked = *victim;
			_buffer.erase(victim);
			Deliver(picked.data, picked.summary, out);
			Cascade(out);
		}
	}

	void DatagramReorderBuffer::Rebase(const Summary &summary)
	{
		for (uint16_t pid : summary.pid_order)
		{
			_expected_cc[pid] = static_cast<uint8_t>((summary.last_cc.at(pid) + 1) & 0x0F);
		}
	}

	bool DatagramReorderBuffer::Resync(const std::shared_ptr<const ov::Data> &data, const Summary &summary,
									   std::vector<std::shared_ptr<const ov::Data>> *out, EnqueueResult *result)
	{
		const auto shares_pid = std::any_of(_buffer.begin(), _buffer.end(), [&summary](const Pending &pending) {
			return std::any_of(summary.pid_order.begin(), summary.pid_order.end(), [&pending](uint16_t pid) {
				return pending.summary.first_cc.find(pid) != pending.summary.first_cc.end();
			});
		});

		if (shares_pid == false)
		{
			return false;
		}

		for (uint16_t pid : summary.pid_order)
		{
			const auto expected = _expected_cc.find(pid);
			if ((expected != _expected_cc.end()) && (Displacement(summary.first_cc.at(pid), expected->second) > AHEAD_LIMIT))
			{
				result->resync_pids.push_back({pid, summary.first_cc.at(pid), expected->second});
			}
		}

		result->resynced	= true;
		result->resync_held = _buffer.size();

		for (const auto &pending : _buffer)
		{
			Deliver(pending.data, pending.summary, out);
		}
		_buffer.clear();

		Deliver(data, summary, out);
		return true;
	}

	std::optional<uint64_t> DatagramReorderBuffer::ArrivalKey(const std::shared_ptr<const ov::Data> &datagram)
	{
		const uint8_t *data = datagram->GetDataAs<uint8_t>();
		const size_t length = datagram->GetLength();

		auto is_null_packet = [data](size_t offset) { return PacketPid(data + offset) == 0x1FFF; };

		size_t first		= 0;
		while ((first < length) && is_null_packet(first))
		{
			first += MPEGTS_MIN_PACKET_SIZE;
		}
		if (first >= length)
		{
			return std::nullopt;
		}

		size_t last = length - MPEGTS_MIN_PACKET_SIZE;
		while (is_null_packet(last))
		{
			last -= MPEGTS_MIN_PACKET_SIZE;
		}

		const std::hash<std::string_view> hash;
		auto packet_hash = [&hash, data](size_t offset) {
			return hash(std::string_view(reinterpret_cast<const char *>(data + offset), MPEGTS_MIN_PACKET_SIZE));
		};
		const uint64_t first_hash = packet_hash(first);
		const uint64_t last_hash  = (last == first) ? first_hash : packet_hash(last);

		return (first_hash * 0x9E3779B97F4A7C15ULL) ^ last_hash ^ (static_cast<uint64_t>(length) * 0xC2B2AE3D27D4EB4FULL);
	}

	bool DatagramReorderBuffer::RecordArrival(const std::shared_ptr<const ov::Data> &datagram, int64_t now)
	{
		if ((_recent_arrivals.empty() == false) && (now < _recent_arrivals.back().first))
		{
			_recent_arrivals.clear();
			_recent_keys.clear();
		}

		while ((_recent_arrivals.empty() == false) && ((now - _recent_arrivals.front().first) > DUPLICATE_WINDOW_MSEC))
		{
			const auto expired = _recent_keys.find(_recent_arrivals.front().second);
			if (--expired->second == 0)
			{
				_recent_keys.erase(expired);
			}
			_recent_arrivals.pop_front();
		}

		const auto key = ArrivalKey(datagram);
		if (key.has_value() == false)
		{
			return false;
		}

		_recent_arrivals.emplace_back(now, key.value());
		return (_recent_keys[key.value()]++) > 0;
	}

	DatagramReorderBuffer::EnqueueResult DatagramReorderBuffer::Enqueue(const std::shared_ptr<const ov::Data> &datagram,
																		std::vector<std::shared_ptr<const ov::Data>> *out)
	{
		EnqueueResult result;
		const int64_t now	  = Now();
		const Summary summary = Parse(datagram);

		if ((summary.aligned == false) || summary.discontinuity)
		{
			// Misaligned or a signalled discontinuity: ordering cannot be trusted.
			// Drain what we have in restored order and pass this datagram through.
			DrainInOrder(out);
			out->push_back(datagram);

			if (summary.pid_order.empty())
			{
				// Untrusted with no usable PID (e.g. misaligned): drop all baselines
				// so they are re-established on the next clean datagram.
				_expected_cc.clear();
			}
			else
			{
				// A signalled discontinuity that still carries PIDs: continuity resumes from it.
				Rebase(summary);
			}
			return result;
		}

		if (summary.pid_order.empty())
		{
			// Aligned but carries no payload PID (e.g. a PCR-only or null-padding datagram):
			// it holds no continuity information, so pass it straight through WITHOUT disturbing sequencing state.
			out->push_back(datagram);
			result.declared_lost = FlushIfNeeded(out, now);
			return result;
		}

		const bool recent_copy = RecordArrival(datagram, now);

		switch (Classify(summary))
		{
			case Placement::Init:
			case Placement::InOrder:
				// First datagram / all tracked PIDs continue exactly: deliver and cascade.
				Deliver(datagram, summary, out);
				Cascade(out);
				break;

			case Placement::Ahead:
				// A gap on at least one PID with none behind: buffer until the gap fills.
				// A duplicate of an already-buffered datagram is intentionally NOT deduped here:
				// with multi-packet datagrams a counter match is not proof of duplication
				// (a genuinely different datagram aliases to the same counter every 16 packets),
				// so dropping on a match could lose real data.
				// If a real duplicate is buffered, Cascade's Stale path passes it through out of
				// order (never drops), and the downstream continuity check absorbs it.
				_buffer.push_back({datagram, summary, now});
				break;

			case Placement::Stale:
				// Behind on some PID: a gap that will not fill (loss or source switch), or a late or duplicate datagram.
				// Resync when a held datagram shares a PID with it; otherwise the stream continues from it, so rebase.
				// A recent copy is a network duplicate: pass it through and keep the baseline.
				if (Resync(datagram, summary, out, &result) == false)
				{
					if (recent_copy)
					{
						out->push_back(datagram);
					}
					else
					{
						// No held datagram shares its PIDs, so none can follow it: nothing to cascade.
						Deliver(datagram, summary, out);
					}
				}
				break;
		}

		result.declared_lost = FlushIfNeeded(out, now);
		return result;
	}

	void DatagramReorderBuffer::Flush(std::vector<std::shared_ptr<const ov::Data>> *out)
	{
		// Teardown drain: emit everything still buffered in restored (oldest-first) order.
		DrainInOrder(out);
	}
}  // namespace mpegts
