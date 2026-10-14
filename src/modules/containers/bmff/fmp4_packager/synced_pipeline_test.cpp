//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2026 AirenSoft. All rights reserved.
//
//==============================================================================
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>
#include <base/modules/data_format/cue_event/cue_event.h>

#include "fmp4_test_fixtures.h"
#include "synced_boundary_policy.h"

// Synced segmentation with the real packagers: the reference (video) track cuts
// at its keyframes nearest the segment cadence and publishes every realized
// boundary; the synced (audio) track cuts at those boundaries and adopts their
// numbering. The same segment number must mean the same time slot on both
// tracks whatever the delivery skew or start offset.

namespace
{
	constexpr double kSegmentDurationMs = 4000.0;
	constexpr double kChunkDurationMs = 400.0;
	constexpr int kGopFrames = 30;	// 1 s at 30 fps
	constexpr double kAudioFrameMs = 64.0 / 3.0;
	constexpr double kVideoFrameMs = 100.0 / 3.0;

	using fmp4_test::TrackPipeline;

	struct SyncedRig
	{
		std::shared_ptr<bmff::ReferenceBoundaryPolicy> reference_policy;
		TrackPipeline video;  // reference
		TrackPipeline audio;  // synced
	};

	SyncedRig MakeRig(LLHlsCueOutCutMode cue_out_cut_mode = LLHlsCueOutCutMode::Keyframe)
	{
		SyncedRig rig;

		auto observer = std::make_shared<fmp4_test::NullStorageObserver>();

		bmff::SegmentBoundaryPolicy::Config policy_config;
		policy_config.segment_duration_ms = static_cast<uint64_t>(kSegmentDurationMs);
		policy_config.chunk_duration_ms = kChunkDurationMs;
		policy_config.cue_out_cut_mode = cue_out_cut_mode;

		auto video_track = fmp4_test::MakeVideoTrack(0, 30.0, kGopFrames);
		auto reference_config = policy_config;
		reference_config.log_context = "synced_pipeline_test/video";
		auto reference_policy = std::make_shared<bmff::ReferenceBoundaryPolicy>(reference_config, 30.0);
		rig.reference_policy = reference_policy;
		rig.video = fmp4_test::MakePipeline(video_track, observer, reference_policy, static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

		auto audio_track = fmp4_test::MakeAudioTrack(1);
		auto synced_config = policy_config;
		synced_config.log_context = "synced_pipeline_test/audio";
		auto synced_policy = std::make_shared<bmff::SyncedBoundaryPolicy>(reference_policy, synced_config);
		rig.audio = fmp4_test::MakePipeline(audio_track, observer, synced_policy, static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

		return rig;
	}

	// The synced-mode marker protocol: only the reference accepts markers, the
	// synced tracks receive them relayed on the realized boundaries
	bool InsertCueMarkerToReference(SyncedRig &rig, CueEvent::CueType type, int64_t timestamp_ms, uint32_t duration_ms, bool provisional = false)
	{
		auto data = CueEvent::Create(type, duration_ms, 0, provisional)->Serialize();
		auto marker = Marker::CreateMarker(cmn::BitstreamFormat::CUE, timestamp_ms, timestamp_ms, data);
		if (marker == nullptr)
		{
			return false;
		}

		EXPECT_FALSE(rig.audio.policy->AcceptsMarkers());

		return rig.reference_policy->InsertMarker(marker);
	}

	// Drive both pipelines interleaved by delivery time; audio_dts_lead_ms > 0
	// delivers audio that far ahead of video (the skew the chunk gate absorbs)
	void DriveInterleaved(SyncedRig &rig, int64_t total_duration_ms,
						  int64_t video_start_ms = 0, int64_t audio_start_ms = 0, int64_t audio_dts_lead_ms = 0,
						  const std::vector<int64_t> &cue_out_timestamps = {}, uint32_t cue_duration_ms = 0)
	{
		int64_t video_index = 0;
		int64_t audio_index = 0;
		size_t cue_index = 0;

		while (true)
		{
			const int64_t video_dts = video_start_ms + video_index * 100 / 3;
			const int64_t audio_dts = audio_start_ms + audio_index * 64 / 3;
			const int64_t now_ms = std::min(video_dts, audio_dts - audio_dts_lead_ms);

			if (now_ms >= total_duration_ms)
			{
				break;
			}

			if (cue_index < cue_out_timestamps.size() && now_ms >= cue_out_timestamps[cue_index] - 1000)
			{
				ASSERT_TRUE(InsertCueMarkerToReference(rig, CueEvent::CueType::OUT, cue_out_timestamps[cue_index], cue_duration_ms));
				ASSERT_TRUE(InsertCueMarkerToReference(rig, CueEvent::CueType::IN, cue_out_timestamps[cue_index] + cue_duration_ms, 0, true));
				cue_index++;
			}

			if (video_dts <= audio_dts - audio_dts_lead_ms)
			{
				const int64_t duration = video_start_ms + (video_index + 1) * 100 / 3 - video_dts;
				ASSERT_TRUE(rig.video.packager->AppendSample(fmp4_test::MakeVideoFrame(0, video_dts, duration, (video_index % kGopFrames) == 0)));
				video_index++;
			}
			else
			{
				const int64_t duration = audio_start_ms + (audio_index + 1) * 64 / 3 - audio_dts;
				ASSERT_TRUE(rig.audio.packager->AppendSample(fmp4_test::MakeAudioFrame(1, audio_dts, duration)));
				audio_index++;
			}
		}

		ASSERT_EQ(cue_index, cue_out_timestamps.size());
	}

	// Every segment number present and completed on both tracks must mean the
	// same time slot; audio quantizes to its frame size. The first audio
	// segment of a late-started track begins mid-slot, so only its end counts.
	void ExpectAlignedSegments(const SyncedRig &rig, size_t min_common_segments)
	{
		size_t common_segments = 0;
		bool audio_first_seen = false;

		for (int64_t number = 0; number <= rig.video.storage->GetLastSegmentNumber(); number++)
		{
			auto video_segment = rig.video.storage->GetSegment(number);
			auto audio_segment = rig.audio.storage->GetSegment(number);

			bool is_first_audio_segment = (audio_segment != nullptr && audio_first_seen == false);
			if (audio_segment != nullptr)
			{
				audio_first_seen = true;
			}

			if (video_segment == nullptr || audio_segment == nullptr ||
				video_segment->IsCompleted() == false || audio_segment->IsCompleted() == false)
			{
				continue;
			}

			common_segments++;

			double video_start_ms = static_cast<double>(video_segment->GetStartTimestamp());
			double audio_start_ms = static_cast<double>(audio_segment->GetStartTimestamp());
			double video_end_ms = video_start_ms + video_segment->GetDurationMs();
			double audio_end_ms = audio_start_ms + audio_segment->GetDurationMs();

			if (is_first_audio_segment == false)
			{
				EXPECT_NEAR(video_start_ms, audio_start_ms, kAudioFrameMs + 1.0) << "segment " << number;
			}

			EXPECT_NEAR(video_end_ms, audio_end_ms, kAudioFrameMs + 1.0) << "segment " << number;
		}

		EXPECT_GE(common_segments, min_common_segments);
	}
}  // namespace

TEST(SyncedPipelineTest, SegmentsAlignFromTheStart)
{
	auto rig = MakeRig();
	DriveInterleaved(rig, 60000);

	// Keyframes sit on the segment cadence, so the reference cuts exactly at
	// multiples of the duration and the audio follows within one frame
	ExpectAlignedSegments(rig, 12);
}

TEST(SyncedPipelineTest, AudioDeliveredAheadStaysAligned)
{
	auto rig = MakeRig();
	DriveInterleaved(rig, 60000, 0, 0, 1000);

	ExpectAlignedSegments(rig, 12);
}

TEST(SyncedPipelineTest, ReferencePartsStayWithinSpecThroughReordering)
{
	// The reference cuts its parts on the duration cadence whatever the
	// reordering of the feed, and every part keeps to the spec limits
	auto track = fmp4_test::MakeReorderedVideoTrack(0);

	bmff::SegmentBoundaryPolicy::Config config;
	config.segment_duration_ms = static_cast<uint64_t>(kSegmentDurationMs);
	config.chunk_duration_ms = kChunkDurationMs;
	config.log_context = "synced_pipeline_test/reference";
	auto policy = std::make_shared<bmff::ReferenceBoundaryPolicy>(config, fmp4_test::kReorderFrameRate);
	auto pipeline = fmp4_test::MakePipeline(track, std::make_shared<fmp4_test::NullStorageObserver>(), policy,
										   static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

	for (int64_t index = 0; index < 120; index++)
	{
		ASSERT_TRUE(pipeline.packager->AppendSample(fmp4_test::MakeReorderedVideoFrame(0, index)));
	}

	fmp4_test::ExpectPartsWithinSpec(pipeline, kChunkDurationMs, 20);
}

TEST(SyncedPipelineTest, SyncedPartsStayWithinSpecThroughReordering)
{
	// A synced video track follows the reference's progress, so it emits from
	// a backlog in part-sized slices; every slice keeps to the spec limits
	// whatever the reordering of the feed
	auto observer = std::make_shared<fmp4_test::NullStorageObserver>();

	bmff::SegmentBoundaryPolicy::Config policy_config;
	policy_config.segment_duration_ms = static_cast<uint64_t>(kSegmentDurationMs);
	policy_config.chunk_duration_ms = kChunkDurationMs;

	auto reference_config = policy_config;
	reference_config.log_context = "synced_pipeline_test/reference";
	auto reference_policy = std::make_shared<bmff::ReferenceBoundaryPolicy>(reference_config, 30.0);
	auto reference = fmp4_test::MakePipeline(fmp4_test::MakeVideoTrack(0, 30.0, kGopFrames), observer, reference_policy,
											 static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

	auto synced_config = policy_config;
	synced_config.log_context = "synced_pipeline_test/synced";
	auto synced_policy = std::make_shared<bmff::SyncedBoundaryPolicy>(reference_policy, synced_config);
	auto synced = fmp4_test::MakePipeline(fmp4_test::MakeReorderedVideoTrack(1), observer, synced_policy,
										  static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

	// Interleaved by decode time, the reference first on a tie so that its
	// progress mark is ahead whenever the synced track asks
	int64_t reference_index = 0;
	int64_t synced_index = 0;
	while (synced_index < 120)
	{
		const int64_t reference_dts = reference_index * 100 / 3;
		const int64_t synced_dts = synced_index * fmp4_test::kReorderFrameMs;

		if (reference_dts <= synced_dts)
		{
			const int64_t duration = (reference_index + 1) * 100 / 3 - reference_dts;
			ASSERT_TRUE(reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, reference_dts, duration, (reference_index % kGopFrames) == 0)));
			reference_index++;
		}
		else
		{
			ASSERT_TRUE(synced.packager->AppendSample(fmp4_test::MakeReorderedVideoFrame(1, synced_index)));
			synced_index++;
		}
	}

	fmp4_test::ExpectPartsWithinSpec(synced, kChunkDurationMs, 20);
}

namespace
{
	// Two video tracks carrying the same pictures with different reorder
	// delays: every frame's pts is dts + delay, so the same picture has a
	// different dts on each track. Keyframes every second on the reference
	// and every two seconds on the synced track, both on the same pictures.
	struct VideoPair
	{
		std::shared_ptr<bmff::ReferenceBoundaryPolicy> reference_policy;
		TrackPipeline reference;
		TrackPipeline synced;
	};

	VideoPair MakeVideoPair(LLHlsCueOutCutMode cue_out_cut_mode = LLHlsCueOutCutMode::Keyframe)
	{
		VideoPair pair;
		auto observer = std::make_shared<fmp4_test::NullStorageObserver>();

		bmff::SegmentBoundaryPolicy::Config policy_config;
		policy_config.segment_duration_ms = static_cast<uint64_t>(kSegmentDurationMs);
		policy_config.chunk_duration_ms = kChunkDurationMs;
		policy_config.cue_out_cut_mode = cue_out_cut_mode;

		auto reference_config = policy_config;
		reference_config.log_context = "synced_pipeline_test/reference";
		pair.reference_policy = std::make_shared<bmff::ReferenceBoundaryPolicy>(reference_config, 30.0);
		pair.reference = fmp4_test::MakePipeline(fmp4_test::MakeVideoTrack(0, 30.0, kGopFrames), observer, pair.reference_policy,
												 static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

		auto synced_config = policy_config;
		synced_config.log_context = "synced_pipeline_test/synced";
		auto synced_policy = std::make_shared<bmff::SyncedBoundaryPolicy>(pair.reference_policy, synced_config);
		pair.synced = fmp4_test::MakePipeline(fmp4_test::MakeVideoTrack(1, 30.0, kGopFrames * 2), observer, synced_policy,
											  static_cast<uint64_t>(kSegmentDurationMs), kChunkDurationMs, "synced_pipeline_test");

		return pair;
	}

	VideoPair DriveVideoPair(int64_t reference_delay_ms, int64_t synced_delay_ms, int64_t total_duration_ms,
							 const std::vector<int64_t> &cue_out_timestamps = {}, uint32_t cue_duration_ms = 0, int64_t synced_shift_ms = 0,
							 LLHlsCueOutCutMode cue_out_cut_mode = LLHlsCueOutCutMode::Keyframe)
	{
		VideoPair pair = MakeVideoPair(cue_out_cut_mode);

		// Picture i displays at 1000 + i * 100 / 3 on both tracks
		size_t cue_index = 0;
		for (int64_t index = 0; 1000 + index * 100 / 3 < total_duration_ms; index++)
		{
			const int64_t pts = 1000 + index * 100 / 3;
			const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;

			if (cue_index < cue_out_timestamps.size() && pts >= cue_out_timestamps[cue_index] - 1000)
			{
				auto out_data = CueEvent::Create(CueEvent::CueType::OUT, cue_duration_ms, 0, false)->Serialize();
				EXPECT_TRUE(pair.reference_policy->InsertMarker(Marker::CreateMarker(cmn::BitstreamFormat::CUE, cue_out_timestamps[cue_index], cue_out_timestamps[cue_index], out_data)));
				auto in_data = CueEvent::Create(CueEvent::CueType::IN, 0, 0, true)->Serialize();
				EXPECT_TRUE(pair.reference_policy->InsertMarker(Marker::CreateMarker(cmn::BitstreamFormat::CUE, cue_out_timestamps[cue_index] + cue_duration_ms, cue_out_timestamps[cue_index] + cue_duration_ms, in_data)));
				cue_index++;
			}

			EXPECT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, pts - reference_delay_ms, duration, (index % kGopFrames) == 0, pts)));
			EXPECT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, pts + synced_shift_ms - synced_delay_ms, duration, (index % (kGopFrames * 2)) == 0, pts + synced_shift_ms)));
		}

		return pair;
	}

	// Every segment number completed on both tracks starts on the same
	// picture, and every part keeps to the spec: never longer than the part
	// target, and never shorter than 85% of it unless independent or final
	void ExpectSamePicturesAndSpecParts(const VideoPair &pair, int64_t reference_delay_ms, int64_t synced_delay_ms, size_t min_common_segments, int64_t synced_shift_ms = 0, int64_t first_segment = 0)
	{
		size_t common_segments = 0;

		for (int64_t number = first_segment; number <= pair.reference.storage->GetLastSegmentNumber(); number++)
		{
			auto reference_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.reference.storage->GetSegment(number));
			auto synced_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.synced.storage->GetSegment(number));
			if (reference_segment == nullptr || synced_segment == nullptr ||
				reference_segment->IsCompleted() == false || synced_segment->IsCompleted() == false)
			{
				continue;
			}

			common_segments++;

			int64_t reference_start_pts = reference_segment->GetStartTimestamp() + reference_delay_ms;
			int64_t synced_start_pts = synced_segment->GetStartTimestamp() + synced_delay_ms - synced_shift_ms;
			EXPECT_EQ(reference_start_pts, synced_start_pts) << "segment " << number << " opens on different pictures";
			EXPECT_NEAR(synced_segment->GetDurationMs(), kSegmentDurationMs, kVideoFrameMs + 1.0) << "segment " << number;

			for (uint64_t index = 0; index < synced_segment->GetPartialCount(); index++)
			{
				auto partial = synced_segment->GetPartialSegment(index);
				ASSERT_NE(partial, nullptr);

				EXPECT_LE(partial->GetDurationMs(), kChunkDurationMs + 1.0) << "segment " << number << " part " << index;

				bool final_part = (index + 1 == synced_segment->GetPartialCount());
				if (partial->IsIndependent() == false && final_part == false)
				{
					EXPECT_GE(partial->GetDurationMs(), kChunkDurationMs * 0.85 - 1.0) << "segment " << number << " part " << index;
				}
			}
		}

		EXPECT_GE(common_segments, min_common_segments);
	}
}  // namespace

