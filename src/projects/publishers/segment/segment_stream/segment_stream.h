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
#include "stream_packetizer.h"

class PacketizerFactoryInterface
{
public:
	virtual std::shared_ptr<StreamPacketizer> Create(
		const ov::String &app_name, const ov::String &stream_name,
		uint32_t segment_count, uint32_t segment_duration,
		const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
		const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer) = 0;
};

template <typename Tpacketizer, typename std::enable_if_t<std::is_base_of_v<StreamPacketizer, Tpacketizer>, int> = 0>
class PacketizerFactory : public PacketizerFactoryInterface
{
public:
	std::shared_ptr<StreamPacketizer> Create(
		const ov::String &app_name, const ov::String &stream_name,
		uint32_t segment_count, uint32_t segment_duration,
		const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
		const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer) override
	{
		return std::make_shared<Tpacketizer>(
			app_name, stream_name,
			segment_count, segment_duration,
			video_track, audio_track,
			chunked_transfer);
	}
};

class SegmentStream : public pub::Stream
{
public:
	SegmentStream(
		const std::shared_ptr<pub::Application> application,
		const info::Stream &info,
		int segment_count, int segment_duration,
		const std::shared_ptr<PacketizerFactoryInterface> &packetizer_factory,
		const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	template <typename Tpacketizer>
	static std::shared_ptr<SegmentStream> Create(const std::shared_ptr<pub::Application> &application,
												 const info::Stream &info,
												 int segment_count, int segment_duration,
												 uint32_t thread_count,
												 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	{
		return std::make_shared<SegmentStream>(
			application, info,
			segment_count, segment_duration,
			std::make_shared<PacketizerFactory<Tpacketizer>>(),
			chunked_transfer);
	}

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	bool Start() override;
	bool Stop() override;

	bool GetPlayList(ov::String &play_list);

	std::shared_ptr<const SegmentItem> GetSegmentData(const ov::String &file_name) const;

protected:
	virtual bool CheckCodec(cmn::MediaType type, cmn::MediaCodecId codec_id);

	int _segment_count = 0;
	int _segment_duration = 0;

	std::shared_ptr<PacketizerFactoryInterface> _packetizer_factory;

	std::shared_ptr<ChunkedTransferInterface> _chunked_transfer;

	std::shared_ptr<StreamPacketizer> _stream_packetizer = nullptr;

	std::map<uint32_t, std::shared_ptr<MediaTrack>> _media_tracks;

	std::shared_ptr<MediaTrack> _video_track;
	std::shared_ptr<MediaTrack> _audio_track;
};
