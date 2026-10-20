//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "mpegts_datagram_reorder_buffer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>

namespace
{
	constexpr uint16_t VIDEO_PID = 0x0100;
	constexpr uint16_t AUDIO_PID = 0x0101;

	// Builds a datagram (a contiguous run of 188-byte TS packets) from a list of (pid, cc) pairs.
	// `payload_fill` fills the payload of payload-only packets, so different senders produce different bytes.
	std::shared_ptr<const ov::Data> MakeDatagram(const std::vector<std::pair<uint16_t, uint8_t>> &packets,
												 bool discontinuity = false, bool transport_error = false, uint8_t payload_fill = 0x00)
	{
		std::vector<uint8_t> buffer(packets.size() * 188, 0x00);

		for (size_t i = 0; i < packets.size(); i++)
		{
			uint8_t *p		   = buffer.data() + (i * 188);
			const uint16_t pid = packets[i].first;
			const uint8_t cc   = packets[i].second;

			p[0]			   = 0x47;
			p[1]			   = static_cast<uint8_t>((transport_error ? 0x80 : 0x00) | ((pid >> 8) & 0x1F));
			p[2]			   = static_cast<uint8_t>(pid & 0xFF);

			if (discontinuity)
			{
				// Adaptation field + payload, with the discontinuity_indicator set.
				p[3] = static_cast<uint8_t>((0b11 << 4) | (cc & 0x0F));
				p[4] = 1;	  // adaptation_field_length
				p[5] = 0x80;  // discontinuity_indicator
			}
			else
			{
				// Payload only.
				p[3] = static_cast<uint8_t>((0b01 << 4) | (cc & 0x0F));
				std::fill(p + 4, p + 188, payload_fill);
			}
		}

		return std::make_shared<ov::Data>(buffer.data(), buffer.size());
	}

	// A single-PID datagram carrying one packet with the given continuity counter.
	std::shared_ptr<const ov::Data> MakeVideo(uint8_t cc)
	{
		return MakeDatagram({{VIDEO_PID, cc}});
	}

	// A datagram carrying `count` consecutive video packets starting at continuity counter `base_cc`
	// (realistic framing: real UDP datagrams carry several TS packets, so the counter steps by `count`
	// per datagram).
	std::shared_ptr<const ov::Data> MakeVideoRun(uint8_t base_cc, int count)
	{
		std::vector<std::pair<uint16_t, uint8_t>> packets;
		for (int i = 0; i < count; i++)
		{
			packets.push_back({VIDEO_PID, static_cast<uint8_t>((base_cc + i) & 0x0F)});
		}
		return MakeDatagram(packets);
	}

	// Drives the buffer and maps delivered datagrams back to their logical index via pointer identity.
	class Harness
	{
	public:
		explicit Harness(mpegts::DatagramReorderBuffer *buffer)
			: _buffer(buffer)
		{
		}

		// Registers a datagram under a logical index and feeds it in one step.
		std::vector<int> Feed(int index, const std::shared_ptr<const ov::Data> &datagram)
		{
			_by_pointer[datagram.get()] = index;

			std::vector<std::shared_ptr<const ov::Data>> out;
			_last_result = _buffer->Enqueue(datagram, &out);
			_resyncs += _last_result.resynced ? 1 : 0;
			_declared_lost += _last_result.declared_lost;
			return ToIndices(out);
		}

		const mpegts::DatagramReorderBuffer::EnqueueResult &LastResult() const
		{
			return _last_result;
		}

		size_t TotalResyncs() const
		{
			return _resyncs;
		}

		size_t TotalDeclaredLost() const
		{
			return _declared_lost;
		}

		std::vector<int> FlushRemaining()
		{
			std::vector<std::shared_ptr<const ov::Data>> out;
			_buffer->Flush(&out);
			return ToIndices(out);
		}

	private:
		std::vector<int> ToIndices(const std::vector<std::shared_ptr<const ov::Data>> &out)
		{
			std::vector<int> indices;
			for (const auto &d : out)
			{
				auto it = _by_pointer.find(d.get());
				indices.push_back(it == _by_pointer.end() ? -1 : it->second);
			}
			return indices;
		}

		mpegts::DatagramReorderBuffer *_buffer;
		std::map<const ov::Data *, int> _by_pointer;
		mpegts::DatagramReorderBuffer::EnqueueResult _last_result;
		size_t _resyncs		  = 0;
		size_t _declared_lost = 0;
	};
}  // namespace

TEST(MpegTsDatagramReorderBuffer, InOrderPassthroughHasNoLatency)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	EXPECT_EQ(h.Feed(0, MakeVideo(0)), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(1, MakeVideo(1)), (std::vector<int>{1}));
	EXPECT_EQ(h.Feed(2, MakeVideo(2)), (std::vector<int>{2}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, AdjacentSwapIsRestored)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));  // in order
	append(h.Feed(2, MakeVideo(2)));  // ahead -> buffered
	append(h.Feed(1, MakeVideo(1)));  // fills the gap -> 1 then cascade 2
	append(h.Feed(3, MakeVideo(3)));

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 3}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, MultiStepReorderCascades)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	// Arrival: 0, 3, 1, 2  ->  3 waits, then 1, then 2 fills and 3 cascades.
	append(h.Feed(0, MakeVideo(0)));
	append(h.Feed(3, MakeVideo(3)));
	append(h.Feed(1, MakeVideo(1)));
	append(h.Feed(2, MakeVideo(2)));

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 3}));
}