TEST(SyncedPipelineTest, SyncedVideoWithALongerReorderDelayStaysAligned)
{
	// The synced track's keyframes decode two frames before they display, the
	// reference's one frame: the keyframe opening a segment sits one frame
	// before the reference's boundary in decode time, on the same picture
	auto pair = DriveVideoPair(33, 67, 60000);
	ExpectSamePicturesAndSpecParts(pair, 33, 67, 12);
}

TEST(SyncedPipelineTest, SyncedVideoWithAShorterReorderDelayStaysAligned)
{
	auto pair = DriveVideoPair(67, 33, 60000);
	ExpectSamePicturesAndSpecParts(pair, 67, 33, 12);
}

TEST(SyncedPipelineTest, SyncedVideoWithAMuchLongerReorderDelayStaysAligned)
{
	// Six frames of delay against one: the opening keyframe decodes well
	// before the reference's boundary and still opens the same picture
	auto pair = DriveVideoPair(33, 200, 60000);
	ExpectSamePicturesAndSpecParts(pair, 33, 200, 12);
}

// The end of what the storage holds for this track: the open segment's end,
// or the last completed one's when the next has nothing stored yet
static int64_t StoredEndMs(const TrackPipeline &pipeline)
{
	for (int64_t number = pipeline.storage->GetLastSegmentNumber(); number >= 0; number--)
	{
		auto segment = std::static_pointer_cast<bmff::FMP4Segment>(pipeline.storage->GetSegment(number));
		if (segment != nullptr && segment->GetPartialCount() > 0)
		{
			return segment->GetStartTimestamp() + static_cast<int64_t>(segment->GetDurationMs());
		}
	}

	return 0;
}

