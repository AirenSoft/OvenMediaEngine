//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mediarouter_stats.h"

#include <base/ovlibrary/ovlibrary.h>

#include "mediarouter_private.h"

#define PTS_CORRECT_THRESHOLD_MS 3000

using namespace cmn;

MediaRouterStats::MediaRouterStats()
{
	_warning_count_bframe = 0;
	_warning_count_out_of_order = 0;

	_stat_start_time = std::chrono::system_clock::now();
	_stop_watch.Start();
}

MediaRouterStats::~MediaRouterStats()
{
	_stat_recv_pkt_lpts.clear();
	_stat_recv_pkt_ldts.clear();
	_stat_recv_pkt_adur.clear();
}

void MediaRouterStats::Init(const std::shared_ptr<info::Stream> &stream_info)
{

}

void MediaRouterStats::Update(
	const int8_t type,
	const bool prepared,
	const ov::ManagedQueue<std::shared_ptr<MediaPacket>> &packets_queue,
	const std::shared_ptr<info::Stream> &stream_info,
	const std::shared_ptr<MediaTrack> &media_track,
	const std::shared_ptr<MediaPacket> &media_packet)
{
	auto track_id = media_track->GetId();

	// Check b-frame of H264/H265 codec
	//
	// Basically, in order to check the presence of B-Frame, the SliceType of H264 should be checked,
	// but in general, it is assumed that there is a B-frame when PTS and DTS are different. This has a performance advantage.
	switch (media_packet->GetBitstreamFormat())
	{
		case cmn::BitstreamFormat::H264_ANNEXB:
		case cmn::BitstreamFormat::H264_AVCC:
		case cmn::BitstreamFormat::H265_ANNEXB:
		case cmn::BitstreamFormat::HVCC:
			if (_warning_count_bframe < 10)
			{
				if (media_track->GetTotalFrameCount() > 0 && _stat_recv_pkt_lpts[track_id] > media_packet->GetPts())
				{
					media_track->SetHasBframes(true);
				}

				// Display a warning message that b-frame exists
				if (media_track->HasBframes() == true)
				{
					logtw("[%s/%s(%u)] b-frame has been detected in the %u track",
						  stream_info->GetApplicationInfo().GetVHostAppName().CStr(),
						  stream_info->GetName().CStr(),
						  stream_info->GetId(),
						  track_id);

					_warning_count_bframe++;
				}
			}
			break;
		default:
			break;
	}

	_stat_recv_pkt_lpts[track_id] = media_packet->GetPts();
	_stat_recv_pkt_ldts[track_id] = media_packet->GetDts();

	// The packet from the provider has no duration. Do not add it to the total duration.
	if (media_packet->GetDuration() != -1)
	{
		_stat_recv_pkt_adur[track_id] += media_packet->GetDuration();
	}

	if (_stop_watch.IsElapsed(5000) && _stop_watch.Update())
	{
		// Uptime
		int64_t uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _stat_start_time).count();

		int64_t min_pts = -1LL;
		int64_t max_pts = -1LL;

		ov::String stat_track_str = "";

		for (const auto &[track_id, track] : stream_info->GetTracks())
		{
			int64_t scaled_last_pts = (int64_t)((double)(_stat_recv_pkt_lpts[track_id] * 1000) * track->GetTimeBase().GetExpr());
			int64_t scaled_acc_duration = (int64_t)((double)(_stat_recv_pkt_adur[track_id] * 1000) * track->GetTimeBase().GetExpr());

			auto codec_name = ov::String::FormatString("%s(%s)",
													   ::StringFromMediaCodecId(track->GetCodecId()).CStr(),
													   track->IsBypass() ? "PT" : GetStringFromCodecModuleId(track->GetCodecModuleId()).CStr());

			auto timebase = ov::String::FormatString("%d/%d", track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen());

			stat_track_str.AppendFormat("\n - track:%11u, type: %5s, codec: %14s, pts: %10lldms, dur: %10lld(%lld)ms, tb: %7s, pkt_cnt: %6lld, pkt_siz: %7sB, bps: %7s/%7s",
										track_id,
										GetMediaTypeString(track->GetMediaType()).CStr(),
										codec_name.CStr(),
										scaled_last_pts,
										scaled_acc_duration,
										(scaled_acc_duration > 0) ? scaled_last_pts - scaled_acc_duration : 0,
										timebase.CStr(),
										track->GetTotalFrameCount(),
										ov::Converter::ToSiString(track->GetTotalFrameBytes(), 1).CStr(),
										ov::Converter::BitToString(track->GetBitrateByMeasured()).CStr(), ov::Converter::BitToString(track->GetBitrateByConfig()).CStr());

			if (track->GetMediaType() == MediaType::Data)
			{
				continue;
			}

			if (track->GetMediaType() == MediaType::Video)
			{
				stat_track_str.AppendFormat(", fps: %.2f/%.2f",
											track->GetFrameRateByMeasured(), track->GetFrameRate());
				stat_track_str.AppendFormat(", kint: %.2f/%d/%s",
											track->GetKeyFrameIntervalByMeasured(),
											track->GetKeyFrameInterval(),
											cmn::GetKeyFrameIntervalTypeToString(track->GetKeyFrameIntervalTypeByConfig()).CStr());
			}

			// calc min/max pts
			if (min_pts == -1LL)
			{
				min_pts = scaled_last_pts;
			}

			if (max_pts == -1LL)
			{
				max_pts = scaled_last_pts;
			}

			min_pts = std::min(min_pts, scaled_last_pts);
			max_pts = std::max(max_pts, scaled_last_pts);
		}

		ov::String stat_stream_str = "";

		stat_stream_str.AppendFormat("Stream. id: %10u, type: %s, name: %s/%s, status: %s, uptime: %lldms, queue: %d, msid: %u, sync: %lldms",
									 stream_info->GetId(),
									 (type == 1) ? "Inbound" : "Outbound",
									 stream_info->GetApplicationInfo().GetVHostAppName().CStr(),
									 stream_info->GetName().CStr(),
									 prepared ? "Started" : "Preapring",
									 (int64_t)uptime,
									 packets_queue.Size(),
									 stream_info->GetMsid(),
									 max_pts - min_pts);

		stat_track_str = stat_stream_str + stat_track_str;

		logtd("%s", stat_track_str.CStr());
	}
}