TEST(MpegTsDatagramReorderBuffer, DepthFlushDeclaresLoss)
{
	// Small depth so the lost datagram is declared quickly.
	mpegts::DatagramReorderBuffer buffer(/*max_datagrams=*/3, /*timeout_msec=*/1000000);
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));  // delivered, expected -> 1
	append(h.Feed(1, MakeVideo(2)));  // cc 1 lost; this is cc 2 -> buffered
	append(h.Feed(2, MakeVideo(3)));  // buffered
	append(h.Feed(3, MakeVideo(4)));  // buffer depth reaches 3 -> flush: emit 2,3,4 in order

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 3}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, TimeoutFlushDeclaresLoss)
{
	int64_t now = 0;
	mpegts::DatagramReorderBuffer buffer(/*max_datagrams=*/16, /*timeout_msec=*/100,
										 /*clock=*/[&now]() { return now; });
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));  // delivered
	append(h.Feed(1, MakeVideo(2)));  // cc 1 lost; cc 2 buffered at t=0
	EXPECT_EQ(buffer.GetBufferedCount(), 1u);

	now = 100;						  // advance past timeout
	append(h.Feed(2, MakeVideo(3)));  // triggers sweep: 2 flushed (loss of 1), then 3 cascades

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, DuplicateDatagramPassesThrough)
{
	// A fixed clock keeps the copy within the duplicate window however long the run takes.
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, []() { return static_cast<int64_t>(0); });
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));
	append(h.Feed(1, MakeVideo(1)));
	// A duplicate (behind expected and byte-identical to a recent arrival) is not dropped:
	// it is passed through without rebasing, and the downstream continuity check handles it.
	// The important property is that nothing received is silently discarded.
	append(h.Feed(2, MakeVideo(0)));

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, DuplicateInAheadWindowIsNeverDropped)
{
	// A duplicate of an already-buffered (ahead) datagram with a single high-rate PID (no low-rate
	// anchor to reveal it as stale) is not deduped in the buffer - deduping on a counter match could
	// drop genuinely different multi-packet data. Once the gap fills, the original is delivered in
	// order and the surplus copy becomes 'behind' and is passed through (never dropped). The
	// downstream continuity check absorbs the redundant copy.
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));   // delivered, expected -> 1
	append(h.Feed(2, MakeVideo(2)));   // ahead -> buffered
	append(h.Feed(99, MakeVideo(2)));  // surplus copy of cc2 -> also buffered
	append(h.Feed(1, MakeVideo(1)));   // fills the gap -> 1, cascade 2, surplus copy passed through

	std::sort(delivered.begin(), delivered.end());
	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 99}));	// every received datagram emitted; none dropped
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, StaleDuplicateRejectedByLowRatePid)
{
	// The joint multi-PID check rejects a stale/duplicate datagram that a high-rate PID alone would
	// alias as "ahead". With 6 video + 1 audio per datagram, video steps by 6 (wraps in ~3 datagrams)
	// but audio steps by 1, so a duplicate of a 2-back datagram shows video "ahead" yet audio
	// "behind" -> classified stale -> passed straight through (never buffered, never re-injected).
	auto run = [](uint8_t v_base, uint8_t a_cc) {
		std::vector<std::pair<uint16_t, uint8_t>> packets;
		for (int i = 0; i < 6; i++)
		{
			packets.push_back({VIDEO_PID, static_cast<uint8_t>((v_base + i) & 0x0F)});
		}
		packets.push_back({AUDIO_PID, a_cc});
		return MakeDatagram(packets);
	};

	// A fixed clock keeps the copy within the duplicate window however long the run takes.
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, []() { return static_cast<int64_t>(0); });
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, run(0, 0)));	// video 0-5,  audio 0
	append(h.Feed(1, run(6, 1)));	// video 6-11, audio 1
	append(h.Feed(2, run(12, 2)));	// video 12-15,0,1, audio 2

	// Duplicate of datagram 1 (video first_cc 6, audio 1). video displacement aliases into "ahead",
	// audio displacement is clearly "behind" -> stale -> passthrough, not buffered.
	append(h.Feed(99, run(6, 1)));

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 99}));	// duplicate passed through, in place
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, PcrOnlyDatagramPreservesReorderBaseline)
{
	// An aligned datagram with no payload PID (e.g. PCR-only / null padding) must pass through
	// without tearing down the sequencing state, so reordering keeps working around it.
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));  // primary = video, delivered, expected -> 1
	append(h.Feed(1, MakeVideo(2)));  // ahead -> buffered

	// A PCR-only datagram: one adaptation-field-only packet (AFC=10, no payload) on a PCR PID.
	std::vector<uint8_t> pcr(188, 0x00);
	pcr[0] = 0x47;
	pcr[1] = (0x0100 >> 8) & 0x1F;
	pcr[2] = 0x00;
	pcr[3] = static_cast<uint8_t>((0b10 << 4) | 0);							// adaptation only, no payload
	pcr[4] = 1;																// adaptation_field_length
	append(h.Feed(2, std::make_shared<ov::Data>(pcr.data(), pcr.size())));	// passthrough, no state change

	append(h.Feed(3, MakeVideo(1)));  // still fills the video gap -> 1 then cascade the buffered 2

	EXPECT_EQ(delivered, (std::vector<int>{0, 2, 3, 1}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, DiscontinuityBypassesAndRebases)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));
	append(h.Feed(1, MakeVideo(1)));
	// A signalled discontinuity with a far-off cc: passes through immediately and rebases.
	append(h.Feed(2, MakeDatagram({{VIDEO_PID, 9}}, /*discontinuity=*/true)));
	// Following datagrams reorder against the new base (cc 10, 11).
	append(h.Feed(3, MakeDatagram({{VIDEO_PID, 11}})));	 // ahead -> buffered
	append(h.Feed(4, MakeDatagram({{VIDEO_PID, 10}})));	 // fills -> 4 then cascade 3

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 4, 3}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, UnalignedDatagramPassesThrough)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	// Not a multiple of 188 -> cannot be trusted for CC ordering -> passed through as-is.
	std::vector<uint8_t> junk(200, 0x00);
	auto datagram = std::make_shared<ov::Data>(junk.data(), junk.size());

	EXPECT_EQ(h.Feed(0, datagram), (std::vector<int>{0}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, MultiPidInterleaveIsRestored)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	// Each datagram carries video and audio packets that move together.
	auto d0		= MakeDatagram({{VIDEO_PID, 0}, {AUDIO_PID, 5}});
	auto d1		= MakeDatagram({{VIDEO_PID, 1}, {AUDIO_PID, 6}});
	auto d2		= MakeDatagram({{VIDEO_PID, 2}, {AUDIO_PID, 7}});

	append(h.Feed(0, d0));
	append(h.Feed(2, d2));	// ahead -> buffered
	append(h.Feed(1, d1));	// fills -> 1 then cascade 2

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, NonPrimaryPidDatagramPassesThrough)
{
	// The primary (sequencing) PID is the first payload PID seen (video here). A datagram that does
	// not carry it (a separate audio-only datagram) is not reordered; it passes straight through in
	// arrival order while the primary PID is reordered independently.
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));					// primary = video, delivered, expected -> 1
	append(h.Feed(1, MakeVideo(2)));					// video ahead -> buffered
	append(h.Feed(2, MakeDatagram({{AUDIO_PID, 7}})));	// audio-only, no primary -> passthrough
	append(h.Feed(3, MakeVideo(1)));					// fills the video gap -> 1 then cascade 2

	EXPECT_EQ(delivered, (std::vector<int>{0, 2, 3, 1}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, FarAheadDatagramBecomesTheBaseline)
{
	// Documents the 4-bit continuity-counter limit: a datagram displaced beyond the ahead window is
	// ambiguous (far-ahead vs stale). It is delivered at once rather than held or dropped,
	// and the stream continues from it.
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	h.Feed(0, MakeVideo(0));									 // expected -> 1
	EXPECT_EQ(h.Feed(1, MakeVideo(10)), (std::vector<int>{1}));	 // displacement 9 (> ahead limit) -> delivered, expected -> 11
	EXPECT_EQ(h.Feed(2, MakeVideo(11)), (std::vector<int>{2}));	 // in order against the new baseline
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, AdjacentSwapRestoredWithMultiPacketDatagrams)
{
	// Realistic framing: 7 TS packets per datagram (the counter steps by 7 each datagram). An
	// adjacent datagram swap is still restored.
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	auto d0		= MakeVideoRun(0, 7);	// cc 0..6
	auto d1		= MakeVideoRun(7, 7);	// cc 7..13
	auto d2		= MakeVideoRun(14, 7);	// cc 14,15,0,1,2,3,4
	auto d3		= MakeVideoRun(5, 7);	// cc 5..11

	append(h.Feed(0, d0));
	append(h.Feed(2, d2));	// d1/d2 swapped in arrival
	append(h.Feed(1, d1));
	append(h.Feed(3, d3));

	EXPECT_EQ(delivered, (std::vector<int>{0, 1, 2, 3}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, DeepReorderResyncsWithoutLossMultiPacket)
{
	// With 7-packet datagrams a depth-2 reorder exceeds the 4-bit counter window and cannot be restored:
	// d3 arrives stale while d2 is held and resyncs, so the late d1 is delivered after them.
	// Every received datagram is still delivered (nothing is dropped).
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	auto d0		= MakeVideoRun(0, 7);
	auto d1		= MakeVideoRun(7, 7);
	auto d2		= MakeVideoRun(14, 7);
	auto d3		= MakeVideoRun(5, 7);

	append(h.Feed(0, d0));
	append(h.Feed(2, d2));
	append(h.Feed(3, d3));
	append(h.Feed(1, d1));
	append(h.FlushRemaining());

	EXPECT_EQ(delivered, (std::vector<int>{0, 2, 3, 1}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, BypassDrainsBufferedInRestoredOrder)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(0, MakeVideo(0)));  // delivered, expected -> 1
	append(h.Feed(3, MakeVideo(3)));  // buffered (arrival first)
	append(h.Feed(2, MakeVideo(2)));  // buffered (arrival second)

	// An unaligned datagram forces the buffer to drain. It must drain in restored order (2 then 3),
	// not arrival order (3 then 2), before the passthrough datagram.
	std::vector<uint8_t> junk(200, 0x00);
	append(h.Feed(9, std::make_shared<ov::Data>(junk.data(), junk.size())));

	EXPECT_EQ(delivered, (std::vector<int>{0, 2, 3, 9}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, CorruptAdaptationLengthBypasses)
{
	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	// AFC=11 (adaptation + payload) with an adaptation_field_length that exceeds the packet body.
	std::vector<uint8_t> bytes(188, 0x00);
	bytes[0]	  = 0x47;
	bytes[1]	  = (VIDEO_PID >> 8) & 0x1F;
	bytes[2]	  = VIDEO_PID & 0xFF;
	bytes[3]	  = static_cast<uint8_t>((0b11 << 4) | 0);
	bytes[4]	  = 200;  // invalid adaptation_field_length (> 183)
	auto datagram = std::make_shared<ov::Data>(bytes.data(), bytes.size());

	// Untrusted -> bypass reordering -> passed through unchanged.
	EXPECT_EQ(h.Feed(0, datagram), (std::vector<int>{0}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, MultiPacketFlushNeverDropsReceivedDatagram)
{
	// Regression: with multi-packet datagrams the 4-bit counter can make a timeout/depth flush pick a
	// victim that advances the baseline past a still-buffered datagram. That datagram must be passed
	// through, never silently dropped.
	int64_t now = 0;
	mpegts::DatagramReorderBuffer buffer(/*max_datagrams=*/8, /*timeout_msec=*/100,
										 [&now]() { return now; });
	Harness h(&buffer);

	std::vector<int> delivered;
	auto append = [&delivered](const std::vector<int> &v) { delivered.insert(delivered.end(), v.begin(), v.end()); };

	append(h.Feed(2, MakeVideoRun(6, 3)));	 // first delivered, expected -> 9
	append(h.Feed(0, MakeVideoRun(0, 3)));	 // displacement 7 -> buffered
	now = 100;								 // age past timeout
	append(h.Feed(5, MakeVideoRun(15, 3)));	 // displacement 6 -> buffered; timeout flush fires
	append(h.FlushRemaining());

	std::sort(delivered.begin(), delivered.end());
	EXPECT_EQ(delivered, (std::vector<int>{0, 2, 5}));	// every received datagram emitted; none dropped
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

// -----------------------------------------------------------------------------
// Property test: within-window shuffles must restore the original order exactly.
// Uses only the raw mt19937 engine output (no std distributions/std::shuffle) so
// the permutation is byte-for-byte reproducible on every platform and compiler.
// -----------------------------------------------------------------------------
TEST(MpegTsDatagramReorderBuffer, WithinWindowShufflePropertyRestoresOrder)
{
	constexpr int BLOCK = 6;   // reorder distance bound (<= ahead limit)
	constexpr int COUNT = 50;  // crosses several 4-bit continuity-counter wraps

	for (uint32_t seed = 1; seed <= 25; seed++)
	{
		std::mt19937 gen(seed);

		// Build a bounded-displacement permutation using only raw gen() outputs (Fisher-Yates within
		// fixed blocks). Every datagram stays inside its block, so its displacement is < BLOCK and
		// the continuity counters live in the buffer never span a full 4-bit wrap (no CC aliasing).
		// Index 0 is kept first so it establishes the reorder baseline (in a real stream the
		// first-arriving datagram is the baseline; there is no datagram "before" it).
		std::vector<int> feed_order(COUNT);
		for (int i = 0; i < COUNT; i++)
		{
			feed_order[i] = i;
		}
		for (int start = 1; start < COUNT; start += BLOCK)
		{
			const int end = std::min(start + BLOCK, COUNT);
			for (int i = end - 1; i > start; i--)
			{
				const int j = start + static_cast<int>(gen() % static_cast<uint32_t>(i - start + 1));
				std::swap(feed_order[i], feed_order[j]);
			}
		}

		// No loss and no time advance -> full restoration is required.
		mpegts::DatagramReorderBuffer buffer(/*max_datagrams=*/64, /*timeout_msec=*/1000000,
											 /*clock=*/[]() { return static_cast<int64_t>(0); });
		Harness h(&buffer);

		std::vector<int> delivered;
		for (int index : feed_order)
		{
			auto out = h.Feed(index, MakeVideo(static_cast<uint8_t>(index % 16)));
			delivered.insert(delivered.end(), out.begin(), out.end());
		}
		auto tail = h.FlushRemaining();
		delivered.insert(delivered.end(), tail.begin(), tail.end());

		std::vector<int> expected(COUNT);
		for (int i = 0; i < COUNT; i++)
		{
			expected[i] = i;
		}

		ASSERT_EQ(delivered, expected) << "seed=" << seed;
	}
}

// -----------------------------------------------------------------------------
// Stale datagrams: a resync when held datagrams share a PID, otherwise a rebase or a duplicate pass-through
// (7-packet datagrams, as ffmpeg sends them, mixed with audio and PSI datagrams).
// -----------------------------------------------------------------------------
namespace
{
	constexpr uint16_t PAT_PID	= 0x0000;
	constexpr uint16_t NULL_PID = 0x1FFF;
	constexpr uint16_t PMT_PID	= 0x1000;

	using Layout				= std::vector<std::pair<uint16_t, int>>;

	const Layout VIDEO_DATAGRAM = {{VIDEO_PID, 7}};
	const Layout AUDIO_DATAGRAM = {{AUDIO_PID, 1}};
	const Layout PSI_DATAGRAM	= {{PAT_PID, 1}, {PMT_PID, 1}, {VIDEO_PID, 5}};

	// One sender: builds consecutive datagrams with a running continuity counter per PID (all start at 0).
	// Senders with a different `payload_fill` never produce byte-identical datagrams.
	class Sender
	{
	public:
		explicit Sender(uint8_t payload_fill = 0x00)
			: _payload_fill(payload_fill)
		{
		}

		std::shared_ptr<const ov::Data> Next(const Layout &layout)
		{
			std::vector<std::pair<uint16_t, uint8_t>> packets;
			for (const auto &[pid, count] : layout)
			{
				for (int i = 0; i < count; i++)
				{
					// Null packets are byte-identical in every datagram of one sender, as in CBR output.
					packets.push_back({pid, (pid == NULL_PID) ? static_cast<uint8_t>(0) : _cc[pid]});
					_cc[pid] = static_cast<uint8_t>((_cc[pid] + 1) & 0x0F);
				}
			}
			// Stamp a sequence number into every non-null packet, so datagrams of one sender are never byte-identical.
			const auto datagram = MakeDatagram(packets, false, false, _payload_fill);
			std::vector<uint8_t> bytes(datagram->GetDataAs<uint8_t>(), datagram->GetDataAs<uint8_t>() + datagram->GetLength());
			for (size_t i = 0; i < packets.size(); i++)
			{
				if (packets[i].first != NULL_PID)
				{
					bytes[(i * 188) + 4] = static_cast<uint8_t>(_sequence & 0xFF);
					bytes[(i * 188) + 5] = static_cast<uint8_t>(_sequence >> 8);
				}
			}
			_sequence++;
			return std::make_shared<ov::Data>(bytes.data(), bytes.size());
		}

		// Moves the continuity counter of `pid` forward, as a muxer that renumbers one PID would.
		void Skip(uint16_t pid, uint8_t count)
		{
			_cc[pid] = static_cast<uint8_t>((_cc[pid] + count) & 0x0F);
		}

	private:
		uint8_t _payload_fill;
		uint16_t _sequence = 0;
		std::map<uint16_t, uint8_t> _cc;
	};

	// Counts packets whose continuity counter does not follow the previous packet of the same PID.
	int CountBreaks(const std::vector<std::shared_ptr<const ov::Data>> &datagrams, const std::vector<int> &order)
	{
		int breaks = 0;
		std::map<uint16_t, uint8_t> last;
		for (int index : order)
		{
			const auto *p = datagrams[index]->GetDataAs<uint8_t>();
			for (size_t offset = 0; offset < datagrams[index]->GetLength(); offset += 188)
			{
				const uint16_t pid = static_cast<uint16_t>(((p[offset + 1] & 0x1F) << 8) | p[offset + 2]);
				const uint8_t cc   = p[offset + 3] & 0x0F;
				const auto it	   = last.find(pid);
				if ((it != last.end()) && (cc != ((it->second + 1) & 0x0F)))
				{
					breaks++;
				}
				last[pid] = cc;
			}
		}
		return breaks;
	}

	// Feeds `arrival` (indices into `datagrams`) one by one and returns everything emitted, Flush included.
	std::vector<int> FeedAll(Harness *h, const std::vector<std::shared_ptr<const ov::Data>> &datagrams, const std::vector<int> &arrival)
	{
		std::vector<int> delivered;
		for (int index : arrival)
		{
			const auto out = h->Feed(index, datagrams[index]);
			delivered.insert(delivered.end(), out.begin(), out.end());
		}
		const auto tail = h->FlushRemaining();
		delivered.insert(delivered.end(), tail.begin(), tail.end());
		return delivered;
	}

	std::vector<int> Without(int count, const std::vector<int> &lost)
	{
		std::vector<int> order;
		for (int i = 0; i < count; i++)
		{
			if (std::find(lost.begin(), lost.end(), i) == lost.end())
			{
				order.push_back(i);
			}
		}
		return order;
	}
}  // namespace

TEST(MpegTsDatagramReorderBuffer, SingleLostVideoDatagramResyncsWithoutAmplifying)
{
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 10; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}

	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	// d1 is lost: d2 is held as ahead, d3 is stale against the expected cc 7 and resyncs.
	EXPECT_EQ(h.Feed(0, d[0]), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(2, d[2]), (std::vector<int>{}));
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{2, 3}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
	EXPECT_TRUE(h.LastResult().resynced);
	EXPECT_EQ(h.LastResult().declared_lost, 0u);

	// The baseline follows d3, so d4 is in order at once.
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{4}));

	std::vector<int> delivered = {0, 2, 3, 4};
	for (int i = 5; i < 10; i++)
	{
		const auto out = h.Feed(i, d[i]);
		delivered.insert(delivered.end(), out.begin(), out.end());
	}

	const auto arrival = Without(10, {1});
	EXPECT_EQ(delivered, arrival);
	EXPECT_EQ(CountBreaks(d, delivered), CountBreaks(d, arrival));
	EXPECT_EQ(CountBreaks(d, delivered), 1);
	EXPECT_EQ(h.TotalResyncs(), 1u);
}

TEST(MpegTsDatagramReorderBuffer, ConsecutiveLossDoesNotAmplify)
{
	for (int run = 2; run <= 6; run++)
	{
		Sender sender;
		std::vector<std::shared_ptr<const ov::Data>> d;
		for (int i = 0; i < 20; i++)
		{
			d.push_back(sender.Next(VIDEO_DATAGRAM));
		}

		std::vector<int> lost;
		for (int i = 1; i <= run; i++)
		{
			lost.push_back(i);
		}
		const auto arrival = Without(20, lost);

		mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS, /*timeout_msec=*/1000000);
		Harness h(&buffer);
		const auto delivered = FeedAll(&h, d, arrival);

		EXPECT_EQ(delivered, arrival) << "run=" << run;
		EXPECT_EQ(CountBreaks(d, delivered), 1) << "run=" << run;
		EXPECT_EQ(h.TotalDeclaredLost(), 0u) << "run=" << run;
	}
}

TEST(MpegTsDatagramReorderBuffer, LostPsiDatagramRebasesLowRatePids)
{
	// Every third datagram carries PAT + PMT + video. Losing one opens a gap on PAT/PMT that never fills.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 24; i++)
	{
		d.push_back(sender.Next(((i % 3) == 1) ? PSI_DATAGRAM : VIDEO_DATAGRAM));
	}

	const auto arrival = Without(24, {7});

	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS, /*timeout_msec=*/1000000);
	Harness h(&buffer);
	std::vector<int> delivered;
	for (int index : arrival)
	{
		const auto out = h.Feed(index, d[index]);
		delivered.insert(delivered.end(), out.begin(), out.end());
	}

	// Resolved by resync, not by a depth/timeout flush, and the later PSI datagrams are no longer held.
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
	EXPECT_EQ(delivered, arrival);
	EXPECT_EQ(CountBreaks(d, delivered), CountBreaks(d, arrival));
	EXPECT_EQ(h.TotalDeclaredLost(), 0u);
	EXPECT_GE(h.TotalResyncs(), 1u);
	EXPECT_EQ(h.Feed(24, sender.Next(PSI_DATAGRAM)), (std::vector<int>{24}));
}

TEST(MpegTsDatagramReorderBuffer, DuplicateOfDeliveredDatagramWhileHeldResyncsBackward)
{
	// d2 and d3 are swapped and a duplicate of the delivered d1 arrives between them.
	// The duplicate is stale and shares the video PID with the held d3, so it resyncs.
	// d3 is emitted and the baseline moves back to the duplicate, so the swap is not restored.
	// The output is the arrival order, as without reordering.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 5; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}
	d.push_back(std::make_shared<ov::Data>(d[1]->GetData(), d[1]->GetLength()));  // index 5: duplicate of d1

	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	EXPECT_EQ(h.Feed(0, d[0]), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(1, d[1]), (std::vector<int>{1}));
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{}));
	EXPECT_EQ(h.Feed(5, d[5]), (std::vector<int>{3, 5}));
	EXPECT_TRUE(h.LastResult().resynced);
	EXPECT_EQ(h.Feed(2, d[2]), (std::vector<int>{2}));
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{}));
	EXPECT_EQ(h.FlushRemaining(), (std::vector<int>{4}));

	const std::vector<int> arrival = {0, 1, 3, 5, 2, 4};
	EXPECT_EQ(CountBreaks(d, arrival), 3);
}