TEST(SyncedPipelineTest, SyncedVideoRunningBehindKeepsItsPartsFlowing)
{
	// The synced track is delivered two seconds behind the reference, so when
	// a boundary is published its buffer still holds the keyframe from the
	// middle of the segment. That keyframe is no candidate for the cut: the
	// parts behind it keep going out, and the track never falls more than a
	// part behind its own samples. Its first segment spans the lag; the
	// alignment holds from the second one on.
	const int64_t reference_delay_ms = 33;
	const int64_t synced_delay_ms = 67;
	const int64_t lag_frames = kGopFrames * 2;
	auto pair = MakeVideoPair();

	int64_t worst_backlog_ms = 0;
	for (int64_t index = 0; index < 1800; index++)
	{
		const int64_t reference_index = index + lag_frames;
		const int64_t reference_pts = 1000 + reference_index * 100 / 3;
		const int64_t reference_duration = 1000 + (reference_index + 1) * 100 / 3 - reference_pts;
		ASSERT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, reference_pts - reference_delay_ms, reference_duration, (reference_index % kGopFrames) == 0, reference_pts)));

		const int64_t pts = 1000 + index * 100 / 3;
		const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;
		ASSERT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, pts - synced_delay_ms, duration, (index % (kGopFrames * 2)) == 0, pts)));

		if (index > lag_frames + kGopFrames * 2)
		{
			worst_backlog_ms = std::max(worst_backlog_ms, (pts - synced_delay_ms) - StoredEndMs(pair.synced));
		}
	}

	// One part may wait for its target, and a candidate keyframe may hold the
	// samples behind it for up to its lead past the boundary: one more part
	EXPECT_LE(worst_backlog_ms, static_cast<int64_t>(kChunkDurationMs) * 2 + 1);
	ExpectSamePicturesAndSpecParts(pair, reference_delay_ms, synced_delay_ms, 12, 0, 1);
}

