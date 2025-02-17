//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================

#include "transcoder_events.h"

#include <modules/bitstream/nalu/nal_header.h>
#include <modules/bitstream/nalu/nal_unit_insertor.h>

#include "transcoder_private.h"



TranscoderEvents::TranscoderEvents()
{
}

TranscoderEvents::~TranscoderEvents()
{
}

// If the event is saved, return true and allow the packet to be discarded
bool TranscoderEvents::PushEvent(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> &packet)
{
	if (packet->GetBitstreamFormat() == cmn::BitstreamFormat::SEI)
	{
		for (auto &[output_track_id, track] : stream->GetTracks())
		{
			auto found = (track->GetCodecId() == cmn::MediaCodecId::H264);
						// Not implemented
						// || track->GetCodecId() == cmn::MediaCodecId::H265); 
			if (!found)
			{
				continue;
			}

			_event_packets[output_track_id].push_back(packet);

			// logtd("Received NALU(SEI) event. stream(%s), track(%u), timestamp(%lld)", stream->GetName().CStr(), output_track_id, packet->GetDts());
		}

		// Discard event packet
		return true;
	}

	// If the event is not saved, return false and allow the packet to be sent
	return false;
}

// If the stored event matches the packet, insert the event.
void TranscoderEvents::RunEvent(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> &packet)
{
	auto event_packets = _event_packets.find(packet->GetTrackId());
	if (event_packets != _event_packets.end())
	{
		auto packet_time = (double)packet->GetDts() * ((double)1000 * stream->GetTrack(packet->GetTrackId())->GetTimeBase().GetExpr());

		auto &event_list = event_packets->second;
		auto it = event_list.begin();
		while (it != event_list.end())
		{
			auto event_packet = *it;

			auto event_time = (double)event_packet->GetDts() * ((double)1000 * stream->GetTrack(event_packet->GetTrackId())->GetTimeBase().GetExpr());

			if (event_time <= packet_time)
			{
				auto nalu_data = std::make_shared<ov::Data>();
				auto codec_id = stream->GetTrack(packet->GetTrackId())->GetCodecId();

				if (codec_id == cmn::MediaCodecId::H264)
				{
					nalu_data->Append(NalHeader::CreateH264(6));
				}
				else if (codec_id == cmn::MediaCodecId::H265)
				{
					// nalu_data->Append(NalHeader::CreateH265(35, 0, 0));
					// continue;

					// Not implemented
				}

				nalu_data->Append(event_packet->GetData());

				auto result = NalUnitInsertor::Insert(packet->GetData(), nalu_data, packet->GetBitstreamFormat());
				if (result.has_value())
				{
					packet->SetData(std::get<0>(result.value()));
					packet->SetFragHeader(std::get<1>(result.value()).get());

					logtd("Inserted NALU(SEI). stream(%s), track(%u), timestamp(%.0fms, %lld)", stream->GetName().CStr(), packet->GetTrackId(), packet_time, packet->GetDts());
					// logtd("Payload\n%s", NalUnitInsertor::EmulationPreventionBytes(nalu_data)->Dump().CStr());
				}
				else
				{
					logte("Could not insert NALU(SEI)");
				}

				// Remove used event packet
				it = event_list.erase(it);
			}
			else
			{
				it++;
			}
		}
	}
}