TEST(MpegTsDatagramReorderBuffer, SourceSwitchRestartingEveryCounterRecoversAtOnce)
{
	// A new sender takes over without a discontinuity_indicator and its counters restart from cc 0.
	// n0 is stale with nothing held, so the stream continues from it: n0 becomes the baseline and nothing is held.
	Sender old_sender;
	Sender new_sender(0x5A);
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 3; i++)
	{
		d.push_back(old_sender.Next(VIDEO_DATAGRAM));  // d0..d2: cc 0..20
	}
	for (int i = 0; i < 6; i++)
	{
		d.push_back(new_sender.Next(VIDEO_DATAGRAM));  // n0..n5: cc 0..41
	}

	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	for (int i = 0; i < static_cast<int>(d.size()); i++)
	{
		EXPECT_EQ(h.Feed(i, d[i]), (std::vector<int>{i})) << "index=" << i;
		EXPECT_EQ(buffer.GetBufferedCount(), 0u) << "index=" << i;
	}
	EXPECT_EQ(h.TotalResyncs(), 0u);
	EXPECT_EQ(CountBreaks(d, {0, 1, 2, 3, 4, 5, 6, 7, 8}), 1);
}

TEST(MpegTsDatagramReorderBuffer, SourceSwitchWithMixedPidsRebasesEveryPidAtOnce)
{
	// Same switch with PSI and audio datagrams. Each PID restarts behind its old baseline.
	// The first new-sender datagram carrying it is stale with nothing held, so it becomes the baseline for its PIDs.
	// Every datagram is delivered on arrival, with no resync and nothing held.
	Sender old_sender;
	Sender new_sender(0x5A);
	std::vector<std::shared_ptr<const ov::Data>> d;
	const std::vector<Layout> pattern = {PSI_DATAGRAM, VIDEO_DATAGRAM, AUDIO_DATAGRAM, VIDEO_DATAGRAM};
	for (int i = 0; i < 13; i++)
	{
		d.push_back(old_sender.Next(pattern[i % pattern.size()]));
	}
	for (int i = 0; i < 16; i++)
	{
		d.push_back(new_sender.Next(pattern[i % pattern.size()]));
	}

	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS, /*timeout_msec=*/1000000);
	Harness h(&buffer);

	for (int i = 0; i < static_cast<int>(d.size()); i++)
	{
		EXPECT_EQ(h.Feed(i, d[i]), (std::vector<int>{i})) << "index=" << i;
		EXPECT_EQ(buffer.GetBufferedCount(), 0u) << "index=" << i;
	}
	EXPECT_EQ(h.TotalResyncs(), 0u);
	EXPECT_EQ(h.TotalDeclaredLost(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, LateDatagramAfterResyncIsDeliveredAndBecomesTheBaseline)
{
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 6; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}

	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	EXPECT_EQ(h.Feed(0, d[0]), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(2, d[2]), (std::vector<int>{}));
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{2, 3}));  // resync

	// d1 arrives after the resync: stale with nothing held, so it is delivered and the baseline moves back to it.
	EXPECT_EQ(h.Feed(1, d[1]), (std::vector<int>{1}));
	EXPECT_FALSE(h.LastResult().resynced);

	// d4 is then stale against d1 with nothing held, so the baseline moves forward again without holding anything.
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{4}));
	EXPECT_FALSE(h.LastResult().resynced);
	EXPECT_EQ(h.Feed(5, d[5]), (std::vector<int>{5}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, RecentCopyWithNothingHeldPassesThroughWithoutRebasing)
{
	// A stale datagram byte-identical to a recent arrival is a network duplicate: it is passed through
	// and the baseline stays where it was.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 4; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}
	d.push_back(std::make_shared<ov::Data>(d[0]->GetData(), d[0]->GetLength()));  // index 4: duplicate of d0

	// A fixed clock keeps the copies within the duplicate window however long the run takes.
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, []() { return static_cast<int64_t>(0); });
	Harness h(&buffer);

	EXPECT_EQ(h.Feed(0, d[0]), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(1, d[1]), (std::vector<int>{1}));
	EXPECT_EQ(h.Feed(2, d[2]), (std::vector<int>{2}));
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{4}));
	EXPECT_FALSE(h.LastResult().resynced);
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{3}));	// the baseline still follows d2
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, StaleWithoutSharedPidDoesNotResync)
{
	// Video datagram 4 is held behind the swapped 3. A duplicate audio datagram is stale on the audio PID only,
	// which no held datagram carries, so it passes through and the swap is still restored.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 0
	d.push_back(sender.Next(AUDIO_DATAGRAM));  // 1
	d.push_back(sender.Next(AUDIO_DATAGRAM));  // 2
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 3
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 4

	// 5: duplicate of 1
	d.push_back(std::make_shared<ov::Data>(d[1]->GetData(), d[1]->GetLength()));

	// A fixed clock keeps the copies within the duplicate window however long the run takes.
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, []() { return static_cast<int64_t>(0); });
	Harness h(&buffer);

	EXPECT_EQ(h.Feed(0, d[0]), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(1, d[1]), (std::vector<int>{1}));
	EXPECT_EQ(h.Feed(2, d[2]), (std::vector<int>{2}));
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{}));
	EXPECT_EQ(h.Feed(5, d[5]), (std::vector<int>{5}));
	EXPECT_FALSE(h.LastResult().resynced);
	EXPECT_EQ(buffer.GetBufferedCount(), 1u);
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{3, 4}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, AudioDatagramDuringVideoLossKeepsResync)
{
	// Video datagram 2 is lost; an in-order audio datagram arrives while 3 is held and is delivered at once.
	// The next video datagram still resyncs.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 0
	d.push_back(sender.Next(AUDIO_DATAGRAM));  // 1
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 2 (lost)
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 3
	d.push_back(sender.Next(AUDIO_DATAGRAM));  // 4
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 5
	d.push_back(sender.Next(VIDEO_DATAGRAM));  // 6

	mpegts::DatagramReorderBuffer buffer;
	Harness h(&buffer);

	EXPECT_EQ(h.Feed(0, d[0]), (std::vector<int>{0}));
	EXPECT_EQ(h.Feed(1, d[1]), (std::vector<int>{1}));
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{}));
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{4}));
	EXPECT_EQ(h.Feed(5, d[5]), (std::vector<int>{3, 5}));
	EXPECT_TRUE(h.LastResult().resynced);
	EXPECT_EQ(h.Feed(6, d[6]), (std::vector<int>{6}));

	const std::vector<int> delivered = {0, 1, 4, 3, 5, 6};
	EXPECT_EQ(CountBreaks(d, delivered), CountBreaks(d, {0, 1, 3, 4, 5, 6}));
}