TEST(SyncedPipelineTest, SyncedVideoRunningAheadNeverPassesTheReference)
{
	// The synced track is delivered 1.5 s ahead of the reference and its
	// keyframes sit 1.5 s past every boundary, so each cut lands on a keyframe
	// the reference has not reached yet. Nothing it stores may run ahead of
	// what the reference has stored, the closing part included.
	const int64_t reference_delay_ms = 33;
	const int64_t synced_delay_ms = 33;
	const int64_t lead_frames = 45;
	auto pair = MakeVideoPair();

	for (int64_t index = 0; index < 1800; index++)
	{
		const int64_t synced_index = index + lead_frames;
		const int64_t synced_pts = 1000 + synced_index * 100 / 3;
		const int64_t synced_duration = 1000 + (synced_index + 1) * 100 / 3 - synced_pts;
		ASSERT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, synced_pts - synced_delay_ms, synced_duration, (synced_index % (kGopFrames * 4)) == lead_frames, synced_pts)));

		const int64_t pts = 1000 + index * 100 / 3;
		const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;
		ASSERT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, pts - reference_delay_ms, duration, (index % kGopFrames) == 0, pts)));

		EXPECT_LE(StoredEndMs(pair.synced), StoredEndMs(pair.reference) + 1) << "at picture " << index;
	}

	EXPECT_GE(pair.synced.storage->GetLastSegmentNumber(), 12);
}

