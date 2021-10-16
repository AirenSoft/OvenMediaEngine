//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/publisher/stream.h>

#include <map>

#include "packetizer/chunked_transfer_interface.h"
#include "packetizer/packetizer.h"

using PacketizerFactory = std::function<std::shared_ptr<Packetizer>(ov::String app_name, ov::String stream_name, std::shared_ptr<MediaTrack> _video_track, std::shared_ptr<MediaTrack> _audio_track)>;

class SegmentStream : public pub::Stream
{
public:
	SegmentStream(
		const std::shared_ptr<pub::Application> application,
		const info::Stream &info,
		PacketizerFactory packetizer_factory);

	static std::shared_ptr<SegmentStream> Create(const std::shared_ptr<pub::Application> &application,
												 const info::Stream &info,
												 PacketizerFactory packetizer_factory)
	{
		return std::make_shared<SegmentStream>(application, info, packetizer_factory);
	}

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	bool Start() override;
	bool Stop() override;
	bool OnStreamUpdated(const std::shared_ptr<info::Stream> &info) override;

	bool GetPlayList(ov::String &play_list);

	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const;

protected:
	virtual bool CheckCodec(cmn::MediaType type, cmn::MediaCodecId codec_id);

	std::shared_ptr<Packetizer> _packetizer = nullptr;

	PacketizerFactory _packetizer_factory;

	std::map<uint32_t, std::shared_ptr<MediaTrack>> _media_tracks;

	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;

	std::atomic<int> _last_msid{0};
};