TEST(MpegTsDatagramReorderBuffer, AdjacentSwapsInMixedStreamAreRestoredWithoutResync)
{
	// Swaps one adjacent pair in every four datagrams of a stream that mixes video, audio and PSI datagrams.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	const std::vector<Layout> pattern = {PSI_DATAGRAM, VIDEO_DATAGRAM, VIDEO_DATAGRAM, AUDIO_DATAGRAM, VIDEO_DATAGRAM};
	for (int i = 0; i < 60; i++)
	{
		d.push_back(sender.Next(pattern[i % pattern.size()]));
	}

	std::vector<int> arrival;
	for (int i = 0; i < 60; i++)
	{
		arrival.push_back(i);
	}
	for (int i = 1; i + 1 < 60; i += 4)
	{
		std::swap(arrival[i], arrival[i + 1]);
	}

	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS, /*timeout_msec=*/1000000);
	Harness h(&buffer);
	const auto delivered = FeedAll(&h, d, arrival);

	EXPECT_EQ(CountBreaks(d, delivered), 0);
	EXPECT_EQ(h.TotalResyncs(), 0u);
	EXPECT_EQ(h.TotalDeclaredLost(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, LossAndResyncAreReportedSeparately)
{
	{
		// A depth flush declares the gap lost and is not a resync.
		mpegts::DatagramReorderBuffer buffer(/*max_datagrams=*/3, /*timeout_msec=*/1000000);
		Harness h(&buffer);
		h.Feed(0, MakeVideo(0));
		h.Feed(1, MakeVideo(2));
		h.Feed(2, MakeVideo(3));
		h.Feed(3, MakeVideo(4));
		EXPECT_EQ(h.LastResult().declared_lost, 1u);
		EXPECT_FALSE(h.LastResult().resynced);
		EXPECT_EQ(h.TotalDeclaredLost(), 1u);
		EXPECT_EQ(h.TotalResyncs(), 0u);
	}

	{
		// A resync is not a declared loss, and it names the behind PID with its received and expected counters.
		Sender sender;
		std::vector<std::shared_ptr<const ov::Data>> d;
		for (int i = 0; i < 4; i++)
		{
			d.push_back(sender.Next(VIDEO_DATAGRAM));
		}

		mpegts::DatagramReorderBuffer buffer;
		Harness h(&buffer);
		h.Feed(0, d[0]);
		h.Feed(2, d[2]);
		h.Feed(3, d[3]);

		const auto &result = h.LastResult();
		EXPECT_TRUE(result.resynced);
		EXPECT_EQ(result.declared_lost, 0u);
		EXPECT_EQ(result.resync_held, 1u);
		ASSERT_EQ(result.resync_pids.size(), 1u);
		EXPECT_EQ(result.resync_pids[0].pid, VIDEO_PID);
		EXPECT_EQ(result.resync_pids[0].received_cc, 5);  // d3 starts at cc 21 mod 16
		EXPECT_EQ(result.resync_pids[0].expected_cc, 7);  // d1 (cc 7) never arrived
		EXPECT_EQ(h.TotalDeclaredLost(), 0u);
	}
}

TEST(MpegTsDatagramReorderBuffer, RandomImpairmentsEmitEveryDatagramExactlyOnce)
{
	// Loss, duplication, adjacent swaps and deeper reorders on a mixed multi-PID stream:
	// every received datagram (each duplicate is a separate object) comes out exactly once.
	const std::vector<Layout> layouts = {VIDEO_DATAGRAM, VIDEO_DATAGRAM, VIDEO_DATAGRAM, VIDEO_DATAGRAM, AUDIO_DATAGRAM, PSI_DATAGRAM, {{VIDEO_PID, 6}, {AUDIO_PID, 1}}};

	for (uint32_t seed = 1; seed <= 20; seed++)
	{
		std::mt19937 gen(seed);
		auto chance = [&gen](uint32_t percent) { return (gen() % 100) < percent; };

		Sender sender;
		std::vector<std::shared_ptr<const ov::Data>> d;
		std::vector<int> arrival;
		for (int i = 0; i < 400; i++)
		{
			d.push_back(sender.Next(layouts[gen() % layouts.size()]));
			if (chance(2) == false)
			{
				arrival.push_back(i);
			}
		}

		const int originals = static_cast<int>(d.size());
		for (size_t i = 0; i < arrival.size(); i++)
		{
			if (chance(2))
			{
				const int source = arrival[i];
				d.push_back(std::make_shared<ov::Data>(d[source]->GetData(), d[source]->GetLength()));
				const size_t at = std::min(arrival.size(), i + 1 + (gen() % 10));
				arrival.insert(arrival.begin() + at, static_cast<int>(d.size()) - 1);
			}
		}
		for (size_t i = 0; i + 1 < arrival.size(); i++)
		{
			if (chance(5))
			{
				const size_t depth = 1 + (gen() % 4);
				const int moved	   = arrival[i];
				arrival.erase(arrival.begin() + i);
				arrival.insert(arrival.begin() + std::min(arrival.size(), i + depth), moved);
			}
		}

		int64_t now = 0;
		mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
											 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC,
											 [&now]() { return now; });
		Harness h(&buffer);
		std::vector<int> delivered;
		for (int index : arrival)
		{
			now += static_cast<int64_t>(gen() % 30);
			const auto out = h.Feed(index, d[index]);
			delivered.insert(delivered.end(), out.begin(), out.end());
		}
		const auto tail = h.FlushRemaining();
		delivered.insert(delivered.end(), tail.begin(), tail.end());

		auto sorted_arrival = arrival;
		std::sort(sorted_arrival.begin(), sorted_arrival.end());
		std::sort(delivered.begin(), delivered.end());
		ASSERT_EQ(delivered, sorted_arrival) << "seed=" << seed;
		ASSERT_GT(static_cast<int>(d.size()), originals) << "seed=" << seed;
	}
}