TEST(SyncedPipelineTest, APropagatedBreakClosesBothTracksOnTheSamePicture)
{
	// A break propagated from another track reaches both video tracks just
	// before a picture both have a keyframe on. Each closes at that keyframe,
	// so the discontinuity lands on the same segment number and the segment
	// after it opens on the same picture, the reorder skew notwithstanding.
	const int64_t reference_delay_ms = 33;
	const int64_t synced_delay_ms = 67;
	const int64_t break_ms = 6900;
	auto pair = MakeVideoPair();

	bool requested = false;
	for (int64_t index = 0; 1000 + index * 100 / 3 < 20000; index++)
	{
		const int64_t pts = 1000 + index * 100 / 3;
		const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;

		if (requested == false && pts >= break_ms - 500)
		{
			pair.reference_policy->RequestDiscontinuity(break_ms * 1000);
			pair.synced.policy->RequestDiscontinuity(break_ms * 1000);
			requested = true;
		}

		ASSERT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, pts - reference_delay_ms, duration, (index % kGopFrames) == 0, pts)));
		ASSERT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, pts - synced_delay_ms, duration, (index % (kGopFrames * 2)) == 0, pts)));
	}

	auto first_break_segment = [](const TrackPipeline &pipeline) -> int64_t {
		for (int64_t number = 0; number <= pipeline.storage->GetLastSegmentNumber(); number++)
		{
			auto segment = pipeline.storage->GetSegment(number);
			if (segment != nullptr && segment->IsDiscontinuityPoint() == true)
			{
				return number;
			}
		}
		return -1;
	};

	int64_t reference_break = first_break_segment(pair.reference);
	int64_t synced_break = first_break_segment(pair.synced);
	ASSERT_GE(reference_break, 0);
	EXPECT_EQ(reference_break, synced_break);

	for (int64_t number = 1; number <= pair.reference.storage->GetLastSegmentNumber(); number++)
	{
		auto reference_segment = pair.reference.storage->GetSegment(number);
		auto synced_segment = pair.synced.storage->GetSegment(number);
		if (reference_segment == nullptr || synced_segment == nullptr || synced_segment->IsCompleted() == false)
		{
			continue;
		}

		EXPECT_EQ(reference_segment->GetStartTimestamp() + reference_delay_ms, synced_segment->GetStartTimestamp() + synced_delay_ms) << "segment " << number;
	}

	auto break_segment = pair.synced.storage->GetSegment(synced_break);
	ASSERT_NE(break_segment, nullptr);
	EXPECT_EQ(break_segment->GetStartTimestamp() + synced_delay_ms, 7000);
}

