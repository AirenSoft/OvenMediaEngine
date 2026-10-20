//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mpegts
{
	// Best-effort reordering of whole UDP datagrams by their per-PID continuity counters (CCs).
	// 4-bit CCs wrap quickly, so in practice only adjacent datagram swaps are restored.
	//
	// 1. In order: delivered. Ahead: held until the gap fills or the depth/timeout limit declares it lost.
	// 2. Stale (behind on a PID): if a held datagram shares a PID, the held ones go out in arrival order
	//    (their CC order is no longer trusted) and the stale one becomes the baseline (resync).
	//    Otherwise it is delivered and becomes the baseline, unless it is a recent copy (`DUPLICATE_WINDOW_MSEC`).
	// 3. A datagram that cannot be placed passes through; every datagram is emitted exactly once.
	//
	// Not thread-safe; the caller serializes access.
	class DatagramReorderBuffer
	{
	public:
		// Maximum datagrams held before the head gap is declared lost and flushed.
		static constexpr size_t DEFAULT_MAX_DATAGRAMS  = 8;

		// Maximum time a datagram waits behind a gap before the gap is declared lost (milliseconds).
		static constexpr int64_t DEFAULT_TIMEOUT_MSEC  = 100;

		// Continuity-counter displacement (mod 16) at or below which a PID is "ahead" (a plausible future).
		// A larger displacement makes the PID "behind" and the datagram stale.
		static constexpr uint8_t AHEAD_LIMIT		   = 7;

		// A stale datagram matching an arrival within this window (milliseconds) is a network duplicate:
		// unless it resyncs, it is passed through without becoming the baseline.
		// About half the shortest interval at which ffmpeg output repeats a datagram key (491 ms).
		static constexpr int64_t DUPLICATE_WINDOW_MSEC = 250;

	public:
		// A PID that made a datagram stale when it triggered a resync.
		struct ResyncPid
		{
			uint16_t pid		= 0;
			uint8_t received_cc = 0;
			uint8_t expected_cc = 0;
		};

		// What one `Enqueue()` call did besides emitting datagrams, so the caller can log each event.
		struct EnqueueResult
		{
			// Gaps declared lost by a depth/timeout flush
			size_t declared_lost = 0;
			// Whether a stale arrival caused a resync; a resync is not a declared loss
			bool resynced		 = false;
			// Held datagrams the resync emitted
			size_t resync_held	 = 0;
			// The behind PIDs of the stale datagram that triggered the resync
			std::vector<ResyncPid> resync_pids;
		};

	public:
		// `clock` returns the current time in milliseconds; `nullptr` uses the system clock.
		// Injecting a clock keeps timeout-driven behavior deterministic in tests.
		explicit DatagramReorderBuffer(size_t max_datagrams			  = DEFAULT_MAX_DATAGRAMS,
									   int64_t timeout_msec			  = DEFAULT_TIMEOUT_MSEC,
									   std::function<int64_t()> clock = nullptr);

		// Feeds one whole datagram and appends the datagrams that become deliverable to `out`, in processing order.
		// Returns the gaps declared lost (depth/timeout flush) and the resync this call made, reported separately.
		EnqueueResult Enqueue(const std::shared_ptr<const ov::Data> &datagram,
							  std::vector<std::shared_ptr<const ov::Data>> *out);

		// Emits everything still buffered (in restored order). Available for an explicit teardown drain;
		// it is not called automatically on destruction, so if a stream ends while datagrams are held behind a gap,
		// up to (`max_datagrams - 1`) already-received datagrams are dropped at end-of-stream.
		// That is bounded and only affects shutdown.
		void Flush(std::vector<std::shared_ptr<const ov::Data>> *out);

		size_t GetBufferedCount() const
		{
			return _buffer.size();
		}

	private:
		// Per-datagram continuity-counter summary extracted by a light header-only parse.
		struct Summary
		{
			// starts on a 188 boundary and every sync byte is present
			bool aligned	   = false;
			// a packet signals discontinuity_indicator
			bool discontinuity = false;
			// payload-bearing PIDs in packet order
			std::vector<uint16_t> pid_order;
			std::map<uint16_t, uint8_t> first_cc;
			std::map<uint16_t, uint8_t> last_cc;
		};

		struct Pending
		{
			std::shared_ptr<const ov::Data> data;
			Summary summary;
			int64_t arrival_msec = 0;
		};

		enum class Placement
		{
			// no PID is tracked yet (stream start/all-new PIDs)
			Init,
			// every tracked PID continues exactly
			InOrder,
			// every tracked PID is ahead (a gap); none behind
			Ahead,
			// at least one tracked PID is behind (a late or duplicate datagram, or a stale baseline)
			Stale,
		};

	private:
		static Summary Parse(const std::shared_ptr<const ov::Data> &datagram);
		static uint8_t Displacement(uint8_t cc, uint8_t expected);
		static uint16_t PacketPid(const uint8_t *packet);

		int64_t Now() const;
		Placement Classify(const Summary &summary) const;

		// Delivers a datagram and advances the expected counter for every PID it carries.
		void Deliver(const std::shared_ptr<const ov::Data> &data, const Summary &summary,
					 std::vector<std::shared_ptr<const ov::Data>> *out);

		void Cascade(std::vector<std::shared_ptr<const ov::Data>> *out);

		// The buffered datagram nearest the expected counters (smallest displacement over its tracked
		// PIDs); the best guess for the datagram right after a lost head gap.
		std::deque<Pending>::iterator SelectClosest();

		// Returns the number of gaps declared lost (datagrams force-flushed) during this call.
		size_t FlushIfNeeded(std::vector<std::shared_ptr<const ov::Data>> *out, int64_t now);

		// Drains the whole buffer in restored (oldest-first) order.
		void DrainInOrder(std::vector<std::shared_ptr<const ov::Data>> *out);

		// Re-seeds the expected counters for the PIDs a datagram carries (used after a discontinuity).
		void Rebase(const Summary &summary);

		// Resyncs on a stale datagram when a held datagram shares one of its PIDs, and returns false otherwise.
		// A resync emits the held datagrams in arrival order, then delivers the stale datagram.
		bool Resync(const std::shared_ptr<const ov::Data> &data, const Summary &summary,
					std::vector<std::shared_ptr<const ov::Data>> *out, EnqueueResult *result);

		// Identifies an aligned datagram by its length and hashes of its first and last non-null TS packets.
		// Returns no key when every packet is a null packet.
		// Two different datagrams can share a key; a collision only makes one stale datagram skip its rebase.
		static std::optional<uint64_t> ArrivalKey(const std::shared_ptr<const ov::Data> &datagram);

		// Records an arrival and returns whether a datagram with the same key arrived within `DUPLICATE_WINDOW_MSEC`.
		// A clock that goes backward clears the recorded arrivals.
		bool RecordArrival(const std::shared_ptr<const ov::Data> &datagram, int64_t now);

	private:
		size_t _max_datagrams;
		int64_t _timeout_msec;
		std::function<int64_t()> _clock;

		// Expected next continuity counter per PID (the joint sequencing state).
		std::map<uint16_t, uint8_t> _expected_cc;

		std::deque<Pending> _buffer;

		// Arrivals within the last `DUPLICATE_WINDOW_MSEC` as (arrival time, key), oldest first.
		std::deque<std::pair<int64_t, uint64_t>> _recent_arrivals;
		// Number of `_recent_arrivals` entries per key.
		std::unordered_map<uint64_t, uint32_t> _recent_keys;
	};
}  // namespace mpegts