// -----------------------------------------------------------------------------
// A stale datagram that does not resync becomes the baseline, unless it is a recent copy.
// A recent copy matches an arrival within `DUPLICATE_WINDOW_MSEC` by length and first and last non-null TS packets.
// -----------------------------------------------------------------------------
TEST(MpegTsDatagramReorderBuffer, LateBurstIsNeverWorseThanArrivalOrder)
{
	// A run of 3 to 5 datagrams arrives 1 to 5 datagrams late, at every start position.
	// The late run continues from its own first datagram, so it is never held behind the stale baseline.
	// The third pattern starts every datagram with a byte-identical null packet, as CBR output does.
	const std::vector<std::vector<Layout>> patterns = {
		{VIDEO_DATAGRAM},
		{PSI_DATAGRAM, VIDEO_DATAGRAM, VIDEO_DATAGRAM, AUDIO_DATAGRAM, VIDEO_DATAGRAM},
		{{{NULL_PID, 1}, {VIDEO_PID, 6}}},
	};

	for (size_t p = 0; p < patterns.size(); p++)
	{
		Sender sender;
		std::vector<std::shared_ptr<const ov::Data>> d;
		for (int i = 0; i < 48; i++)
		{
			d.push_back(sender.Next(patterns[p][i % patterns[p].size()]));
		}

		for (int start = 4; start < 32; start++)
		{
			for (int length = 3; length <= 5; length++)
			{
				for (int delay = 1; delay <= 5; delay++)
				{
					std::vector<int> arrival;
					for (int i = 0; i < 48; i++)
					{
						if ((i < start) || (i >= start + length))
						{
							arrival.push_back(i);
						}
						if (i == start + length - 1 + delay)
						{
							for (int k = start; k < start + length; k++)
							{
								arrival.push_back(k);
							}
						}
					}

					int64_t now = 0;
					mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
														 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, [&now]() { return now; });
					Harness h(&buffer);
					std::vector<int> delivered;
					for (int index : arrival)
					{
						now++;
						const auto out = h.Feed(index, d[index]);
						delivered.insert(delivered.end(), out.begin(), out.end());
					}
					const auto tail = h.FlushRemaining();
					delivered.insert(delivered.end(), tail.begin(), tail.end());

					auto sorted = delivered;
					std::sort(sorted.begin(), sorted.end());
					ASSERT_EQ(sorted, Without(48, {})) << "pattern=" << p << " start=" << start << " length=" << length << " delay=" << delay;
					EXPECT_LE(CountBreaks(d, delivered), CountBreaks(d, arrival))
						<< "pattern=" << p << " start=" << start << " length=" << length << " delay=" << delay;
				}
			}
		}
	}
}