TEST(SyncedPipelineTest, ABreakInsideASyncedGopClosesAtTheKeyframeBeforeIt)
{
	// The reference changes on one of its keyframes while the synced track's
	// keyframes sit 100 ms before the reference's, off the shared grid. The
	// synced track has no keyframe at the break; its keyframe nearest the
	// boundary lies just before it, within a part. The segment closes there,
	// carries the discontinuity on the same segment number as the reference,
	// and the following segments keep the same 100 ms alignment, instead of
	// waiting a whole GOP for the next keyframe or being forced closed.
	const int64_t break_ms = 8000;
	const int64_t synced_lead_ms = 100;
	auto pair = MakeVideoPair();

	bool requested = false;
	for (int64_t index = 0; 1000 + index * 100 / 3 < 20000; index++)
	{
		const int64_t pts = 1000 + index * 100 / 3;
		const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;

		if (requested == false && pts >= break_ms - 500)
		{
			pair.reference_policy->RequestDiscontinuity(break_ms * 1000);
			pair.synced.policy->RequestDiscontinuity(break_ms * 1000);
			requested = true;
		}

		// Reference keyframes at 1000, 2000, ... and synced ones at 1000, 1900, 2900, ...
		bool reference_keyframe = (index % kGopFrames) == 0;
		bool synced_keyframe = (index == 0) || (index % kGopFrames) == kGopFrames - 3;
		ASSERT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, pts, duration, reference_keyframe, pts)));
		ASSERT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, pts, duration, synced_keyframe, pts)));
	}

	auto first_break_segment = [](const TrackPipeline &pipeline) -> int64_t {
		for (int64_t number = 0; number <= pipeline.storage->GetLastSegmentNumber(); number++)
		{
			auto segment = pipeline.storage->GetSegment(number);
			if (segment != nullptr && segment->IsDiscontinuityPoint() == true)
			{
				return number;
			}
		}
		return -1;
	};

	int64_t reference_break = first_break_segment(pair.reference);
	int64_t synced_break = first_break_segment(pair.synced);
	ASSERT_GE(reference_break, 0);
	EXPECT_EQ(reference_break, synced_break);

	// The reference closes on its keyframe at the break, the synced track on
	// its keyframe 100 ms before it
	EXPECT_EQ(pair.reference.storage->GetSegment(reference_break)->GetStartTimestamp(), break_ms);
	EXPECT_EQ(pair.synced.storage->GetSegment(synced_break)->GetStartTimestamp(), break_ms - synced_lead_ms);

	size_t common_segments = 0;
	for (int64_t number = 1; number <= pair.reference.storage->GetLastSegmentNumber(); number++)
	{
		auto reference_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.reference.storage->GetSegment(number));
		auto synced_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.synced.storage->GetSegment(number));
		if (reference_segment == nullptr || synced_segment == nullptr || synced_segment->IsCompleted() == false)
		{
			continue;
		}

		common_segments++;
		EXPECT_EQ(synced_segment->GetStartTimestamp(), reference_segment->GetStartTimestamp() - synced_lead_ms) << "segment " << number;
		// Never stretched to the next keyframe, never forced closed
		EXPECT_LE(synced_segment->GetDurationMs(), kSegmentDurationMs + kChunkDurationMs) << "segment " << number;
	}
	EXPECT_GE(common_segments, 3u);
}

TEST(SyncedPipelineTest, ASyncedTrackWithNoKeyframeNearTheBoundaryWaitsForTheNextOne)
{
	// The synced track's keyframes sit 500 ms after the reference's, further
	// than a part, so no keyframe is buffered when a boundary is reached and
	// the nearest-keyframe planner has nothing to pick yet. The segment must
	// stay open until that keyframe arrives: every segment opens independent
	// and keeps the reference's numbering.
	const int64_t synced_lag_ms = 500;
	auto pair = MakeVideoPair();

	for (int64_t index = 0; 1000 + index * 100 / 3 < 30000; index++)
	{
		const int64_t pts = 1000 + index * 100 / 3;
		const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;

		bool reference_keyframe = (index % kGopFrames) == 0;
		bool synced_keyframe = (index == 0) || (index % kGopFrames) == kGopFrames / 2;
		ASSERT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, pts, duration, reference_keyframe, pts)));
		ASSERT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, pts, duration, synced_keyframe, pts)));
	}

	size_t common_segments = 0;
	for (int64_t number = 1; number <= pair.reference.storage->GetLastSegmentNumber(); number++)
	{
		auto reference_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.reference.storage->GetSegment(number));
		auto synced_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.synced.storage->GetSegment(number));
		if (reference_segment == nullptr || synced_segment == nullptr || synced_segment->IsCompleted() == false)
		{
			continue;
		}

		common_segments++;
		ASSERT_GT(synced_segment->GetPartialCount(), 0u) << "segment " << number;
		EXPECT_TRUE(synced_segment->GetPartialSegment(0)->IsIndependent()) << "segment " << number << " opens on a non-keyframe";
		EXPECT_EQ(synced_segment->GetStartTimestamp(), reference_segment->GetStartTimestamp() + synced_lag_ms) << "segment " << number;
	}
	EXPECT_GE(common_segments, 4u);
}

TEST(SyncedPipelineTest, SyncedVideoOnAShiftedFrameGridStaysAligned)
{
	// A frame rate conversion re-times the synced track a few ms off the
	// reference's frames: its keyframe nearest to the boundary opens the segment
	auto pair = DriveVideoPair(33, 33, 60000, {}, 0, 4);
	ExpectSamePicturesAndSpecParts(pair, 33, 33, 12, 4);
}