TEST(MpegTsDatagramReorderBuffer, ShiftedLowRatePidRebasesWithoutRepeatedResync)
{
	// Midway, a low-rate PID (an SDT-like table) jumps 9 forward (7 behind modulo 16), as after a switch.
	// The first datagram carrying it is stale with nothing held and becomes the baseline for all its PIDs,
	// so the video in that datagram and after it keeps flowing without holds or resyncs.
	constexpr uint16_t SDT_PID = 0x0011;
	const Layout SDT_DATAGRAM  = {{SDT_PID, 1}, {VIDEO_PID, 6}};

	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 120; i++)
	{
		if (i == 60)
		{
			sender.Skip(SDT_PID, 9);
		}
		d.push_back(sender.Next(((i % 10) == 0) ? SDT_DATAGRAM : (((i % 4) == 3) ? AUDIO_DATAGRAM : VIDEO_DATAGRAM)));
	}

	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS, /*timeout_msec=*/1000000);
	Harness h(&buffer);
	for (int i = 0; i < static_cast<int>(d.size()); i++)
	{
		EXPECT_EQ(h.Feed(i, d[i]), (std::vector<int>{i})) << "index=" << i;
	}
	EXPECT_EQ(h.TotalResyncs(), 0u);
	EXPECT_EQ(h.TotalDeclaredLost(), 0u);
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, CopyWithinDuplicateWindowDoesNotRebase)
{
	int64_t now = 0;
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, [&now]() { return now; });
	Harness h(&buffer);

	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 5; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}
	auto copy_of_d0 = [&d]() { return std::make_shared<ov::Data>(d[0]->GetData(), d[0]->GetLength()); };

	now				= 10;
	h.Feed(0, d[0]);
	now = 20;
	h.Feed(1, d[1]);
	now = 30;
	h.Feed(2, d[2]);

	// 250 ms after d0: still within the window, so the stale copy passes through and the baseline stays on d2.
	now = 10 + mpegts::DatagramReorderBuffer::DUPLICATE_WINDOW_MSEC;
	EXPECT_EQ(h.Feed(10, copy_of_d0()), (std::vector<int>{10}));

	// d0 itself has left the window, but the first copy has not, so a second copy is still a duplicate.
	now += 10;
	EXPECT_EQ(h.Feed(11, copy_of_d0()), (std::vector<int>{11}));

	// A different datagram continuing d0 (cc 7) is two ahead of the baseline (cc 5 after d2), so it is held.
	EXPECT_EQ(h.Feed(12, MakeVideoRun(7, 7)), (std::vector<int>{}));

	// d3 continues d2; the held datagram is then behind and passed through after it.
	EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{3, 12}));
	EXPECT_EQ(h.Feed(4, d[4]), (std::vector<int>{4}));
	EXPECT_EQ(h.TotalResyncs(), 0u);
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, CopyOlderThanDuplicateWindowIsALateOriginal)
{
	int64_t now = 0;
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, [&now]() { return now; });
	Harness h(&buffer);

	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 3; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}

	now = 10;
	h.Feed(0, d[0]);
	now = 20;
	h.Feed(1, d[1]);
	now = 30;
	h.Feed(2, d[2]);

	// 251 ms after d0: outside the window, so the stale copy is delivered as a late original and the baseline follows it.
	now = 10 + mpegts::DatagramReorderBuffer::DUPLICATE_WINDOW_MSEC + 1;
	EXPECT_EQ(h.Feed(10, std::make_shared<ov::Data>(d[0]->GetData(), d[0]->GetLength())), (std::vector<int>{10}));
	EXPECT_FALSE(h.LastResult().resynced);

	// A copy of d1 now continues the baseline exactly, so it is delivered in order.
	EXPECT_EQ(h.Feed(11, std::make_shared<ov::Data>(d[1]->GetData(), d[1]->GetLength())), (std::vector<int>{11}));
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}

TEST(MpegTsDatagramReorderBuffer, KeyCollisionOnlySkipsTheRebase)
{
	// Copies are matched by length and the first and last non-null TS packets only.
	// A different datagram with the same length, first packet and last packet counts as a copy:
	// it is still emitted once, and only its rebase is skipped.
	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 4; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}

	// Same length and same first (cc 0) and last (cc 6) packets as d0, different counters in between.
	const auto collision  = MakeDatagram({{VIDEO_PID, 0}, {VIDEO_PID, 9}, {VIDEO_PID, 10}, {VIDEO_PID, 11}, {VIDEO_PID, 12}, {VIDEO_PID, 13}, {VIDEO_PID, 6}});
	// Same length and first packet as d0 but a different last packet: not a copy.
	const auto other_last = MakeDatagram({{VIDEO_PID, 0}, {VIDEO_PID, 1}, {VIDEO_PID, 2}, {VIDEO_PID, 3}, {VIDEO_PID, 4}, {VIDEO_PID, 5}, {VIDEO_PID, 9}});

	{
		// A fixed clock keeps the copies within the duplicate window however long the run takes.
		mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
											 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, []() { return static_cast<int64_t>(0); });
		Harness h(&buffer);
		h.Feed(0, d[0]);
		h.Feed(1, d[1]);
		h.Feed(2, d[2]);

		EXPECT_EQ(h.Feed(10, collision), (std::vector<int>{10}));
		EXPECT_EQ(h.Feed(3, d[3]), (std::vector<int>{3}));	// the baseline stayed on d2
		EXPECT_EQ(buffer.GetBufferedCount(), 0u);
	}

	{
		mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
											 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, []() { return static_cast<int64_t>(0); });
		Harness h(&buffer);
		h.Feed(0, d[0]);
		h.Feed(1, d[1]);
		h.Feed(2, d[2]);

		EXPECT_EQ(h.Feed(11, other_last), (std::vector<int>{11}));						 // stale, not a copy: it becomes the baseline
		EXPECT_EQ(h.Feed(12, MakeDatagram({{VIDEO_PID, 10}})), (std::vector<int>{12}));	 // in order after its last cc 9
		EXPECT_EQ(h.TotalResyncs(), 0u);
		EXPECT_EQ(buffer.GetBufferedCount(), 0u);
	}
}