struct MarkerObservation
{
	int64_t segment_number = -1;
	std::shared_ptr<Marker> marker;
	double segment_end_ms = 0.0;
};

static std::map<size_t, MarkerObservation> CollectMarkers(const TrackPipeline &pipeline, CueEvent::CueType type)
{
	std::map<size_t, MarkerObservation> observations;
	size_t index = 0;

	for (int64_t number = 0; number <= pipeline.storage->GetLastSegmentNumber(); number++)
	{
		auto segment = std::static_pointer_cast<bmff::FMP4Segment>(pipeline.storage->GetSegment(number));
		if (segment == nullptr || segment->HasMarker() == false)
		{
			continue;
		}

		for (const auto &marker : segment->GetMarkers())
		{
			auto cue_event = marker->GetCueEvent();
			if (cue_event == nullptr || cue_event->GetCueType() != type)
			{
				continue;
			}

			observations[index++] = {segment->GetNumber(), marker,
									 static_cast<double>(segment->GetStartTimestamp()) + segment->GetDurationMs()};
		}
	}

	return observations;
}

// The marker protocol on the realized boundaries, in both cut modes: the OUT
// cuts where its mode dictates, the IN (a return point) cuts at the first
// keyframe at or after it, and every synced track carries the same marker on
// the same segment number.
static void ExpectMarkersRideTheBoundary(LLHlsCueOutCutMode cut_mode)
{
	auto rig = MakeRig(cut_mode);

	// Both cues sit mid-GOP (keyframes are at multiples of 1000)
	std::vector<int64_t> cue_out_timestamps = {13210, 33210};
	DriveInterleaved(rig, 60000, 0, 0, 0, cue_out_timestamps, 10000);

	for (auto type : {CueEvent::CueType::OUT, CueEvent::CueType::IN})
	{
		auto video_markers = CollectMarkers(rig.video, type);
		auto audio_markers = CollectMarkers(rig.audio, type);

		ASSERT_EQ(video_markers.size(), cue_out_timestamps.size());
		ASSERT_EQ(audio_markers.size(), cue_out_timestamps.size());

		for (size_t index = 0; index < cue_out_timestamps.size(); index++)
		{
			// One marker object, on the same segment number, on both tracks
			EXPECT_EQ(video_markers[index].segment_number, audio_markers[index].segment_number) << "cue " << index;
			EXPECT_EQ(video_markers[index].marker, audio_markers[index].marker) << "cue " << index;

			double requested_ms = static_cast<double>((type == CueEvent::CueType::OUT) ? cue_out_timestamps[index] : cue_out_timestamps[index] + 10000);
			double realized_ms = video_markers[index].segment_end_ms;

			if (type == CueEvent::CueType::OUT && cut_mode == LLHlsCueOutCutMode::Immediate)
			{
				// An immediate OUT cuts at the first sample boundary at or after
				// its position, mid-GOP included (samples are not splittable)
				EXPECT_GE(realized_ms, requested_ms) << "cue " << index;
				EXPECT_LT(realized_ms, requested_ms + kVideoFrameMs + 1.0) << "cue " << index;
			}
			else
			{
				// A keyframe cut waits for the first keyframe at or after the position
				double next_keyframe_ms = std::ceil(requested_ms / 1000.0) * 1000.0;
				EXPECT_NEAR(realized_ms, next_keyframe_ms, 0.1) << "cue " << index;
			}

			// The audio cut lands on the same boundary within one audio frame
			EXPECT_GE(audio_markers[index].segment_end_ms, realized_ms - 0.1) << "cue " << index;
			EXPECT_LT(audio_markers[index].segment_end_ms, realized_ms + kAudioFrameMs * 2.0 + 1.0) << "cue " << index;
		}
	}
}

TEST(SyncedPipelineTest, MarkersRideTheRealizedBoundary)
{
	ExpectMarkersRideTheBoundary(LLHlsCueOutCutMode::Keyframe);
}

TEST(SyncedPipelineTest, ImmediateCutRealizesTheMarkerMidGop)
{
	ExpectMarkersRideTheBoundary(LLHlsCueOutCutMode::Immediate);
}

TEST(SyncedPipelineTest, MarkersReachASyncedVideoWithALongerReorderDelay)
{
	// Cues whose keyframe cuts (15000, 25000, 35000, 45000) are pictures both
	// tracks have keyframes on; the markers must ride the same segment numbers
	// on the synced track, closed on the same pictures
	const int64_t reference_delay_ms = 33;
	const int64_t synced_delay_ms = 67;
	std::vector<int64_t> cue_out_timestamps = {14210, 34210};
	auto pair = DriveVideoPair(reference_delay_ms, synced_delay_ms, 60000, cue_out_timestamps, 10000);

	for (auto type : {CueEvent::CueType::OUT, CueEvent::CueType::IN})
	{
		auto reference_markers = CollectMarkers(pair.reference, type);
		auto synced_markers = CollectMarkers(pair.synced, type);

		ASSERT_EQ(reference_markers.size(), cue_out_timestamps.size());
		ASSERT_EQ(synced_markers.size(), cue_out_timestamps.size());

		for (size_t index = 0; index < cue_out_timestamps.size(); index++)
		{
			EXPECT_EQ(reference_markers[index].segment_number, synced_markers[index].segment_number) << "cue " << index;
			EXPECT_EQ(reference_markers[index].marker, synced_markers[index].marker) << "cue " << index;
			EXPECT_NEAR(reference_markers[index].segment_end_ms + reference_delay_ms, synced_markers[index].segment_end_ms + synced_delay_ms, 1.0) << "cue " << index;
		}
	}
}