TEST(MpegTsDatagramReorderBuffer, ClockGoingBackwardClearsRecentArrivals)
{
	int64_t now = 0;
	mpegts::DatagramReorderBuffer buffer(mpegts::DatagramReorderBuffer::DEFAULT_MAX_DATAGRAMS,
										 mpegts::DatagramReorderBuffer::DEFAULT_TIMEOUT_MSEC, [&now]() { return now; });
	Harness h(&buffer);

	Sender sender;
	std::vector<std::shared_ptr<const ov::Data>> d;
	for (int i = 0; i < 3; i++)
	{
		d.push_back(sender.Next(VIDEO_DATAGRAM));
	}
	auto copy = [&d](int index) { return std::make_shared<ov::Data>(d[index]->GetData(), d[index]->GetLength()); };

	now		  = 1000000;
	h.Feed(0, d[0]);
	h.Feed(1, d[1]);
	h.Feed(2, d[2]);

	// The clock jumps back: earlier arrivals are forgotten, so a stale copy of d0 is a late original and rebases.
	now = 0;
	EXPECT_EQ(h.Feed(10, copy(0)), (std::vector<int>{10}));
	EXPECT_EQ(h.Feed(11, copy(1)), (std::vector<int>{11}));	 // in order after the copy of d0

	// Arrivals are recorded again from the new clock: another copy of d1 is now a duplicate and keeps the baseline.
	now = 10;
	EXPECT_EQ(h.Feed(12, copy(1)), (std::vector<int>{12}));
	EXPECT_EQ(h.Feed(13, copy(2)), (std::vector<int>{13}));	 // still in order after the first copy of d1
	EXPECT_EQ(h.TotalResyncs(), 0u);
	EXPECT_EQ(buffer.GetBufferedCount(), 0u);
}