TEST(SyncedPipelineTest, SyncedVideoOnTheSameGridEndsEverySegmentOnTheBoundary)
{
	// The premise of synced segmentation: both video tracks carry the same
	// picture at the same decode time with the same keyframe interval. Every
	// boundary is then a keyframe on the synced track, and its segments end
	// exactly where the reference's do.
	const int64_t delay_ms = 33;
	auto pair = MakeVideoPair();

	for (int64_t index = 0; 1000 + index * 100 / 3 < 60000; index++)
	{
		const int64_t pts = 1000 + index * 100 / 3;
		const int64_t duration = 1000 + (index + 1) * 100 / 3 - pts;
		const bool keyframe = (index % kGopFrames) == 0;

		ASSERT_TRUE(pair.reference.packager->AppendSample(fmp4_test::MakeVideoFrame(0, pts - delay_ms, duration, keyframe, pts)));
		ASSERT_TRUE(pair.synced.packager->AppendSample(fmp4_test::MakeVideoFrame(1, pts - delay_ms, duration, keyframe, pts)));
	}

	size_t common_segments = 0;
	for (int64_t number = 0; number <= pair.reference.storage->GetLastSegmentNumber(); number++)
	{
		auto reference_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.reference.storage->GetSegment(number));
		auto synced_segment = std::static_pointer_cast<bmff::FMP4Segment>(pair.synced.storage->GetSegment(number));
		if (reference_segment == nullptr || synced_segment == nullptr ||
			reference_segment->IsCompleted() == false || synced_segment->IsCompleted() == false)
		{
			continue;
		}

		common_segments++;
		EXPECT_EQ(reference_segment->GetStartTimestamp(), synced_segment->GetStartTimestamp()) << "segment " << number;
		EXPECT_DOUBLE_EQ(reference_segment->GetDurationMs(), synced_segment->GetDurationMs()) << "segment " << number;
	}

	EXPECT_GE(common_segments, 12u);
	ExpectSamePicturesAndSpecParts(pair, delay_ms, delay_ms, 12);
}

TEST(SyncedPipelineTest, AnImmediateCutClosesBothVideoTracksAtTheMarker)
{
	// Two video tracks on the same decode timeline: an immediate CUE-OUT cuts
	// the reference mid-GOP, and the synced track ends its segment at the same
	// position, keyframe or not, on the same segment number. The cues sit a
	// few hundred milliseconds past a keyframe of the synced track, where a
	// keyframe cut would otherwise settle.
	std::vector<int64_t> cue_out_timestamps = {15310, 35310};
	auto pair = DriveVideoPair(33, 33, 60000, cue_out_timestamps, 10000, 0, LLHlsCueOutCutMode::Immediate);

	auto reference_markers = CollectMarkers(pair.reference, CueEvent::CueType::OUT);
	auto synced_markers = CollectMarkers(pair.synced, CueEvent::CueType::OUT);
	ASSERT_EQ(reference_markers.size(), cue_out_timestamps.size());
	ASSERT_EQ(synced_markers.size(), cue_out_timestamps.size());

	for (size_t index = 0; index < cue_out_timestamps.size(); index++)
	{
		EXPECT_EQ(reference_markers[index].segment_number, synced_markers[index].segment_number) << "cue " << index;
		EXPECT_EQ(reference_markers[index].marker, synced_markers[index].marker) << "cue " << index;
		EXPECT_NEAR(reference_markers[index].segment_end_ms, synced_markers[index].segment_end_ms, 1.0) << "cue " << index;
		EXPECT_GE(reference_markers[index].segment_end_ms, static_cast<double>(cue_out_timestamps[index])) << "cue " << index;
		EXPECT_LT(reference_markers[index].segment_end_ms, static_cast<double>(cue_out_timestamps[index]) + kVideoFrameMs + 1.0) << "cue " << index;
	}
}

TEST(SyncedPipelineTest, LateStartingAudioAdoptsTheSlotNumbering)
{
	auto rig = MakeRig();

	// Video from 0, audio joins mid-slot at 4200: the audio playlist simply has
	// no segment 0 and the numbering agrees from segment 1 on
	DriveInterleaved(rig, 40000, 0, 4200);

	EXPECT_EQ(rig.audio.storage->GetSegment(0), nullptr);

	auto first_audio_segment = rig.audio.storage->GetSegment(1);
	ASSERT_NE(first_audio_segment, nullptr);
	EXPECT_NEAR(static_cast<double>(first_audio_segment->GetStartTimestamp()), 4200.0, kAudioFrameMs + 1.0);

	ExpectAlignedSegments(rig, 7);
}
