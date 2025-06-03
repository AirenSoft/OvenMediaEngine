//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./rtmp_chunk_handler.h"

#include <modules/data_format/id3v2/frames/id3v2_frames.h>
#include <modules/data_format/id3v2/id3v2.h>
#include <modules/rtmp_v2/rtmp.h>
#include <orchestrator/orchestrator.h>

#include "../rtmp_provider_private.h"
#include "../rtmp_stream_v2.h"
#include "../tracks/rtmp_audio_track.h"
#include "../tracks/rtmp_track.h"
#include "../tracks/rtmp_video_track.h"

#undef RTMP_STREAM_DESC
#define RTMP_STREAM_DESC \
	_stream->_vhost_app_name.CStr(), _stream->GetName().CStr(), _stream->GetId()

namespace pvd::rtmp
{
	// Check if the codec is supported when using E-RTMP
	bool IsSupportedCodecForERTMP(cmn::MediaCodecId codec_id)
	{
		switch (codec_id)
		{
			// Supported codecs
			OV_CASE_RETURN(cmn::MediaCodecId::H264, true);
			OV_CASE_RETURN(cmn::MediaCodecId::H265, true);
			OV_CASE_RETURN(cmn::MediaCodecId::Aac, true);

			// Unsupported codecs
			OV_CASE_RETURN(cmn::MediaCodecId::None, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Vp8, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Vp9, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Av1, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Flv, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Mp3, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Opus, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Jpeg, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Png, false);
			OV_CASE_RETURN(cmn::MediaCodecId::Webp, false);
		}
		return false;
	}

	// Extract `SoundFormat` from the metadata object
	std::optional<std::pair<cmn::MediaCodecId, ov::String>> SoundFormatFromMetadata(const modules::rtmp::AmfObjectArray *object)
	{
		const auto property_pair = object->GetPair("audiocodecid");

		if (property_pair == nullptr)
		{
			return std::nullopt;
		}

		const auto &property = property_pair->property;

		switch (property.GetType())
		{
			case modules::rtmp::AmfTypeMarker::String: {
				auto value = property.GetString();
				OV_IF_RETURN((value == "mp3" || value == ".mp3"), std::make_pair(cmn::MediaCodecId::Mp3, value));
				OV_IF_RETURN(value == "mp4a", std::make_pair(cmn::MediaCodecId::Aac, value));
				// Speex is not supported
				// if (value == "speex") { ... }
				break;
			}

			case modules::rtmp::AmfTypeMarker::Number: {
				auto value = static_cast<modules::flv::SoundFormat>(property.GetNumber());
				auto str_value = ov::String::FormatString("%u", static_cast<uint32_t>(value));

				switch (value)
				{
					OV_CASE_RETURN(modules::flv::SoundFormat::Mp3, std::make_pair(cmn::MediaCodecId::Mp3, str_value));
					OV_CASE_RETURN(modules::flv::SoundFormat::Aac, std::make_pair(cmn::MediaCodecId::Aac, str_value));
					default:
						break;
				}
				break;
			}

			default:
				break;
		}

		// Property exists but the codec is unknown
		return std::make_pair(cmn::MediaCodecId::None, "");
	}

	// Convert a `VideoFourCc` to a `MediaCodecId`
	std::optional<cmn::MediaCodecId> ToMediaCodecId(std::optional<modules::flv::VideoFourCc> four_cc)
	{
		if (four_cc.has_value())
		{
			switch (four_cc.value())
			{
				OV_CASE_RETURN(modules::flv::VideoFourCc::Vp8, cmn::MediaCodecId::Vp8);
				OV_CASE_RETURN(modules::flv::VideoFourCc::Vp9, cmn::MediaCodecId::Vp9);
				OV_CASE_RETURN(modules::flv::VideoFourCc::Av1, cmn::MediaCodecId::Av1);
				OV_CASE_RETURN(modules::flv::VideoFourCc::Avc, cmn::MediaCodecId::H264);
				OV_CASE_RETURN(modules::flv::VideoFourCc::Hevc, cmn::MediaCodecId::H265);
			}
		}
		return std::nullopt;
	}

	// Extract `VideoCodecId` from the metadata object
	std::optional<std::pair<cmn::MediaCodecId, ov::String>> VideoCodecIdFromMetadata(const modules::rtmp::AmfObjectArray *object)
	{
		const auto property_pair = object->GetPair("videocodecid");
		if (property_pair == nullptr)
		{
			return std::nullopt;
		}

		const auto &property = property_pair->property;
		switch (property.GetType())
		{
			case modules::rtmp::AmfTypeMarker::String: {
				auto value = property.GetString();
				OV_IF_RETURN((value == "avc1") || (value == "H264Avc"), std::make_pair(cmn::MediaCodecId::H264, value));
				break;
			}

			case modules::rtmp::AmfTypeMarker::Number: {
				auto value = static_cast<uint32_t>(property.GetNumber());
				auto str_value = ov::String::FormatString("%u", value);
				OV_IF_RETURN(value == 7, std::make_pair(cmn::MediaCodecId::H264, str_value));

				// Check the value as FourCC
				auto media_codec_id = ToMediaCodecId(static_cast<modules::flv::VideoFourCc>(value));
				OV_IF_RETURN(media_codec_id.has_value(), std::make_pair(media_codec_id.value(), str_value));
			}
			break;

			default:
				break;
		}

		// Property exists but the codec is unknown
		return std::make_pair(cmn::MediaCodecId::None, "");
	}

	std::optional<cmn::MediaCodecId> ToMediaCodecId(std::optional<modules::flv::VideoCodecId> video_codec_id)
	{
		if (video_codec_id.has_value() == false)
		{
			return std::nullopt;
		}

		if (video_codec_id.has_value())
		{
			switch (video_codec_id.value())
			{
				OV_CASE_RETURN(modules::flv::VideoCodecId::SorensonH263, std::nullopt);
				OV_CASE_RETURN(modules::flv::VideoCodecId::Screen, std::nullopt);
				OV_CASE_RETURN(modules::flv::VideoCodecId::On2VP6, std::nullopt);
				OV_CASE_RETURN(modules::flv::VideoCodecId::On2VP6A, std::nullopt);
				OV_CASE_RETURN(modules::flv::VideoCodecId::ScreenV2, std::nullopt);
				OV_CASE_RETURN(modules::flv::VideoCodecId::Avc, cmn::MediaCodecId::H264);
			}
		}
		return std::nullopt;
	}

	// Extract audio channels from the metadata object
	std::optional<cmn::AudioChannel::Layout> GetAudioChannelsFromMetadata(const modules::rtmp::AmfObjectArray *object)
	{
		auto property_pair = object->GetPair("audiochannels");

		if (property_pair != nullptr)
		{
			switch (property_pair->property.GetType())
			{
				case modules::rtmp::AmfTypeMarker::Number:
					switch (static_cast<int>(property_pair->property.GetNumber()))
					{
						OV_CASE_RETURN(1, cmn::AudioChannel::Layout::LayoutMono);
						OV_CASE_RETURN(2, cmn::AudioChannel::Layout::LayoutStereo);
						OV_CASE_RETURN(3, cmn::AudioChannel::Layout::Layout2Point1);
						OV_CASE_RETURN(4, cmn::AudioChannel::Layout::Layout4Point0);
						OV_CASE_RETURN(5, cmn::AudioChannel::Layout::Layout5Point0Back);
						OV_CASE_RETURN(6, cmn::AudioChannel::Layout::Layout5Point1Back);
						OV_CASE_RETURN(7, cmn::AudioChannel::Layout::Layout6Point1);
						OV_CASE_RETURN(8, cmn::AudioChannel::Layout::Layout7Point1);
					}
					break;

				case modules::rtmp::AmfTypeMarker::String: {
					auto value = property_pair->property.GetString();
					OV_IF_RETURN(value == "stereo", cmn::AudioChannel::Layout::LayoutStereo);
					OV_IF_RETURN(value == "mono", cmn::AudioChannel::Layout::LayoutMono);
					logtd("Unknown audiochannels value: %s", value.CStr());
				}

				default:
					break;
			}
		}
		else
		{
			OV_IF_RETURN(object->GetBoolean("2.1").value_or(false), cmn::AudioChannel::Layout::Layout2Point1);
			OV_IF_RETURN(object->GetBoolean("3.1").value_or(false), cmn::AudioChannel::Layout::Layout3Point1);
			OV_IF_RETURN(object->GetBoolean("4.0").value_or(false), cmn::AudioChannel::Layout::Layout4Point0);
			OV_IF_RETURN(object->GetBoolean("4.1").value_or(false), cmn::AudioChannel::Layout::Layout4Point1);
			OV_IF_RETURN(object->GetBoolean("5.1").value_or(false), cmn::AudioChannel::Layout::Layout5Point1);
			OV_IF_RETURN(object->GetBoolean("7.1").value_or(false), cmn::AudioChannel::Layout::Layout7Point1);

			auto stereo = object->GetBoolean("stereo");
			OV_IF_RETURN(stereo.has_value(), stereo.value() ? cmn::AudioChannel::Layout::LayoutStereo : cmn::AudioChannel::Layout::LayoutMono);
		}

		return std::nullopt;
	}

	std::optional<cmn::MediaCodecId> ToMediaCodecId(std::optional<modules::flv::SoundFormat> sound_format)
	{
		if (sound_format.has_value() == false)
		{
			return std::nullopt;
		}

		switch (sound_format.value())
		{
			OV_CASE_RETURN(modules::flv::SoundFormat::LPcmPlatformEndian, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::AdPcm, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Mp3, cmn::MediaCodecId::Mp3);
			OV_CASE_RETURN(modules::flv::SoundFormat::LPcmLittleEndian, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Nellymoser16KMono, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Nellymoser8KMono, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Nellymoser, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::G711ALaw, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::G711MuLaw, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::ExHeader, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Aac, cmn::MediaCodecId::Aac);
			OV_CASE_RETURN(modules::flv::SoundFormat::Speex, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Mp3_8K, std::nullopt);
			OV_CASE_RETURN(modules::flv::SoundFormat::Native, std::nullopt);
		}

		return std::nullopt;
	}

	RtmpChunkHandler::Stats::Stats()
	{
		ResetCheckTime();
	}

	int64_t RtmpChunkHandler::Stats::GetElapsedInMs() const
	{
		return ov::Time::GetTimestampInMs() - last_stream_check_time;
	}

	void RtmpChunkHandler::Stats::ResetCheckTime()
	{
		last_stream_check_time = ov::Time::GetTimestampInMs();

		video_frame_count = 0;
		audio_frame_count = 0;
		previous_last_video_timestamp = last_video_timestamp;
		previous_last_audio_timestamp = last_audio_timestamp;
	}

	int32_t RtmpChunkHandler::Stats::GetVADelta() const
	{
		return last_video_timestamp - last_audio_timestamp;
	}

	ov::String RtmpChunkHandler::Stats::GetStatsString(int64_t elapsed_ms) const
	{
		return ov::String::FormatString(
			"keyint_ms(%u) ts_ms(v:%u/a:%u/v-a:%d) fps(v:%.2f/a:%.2f) gap_ms(v:%u/a:%u)",
			key_frame_interval,
			last_video_timestamp, last_audio_timestamp, GetVADelta(),
			((video_frame_count * 1000) / static_cast<double>(elapsed_ms)),
			((audio_frame_count * 1000) / static_cast<double>(elapsed_ms)),
			last_audio_timestamp - previous_last_audio_timestamp,
			last_video_timestamp - previous_last_video_timestamp);
	}

	ov::String RtmpChunkHandler::MediaInfo::ToString() const
	{
		return ov::String::FormatString(
			"Audio: %s (%d, raw: %s), layout: %s (%d), bits: %d, samplerate: %d, sampleindex: %d, bitrate: %d\n"
			"Video: %s (%d, raw: %s), width: %d, height: %d, framerate: %.2f, bitrate: %d",
			// Audio information
			audio_codec_id.has_value() ? cmn::GetCodecIdString(audio_codec_id.value()) : "-",
			audio_codec_id.has_value() ? static_cast<int>(audio_codec_id.value()) : 0,
			audio_codec_raw.CStr(),
			audio_channel_layout.has_value() ? cmn::AudioChannel::GetLayoutName(audio_channel_layout.value()) : "-",
			audio_channel_layout.has_value() ? static_cast<int>(audio_channel_layout.value()) : 0,
			audio_bits.value_or(0),
			audio_samplerate.value_or(0),
			audio_sampleindex.value_or(0),
			audio_bitrate.value_or(0),
			// Video information
			video_codec_id.has_value() ? cmn::GetCodecIdString(video_codec_id.value()) : "-",
			video_codec_id.has_value() ? static_cast<int>(video_codec_id.value()) : 0,
			video_codec_raw.CStr(),
			video_width,
			video_height,
			video_framerate,
			video_bitrate);
	}

	RtmpChunkHandler::RtmpChunkHandler(RtmpStreamV2 *stream)
		: _stream(stream),
		  _chunk_parser(DEFAULT_CHUNK_SIZE),
		  _chunk_writer(DEFAULT_CHUNK_SIZE)
	{
	}

	std::shared_ptr<modules::rtmp::ChunkWriteInfo> RtmpChunkHandler::CreateUserControlMessage(modules::rtmp::UserControlEventType message_id, size_t payload_length)
	{
		// RTMP uses message type ID 4 for User Control messages.
		// These messages contain information used by the RTMP streaming layer.
		// Protocol messages with IDs 1, 2, 3, 5, and 6 are used by the RTMP Chunk Stream protocol (Section 5.4).
		//
		// User Control messages SHOULD use message stream ID 0 (known as the control stream) and, when sent over RTMP Chunk Stream, be sent on chunk stream ID 2.
		// User Control messages are effective at the point they are received in the stream; their // timestamps are ignored.
		//
		// The client or the server sends this message to notify the peer about the user control events. This message carries Event type and Event data.
		//
		// +------------------------------+-------------------------
		// |      Event Type (16 bits)    | Event Data
		// +------------------------------+-------------------------
		// Payload for the ‘User Control’ protocol message
		//
		// The first 2 bytes of the message data are used to identify the Event type. Event type is followed by Event data.
		// The size of Event Data field is variable. However, in cases where the message has to pass through the RTMP Chunk Stream layer,
		// the maximum chunk size (Section 5.4.1) SHOULD be large enough to allow these messages to fit in a single chunk.
		//
		// Event Types are and their Event Data formats are enumerated in Section 7.1.7.
		auto chunk_write_info = modules::rtmp::ChunkWriteInfo::Create(
			modules::rtmp::ChunkStreamId::Urgent,
			modules::rtmp::MessageTypeID::UserControl,
			0,
			sizeof(uint16_t) + payload_length);

		chunk_write_info->AppendPayload(message_id);

		return chunk_write_info;
	}

	bool RtmpChunkHandler::SendMessage(const std::shared_ptr<const modules::rtmp::ChunkWriteInfo> &chunk_write_info)
	{
		return _stream->SendData(_chunk_writer.Serialize(chunk_write_info));
	}

	bool RtmpChunkHandler::SendAmfCommand(const std::shared_ptr<modules::rtmp::ChunkWriteInfo> &chunk_write_info, const modules::rtmp::AmfDocument &document)
	{
		if (chunk_write_info == nullptr)
		{
			return false;
		}

		ov::ByteStream stream(2048);
		if (document.Encode(stream) == false)
		{
			return false;
		}

		chunk_write_info->AppendPayload(stream.GetData());

		return SendMessage(chunk_write_info);
	}

	bool RtmpChunkHandler::SendWindowAcknowledgementSize(uint32_t size)
	{
		// The client or the server sends this message to inform the peer of the window size to use between sending acknowledgments.
		// The sender expects acknowledgment from its peer after the sender sends window size bytes.
		// The receiving peer MUST send an Acknowledgement (Section 5.4.3) after receiving the indicated number of bytes since the last Acknowledgement was sent,
		// or from the beginning of the session if no Acknowledgement has yet been sent.
		//
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |            Acknowledgement Window size (4 bytes)              |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// Payload for the ‘Window Acknowledgement Size’ protocol message

		auto chunk_write_info = modules::rtmp::ChunkWriteInfo::Create(
			modules::rtmp::ChunkStreamId::Urgent,
			modules::rtmp::MessageTypeID::WindowAcknowledgementSize,
			_stream->_rtmp_stream_id);

		chunk_write_info->AppendPayload(ov::HostToBE32(size));

		return SendMessage(chunk_write_info);
	}

	bool RtmpChunkHandler::SendSetPeerBandwidth(uint32_t bandwidth)
	{
		// The client or the server sends this message to limit the output bandwidth of its peer.
		// The peer receiving this message limits its output bandwidth by limiting the amount of sent but unacknowledged data to the window size indicated in this message.
		// The peer receiving this message SHOULD respond with a Window Acknowledgement Size message if the window size is different from the last one sent to the sender of this message.
		//
		//  0                   1                   2                   3
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |            Acknowledgement Window size (4 bytes)              |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |  Limit Type   |
		// +-+-+-+-+-+-+-+-+
		// Payload for the ‘Set Peer Bandwidth’ protocol message
		// The Limit Type is one of the following values:
		//
		// 0 - Hard: The peer SHOULD limit its output bandwidth to the indicated window size.
		// 1 - Soft: The peer SHOULD limit its output bandwidth to the the window indicated in this message or the limit already in effect, whichever is smaller.
		// 2 - Dynamic: If the previous Limit Type was Hard, treat this message as though it was marked Hard, otherwise ignore this message.
		auto chunk_write_info = modules::rtmp::ChunkWriteInfo::Create(
			modules::rtmp::ChunkStreamId::Urgent,
			modules::rtmp::MessageTypeID::SetPeerBandwidth,
			_stream->_rtmp_stream_id,
			sizeof(uint32_t) + sizeof(uint8_t));

		chunk_write_info->AppendPayload(ov::HostToBE32(bandwidth));
		chunk_write_info->AppendPayload(modules::rtmp::PeerBandwidthLimitType::Dynamic);

		return SendMessage(chunk_write_info);
	}

	bool RtmpChunkHandler::SendStreamBegin(uint32_t stream_id)
	{
		auto chunk_write_info = CreateUserControlMessage(
			modules::rtmp::UserControlEventType::StreamBegin,
			sizeof(uint32_t));
		chunk_write_info->AppendPayload(ov::HostToBE32(stream_id));

		return SendMessage(chunk_write_info);
	}

	bool RtmpChunkHandler::SendStreamEnd()
	{
		auto chunk_write_info = CreateUserControlMessage(
			modules::rtmp::UserControlEventType::StreamEof,
			sizeof(uint32_t));
		chunk_write_info->AppendPayload(ov::HostToBE32(_stream->_rtmp_stream_id));

		return SendMessage(chunk_write_info);
	}

	bool RtmpChunkHandler::SendAmfConnectSuccess(uint32_t chunk_stream_id, double transaction_id, double object_encoding)
	{
		return SendAmfCommand(
			modules::rtmp::ChunkWriteInfo::Create(chunk_stream_id),
			modules::rtmp::AmfDocumentBuilder()
				// _result
				.Append(modules::rtmp::EnumToString(modules::rtmp::Command::AckResult))
				.Append(transaction_id)
				// properties
				.Append(modules::rtmp::AmfObjectBuilder()
							.Append("fmsVer", "FMS/3,5,2,654")
							.Append("capabilities", 31.0)
							.Append("mode", 1.0)
							.Build())

				// information
				.Append(modules::rtmp::AmfObjectBuilder()
							.Append("level", "status")
							.Append("code", "NetConnection.Connect.Success")
							.Append("description", "Connection succeeded.")
							.Append("clientid", _stream->GetId())
							.Append("objectEncoding", object_encoding)
							.Append("data",
									modules::rtmp::AmfEcmaArrayBuilder()
										.Append("version", "3,5,2,654")
										.Build())
							.Build())
				.Build());
	}

	bool RtmpChunkHandler::SendAmfAckResult(uint32_t chunk_stream_id, double transaction_id)
	{
		_stream->_rtmp_stream_id = 1;

		return SendAmfCommand(
			modules::rtmp::ChunkWriteInfo::Create(chunk_stream_id),
			modules::rtmp::AmfDocumentBuilder()
				.Append(modules::rtmp::EnumToString(modules::rtmp::Command::AckResult))
				.Append(transaction_id)
				.Append(modules::rtmp::AmfProperty::NullProperty())
				.Append(_stream->_rtmp_stream_id)
				.Build());
	}

	bool RtmpChunkHandler::SendAmfOnFCPublish(uint32_t chunk_stream_id, uint32_t stream_id, double client_id)
	{
		return SendAmfCommand(
			modules::rtmp::ChunkWriteInfo::Create(chunk_stream_id, _stream->_rtmp_stream_id),
			modules::rtmp::AmfDocumentBuilder()
				.Append(modules::rtmp::EnumToString(modules::rtmp::Command::OnFCPublish))
				.Append(0.0)
				.Append(modules::rtmp::AmfProperty::NullProperty())
				.Append(modules::rtmp::AmfObjectBuilder()
							.Append("level", "status")
							.Append("code", "NetStream.Publish.Start")
							.Append("description", "FCPublish")
							.Append("clientid", client_id)
							.Build())
				.Build());
	}

	bool RtmpChunkHandler::SendAmfOnStatus(uint32_t chunk_stream_id, uint32_t stream_id, const char *level, const char *code, const char *description, double client_id)
	{
		return SendAmfCommand(
			modules::rtmp::ChunkWriteInfo::Create(chunk_stream_id, stream_id),
			modules::rtmp::AmfDocumentBuilder()
				.Append(modules::rtmp::EnumToString(modules::rtmp::Command::OnStatus))
				.Append(0.0)
				.Append(modules::rtmp::AmfProperty::NullProperty())
				.Append(
					modules::rtmp::AmfObjectBuilder()
						.Append("level", level)
						.Append("code", code)
						.Append("description", description)
						.Append("clientid", client_id)
						.Build())
				.Build());
	}

	void RtmpChunkHandler::SetVhostAppName(const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
	{
		// If the queue inside `_chunk_parser` becomes full before `ValidatePublishUrl()` is called,
		// the app/stream name is set here to provide the best possible hint about which queue it is.
		// This will later be updated using `final_url` in `ValidatePublishUrl()`.
		auto queue_name = ov::String::FormatString("RTMP queue for %s/%s", vhost_app_name.CStr(), stream_name.CStr());

		_chunk_parser.SetMessageQueueAlias(queue_name.CStr());
	}

	int RtmpChunkHandler::GetWaitingTrackCount() const
	{
		return (_waiting_audio_track_count + _waiting_video_track_count);
	}

	bool RtmpChunkHandler::OnAmfConnect(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, modules::rtmp::AmfDocument &document, double transaction_id)
	{
		double object_encoding = 0.0;

		logtd("Received RTMP document\n%s", document.ToString().CStr());

		auto meta_property = document.GetObject(2);
		if (meta_property == nullptr)
		{
			return false;
		}

		auto object = meta_property->GetObject();

		object_encoding = object->GetNumber("objectEncoding").value_or(object_encoding);

		// Instead of extracting `app` from the `object`, we will extract it from the actual streaming URL included in `tcUrl`.
		// auto app_name = object->GetString("app").value_or("")

		// Notify the app name and `tcUrl` to the stream.
		if (_stream->UpdateConnectInfo(
				object->GetString("tcUrl").value_or("")) == false)
		{
			return false;
		}

		if (SendWindowAcknowledgementSize(DEFAULT_ACKNOWNLEDGEMENT_SIZE) == false)
		{
			logae("Failed to send WindowAcknowledgementSize(%u)", DEFAULT_ACKNOWNLEDGEMENT_SIZE);
			return false;
		}

		if (SendSetPeerBandwidth(DEFAULT_PEER_BANDWIDTH) == false)
		{
			logae("Failed to send SetPeerBandwidth(%u)", DEFAULT_PEER_BANDWIDTH);
			return false;
		}

		if (SendStreamBegin(0) == false)
		{
			logae("Failed to send StreamBegin(0)");
			return false;
		}

		if (SendAmfConnectSuccess(header->basic_header.chunk_stream_id, transaction_id, object_encoding) == false)
		{
			logae("Failed to send AmfConnectResult(%u, %.2f, %.2f)",
				  header->basic_header.chunk_stream_id,
				  transaction_id,
				  object_encoding);

			return false;
		}

		// Assume that there is one audio track and one video track for now
		_waiting_audio_track_count = 1;
		_waiting_video_track_count = 1;

		_media_info.ignore_audio_packets = false;
		_media_info.ignore_video_packets = false;

		return true;
	}

	bool RtmpChunkHandler::OnAmfCreateStream(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, modules::rtmp::AmfDocument &document, double transaction_id)
	{
		if (SendAmfAckResult(header->basic_header.chunk_stream_id, transaction_id) == false)
		{
			logae("OnAmfCreateStream - Failed to send AmfCreateStreamResult(%u, %.2f)",
				  header->basic_header.chunk_stream_id,
				  transaction_id);

			return false;
		}

		return true;
	}

	bool RtmpChunkHandler::OnAmfDeleteStream(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, modules::rtmp::AmfDocument &document, double transaction_id)
	{
		logad("OnAmfDeleteStream - Delete Stream");

		_media_info.ignore_audio_packets = true;
		_media_info.ignore_video_packets = true;

		// it will call PhysicalPort::OnDisconnected
		_stream->_remote->Close();

		return true;
	}

	bool RtmpChunkHandler::OnAmfPublish(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, modules::rtmp::AmfDocument &document, double transaction_id)
	{
		if (_stream->GetName().IsEmpty())
		{
			auto property = document.Get(3, modules::rtmp::AmfTypeMarker::String);
			if (property == nullptr)
			{
				logae("OnAmfPublish - No stream name provided");

				// Reject
				SendAmfOnStatus(header->basic_header.chunk_stream_id,
								_stream->_rtmp_stream_id,
								"error",
								"NetStream.Publish.Rejected",
								"Authentication Failed.",
								_stream->GetChannelId());

				return false;
			}

			if (_stream->PostPublish(document) == false)
			{
				logae("OnAmfPublish - Failed to post publish for stream: %s", _stream->GetName().CStr());
				return false;
			}
		}

		if (SendStreamBegin(_stream->_rtmp_stream_id) == false)
		{
			logae("OnAmfPublish - Failed to send StreamBegin(%u)", _stream->_rtmp_stream_id);
			return false;
		}

		if (SendAmfOnStatus(header->basic_header.chunk_stream_id,
							_stream->_rtmp_stream_id,
							"status",
							"NetStream.Publish.Start",
							"Publishing",
							_stream->GetChannelId()) == false)
		{
			logae("OnAmfPublish - Failed to send AmfOnStatus(%u, %u, %s, %s)",
				  header->basic_header.chunk_stream_id,
				  _stream->_rtmp_stream_id,
				  "status",
				  "NetStream.Publish.Start");
			return false;
		}

		return true;
	}

	bool RtmpChunkHandler::OnAmfFCPublish(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, modules::rtmp::AmfDocument &document, double transaction_id)
	{
		if (_stream->GetName().IsEmpty())
		{
			auto property = document.Get(3, modules::rtmp::AmfTypeMarker::String);

			if (property == nullptr)
			{
				logae("OnAmfFCPublish - No stream name provided");
				return false;
			}

			// TODO: check if the chunk stream id is already exist, and generates new rtmp_stream_id and client_id.
			if (SendAmfOnFCPublish(header->basic_header.chunk_stream_id, _stream->_rtmp_stream_id, _stream->GetChannelId()) == false)
			{
				logae("OnAmfFCPublish - Failed to send AmfOnFCPublish(%u, %u)",
					  header->basic_header.chunk_stream_id,
					  _stream->_rtmp_stream_id);

				return false;
			}

			return _stream->PostPublish(document);
		}

		return true;
	}

	bool RtmpChunkHandler::OnAmfMetadata(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, const modules::rtmp::AmfProperty *property)
	{
		const modules::rtmp::AmfObjectArray *object = nullptr;

		switch (property->GetType())
		{
			case modules::rtmp::AmfTypeMarker::Object:
				object = property->GetObject();
				break;

			case modules::rtmp::AmfTypeMarker::EcmaArray:
				object = property->GetEcmaArray();
				break;

			default:
				logae("OnAmfMetadata - Invalid type of metadata: %d", property->GetType());
				return false;
		}

		// An AmfDocument example of onMetadata
		//
		// {
		//     "@setDataFrame",
		//     "onMetaData",
		//     [
		//         duration: 0.0  -------------------------- Common meta
		//         width: 480.0  -----------+
		//         height: 360.0            |
		//         videodatarate: 216.0     |- Video meta
		//         framerate: 30.0          |
		//         videocodecid: 7.0  ------+
		//         audiodatarate: 8.0   --------+
		//         audiosamplerate: 22050.0     |
		//         audiosamplesize: 16.0        |- Audio meta
		//         stereo: false  (Boolean)     |
		//         audiocodecid: 10.0  ---------+
		//         major_brand: "isom"  ------------------+
		//         minor_version: "512"                   |
		//         compatible_brands: "isomiso2avc1mp41"  |- Common meta
		//         encoder: "Lavf61.1.100"                |
		//         filesize: 0.0  ------------------------+
		//     ]
		// }

		// DeviceType (Ex: "Lavf61.1.100")
		_media_info.encoder_name = object->GetString("videodevice", "encoder").value_or("");
		_media_info.encoder_type = ToEncoderType(_media_info.encoder_name);

		_waiting_audio_track_count = 0;
		_waiting_video_track_count = 0;

		// Audio Codec (Ex: 10.0)
		auto sound_format = SoundFormatFromMetadata(object);
		if (sound_format.has_value())
		{
			_media_info.audio_codec_id = sound_format->first;
			_media_info.audio_codec_raw = sound_format->second;

			if (_media_info.IsSupportedAudio())
			{
				auto value = _media_info.audio_codec_id.value();
				if (IsSupportedCodecForERTMP(value) == false)
				{
					logae("Not supported audio codec: %s(%d) (raw: %s)",
						  cmn::GetCodecIdString(value), value,
						  _media_info.audio_codec_raw.CStr());
					_media_info.audio_codec_id = cmn::MediaCodecId::None;
				}
			}
		}
		else
		{
			_media_info.audio_codec_id = std::nullopt;
		}

		// NOTE: FFmpeg and OBS send AVC codec information in the metadata even when sending HEVC
		// So, we need to check the actual codec when we receive audio/video data later
		// Video Codec (Ex: 7.0)
		auto video_codec_id = VideoCodecIdFromMetadata(object);
		if (video_codec_id.has_value())
		{
			_media_info.video_codec_id = video_codec_id->first;
			_media_info.video_codec_raw = video_codec_id->second;

			if (_media_info.IsSupportedVideo())
			{
				auto value = _media_info.video_codec_id.value();
				if (IsSupportedCodecForERTMP(value) == false)
				{
					logae("Not supported video codec: %s(%d) (raw: %s)", cmn::GetCodecIdString(value), value, _media_info.video_codec_raw.CStr());
					_media_info.video_codec_id = cmn::MediaCodecId::None;
				}
			}
		}
		else
		{
			_media_info.video_codec_id = std::nullopt;
		}

		if (_media_info.HasAudio())
		{
			// Even if the codec is not supported, we try to collect as much information as possible
			_media_info.audio_bitrate = static_cast<int32_t>(object->GetNumber("audiodatarate", "audiobitrate").value_or(0.0));
			_media_info.audio_channel_layout = GetAudioChannelsFromMetadata(object);
			_media_info.audio_bits = static_cast<int32_t>(object->GetNumber("audiosamplesize").value_or(0.0));
			_media_info.audio_samplerate = static_cast<int32_t>(object->GetNumber("audiosamplerate").value_or(0.0));

			if (_media_info.IsSupportedAudio())
			{
				_waiting_audio_track_count++;
			}
		}

		if (_media_info.HasVideo())
		{
			// Even if the codec is not supported, we try to collect as much information as possible
			_media_info.video_width = static_cast<int32_t>(object->GetNumber("width").value_or(0.0));
			_media_info.video_height = static_cast<int32_t>(object->GetNumber("height").value_or(0.0));
			_media_info.video_framerate = object->GetNumber("framerate", "videoframerate").value_or(0.0);
			_media_info.video_bitrate = static_cast<int32_t>(object->GetAsNumber("videodatarate", "bitrate", "maxBitrate").value_or(0.0));

			if (_media_info.IsSupportedVideo())
			{
				_waiting_video_track_count++;
			}
		}

		if (_media_info.IsNoAudioVideo())
		{
			logaw("There is no video/audio codec information in the metadata\n%s",
				  property->ToString().CStr());

			// Assume that there is one audio track and one video track if no information is provided
			_waiting_audio_track_count = 1;
			_waiting_video_track_count = 1;
		}

		logtd("Media Info: %s", _media_info.ToString().CStr());

		return true;
	}

	void RtmpChunkHandler::GenerateEvent(const cfg::vhost::app::pvd::Event &event, const ov::String &value)
	{
		logad("Event generated: %s / %s", event.GetTrigger().CStr(), value.CStr());

		bool id3_enabled = false;
		auto id3v2_event = event.GetHLSID3v2(&id3_enabled);

		if (id3_enabled == true)
		{
			ID3v2 tag;
			tag.SetVersion(4, 0);

			if (id3v2_event.GetFrameType() == "TXXX")
			{
				auto info = id3v2_event.GetInfo();
				auto data = id3v2_event.GetData();

				if (info == "${TriggerValue}")
				{
					info = value;
				}
				else if (data == "${TriggerValue}")
				{
					data = value;
				}

				tag.AddFrame(std::make_shared<ID3v2TxxxFrame>(info, data));
			}
			else if (id3v2_event.GetFrameType().UpperCaseString().Get(0) == 'T')
			{
				auto data = id3v2_event.GetData();

				if (data == "${TriggerValue}")
				{
					data = value;
				}

				tag.AddFrame(std::make_shared<ID3v2TextFrame>(id3v2_event.GetFrameType(), data));
			}
			// PRIV
			else if (id3v2_event.GetFrameType() == "PRIV")
			{
				auto owner = id3v2_event.GetInfo();
				auto data = id3v2_event.GetData();

				if (owner == "${TriggerValue}")
				{
					owner = value;
				}
				else if (data == "${TriggerValue}")
				{
					data = value;
				}

				tag.AddFrame(std::make_shared<ID3v2PrivFrame>(owner, data));
			}
			else
			{
				logaw("Unsupported ID3v2 frame type: %s", id3v2_event.GetFrameType().CStr());
				return;
			}

			cmn::PacketType packet_type = cmn::PacketType::Unknown;
			{
				auto event_type = id3v2_event.GetEventType().LowerCaseString();

				if (event_type == "video")
				{
					packet_type = cmn::PacketType::VIDEO_EVENT;
				}
				else if (id3v2_event.GetEventType().LowerCaseString() == "audio")
				{
					packet_type = cmn::PacketType::AUDIO_EVENT;
				}
				else if (id3v2_event.GetEventType().LowerCaseString() == "event")
				{
					packet_type = cmn::PacketType::EVENT;
				}
				else
				{
					logaw("Unsupported inject type: %s", id3v2_event.GetEventType().CStr());
					return;
				}
			}

			int64_t pts = 0;

			if (packet_type == cmn::PacketType::VIDEO_EVENT)
			{
				pts = _last_video_pts + _last_video_pts_clock.Elapsed();
			}
			else if (packet_type == cmn::PacketType::AUDIO_EVENT)
			{
				pts = _last_audio_pts + _last_audio_pts_clock.Elapsed();
			}

			_stream->SendDataFrame(pts, cmn::BitstreamFormat::ID3v2, packet_type, tag.Serialize(), false);
		}
	}

	bool RtmpChunkHandler::CheckEvent(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, modules::rtmp::AmfDocument &document)
	{
		bool found = false;

		for (const auto &event : _event_generator_config.GetEvents())
		{
			auto trigger = event.GetTrigger();
			auto trigger_list = trigger.Split(".");

			// Trigger length must be 3 or more
			// AMFDataMessage.[<Property>.<Property>...<Property>.]<Object Name>.<Key Name>
			if (trigger_list.size() < 3)
			{
				logad("Invalid trigger: %s", trigger.CStr());
				continue;
			}

			if (
				(header->completed.type_id == modules::rtmp::MessageTypeID::Amf0Data) &&
				(trigger_list.at(0) == "AMFDataMessage"))
			{
				auto count = trigger_list.size();

				for (std::size_t size = 1; size < count; size++)
				{
					auto property = document.Get(size - 1);

					if (property == nullptr)
					{
						logad("Document has no property at %zu: %s", size - 1, trigger.CStr());
						break;
					}

					if (size == (count - 1))
					{
						auto type = property->GetType();

						if ((type == modules::rtmp::AmfTypeMarker::Object) || (type == modules::rtmp::AmfTypeMarker::EcmaArray))
						{
							const modules::rtmp::AmfObjectArray *object_array = property->GetObject();

							if (object_array == nullptr)
							{
								object_array = property->GetEcmaArray();
							}

							if (object_array == nullptr)
							{
								logad("Property is not an object: %s", property->GetString().CStr());
								break;
							}

							auto key = trigger_list.at(size);
							auto property_value = object_array->GetString(key);

							if (property_value.has_value())
							{
								found = true;
								GenerateEvent(event, *property_value);
							}
						}
						else if (property->GetType() == modules::rtmp::AmfTypeMarker::String)
						{
							found = true;
							GenerateEvent(event, property->GetString());
						}
						else
						{
							logad("Document property type mismatch at %d: %s", size - 1, property->GetString().CStr());
							break;
						}
					}
					else
					{
						if (trigger_list.at(size) != property->GetString())
						{
							logad("Document property mismatch at %d: %s != %s", size - 1, trigger_list.at(size).CStr(), property->GetString().CStr());
							break;
						}
					}
				}
			}
		}

		return found;
	}

	bool RtmpChunkHandler::SendAcknowledgementSize(uint32_t acknowledgement_traffic)
	{
		auto chunk_write_info = modules::rtmp::ChunkWriteInfo::Create(
			modules::rtmp::ChunkStreamId::Urgent,
			modules::rtmp::MessageTypeID::Acknowledgement,
			0);

		chunk_write_info->AppendPayload(ov::HostToBE32(acknowledgement_traffic));

		return SendMessage(chunk_write_info);
	}

	void RtmpChunkHandler::AccumulateAcknowledgementSize(size_t data_size)
	{
		_acknowledgement_traffic += data_size;

		if (_acknowledgement_traffic >= INT_MAX)
		{
			// Rolled
			_acknowledgement_traffic -= INT_MAX;
		}

		_acknowledgement_traffic_after_last_acked += data_size;

		if (_acknowledgement_traffic_after_last_acked > _acknowledgement_size)
		{
			SendAcknowledgementSize(_acknowledgement_traffic);
			_acknowledgement_traffic_after_last_acked -= _acknowledgement_size;
		}
	}

	bool RtmpChunkHandler::HandleAmf0Command(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		OV_ASSERT2(message->header != nullptr);
		OV_ASSERT2(message->payload != nullptr);
		OV_ASSERT2(message->payload->GetLength() == message->header->message_length);

		ov::ByteStream byte_stream(message->payload);
		modules::rtmp::AmfDocument document;

		if (document.Decode(byte_stream) == false)
		{
			logae("Could not decode AMF document");
			return false;
		}

		// Message Name
		ov::String message_name = document.GetString(0).value_or("");
		// Obtain the Message Transaction ID
		double transaction_id = document.GetNumber(1).value_or(0.0);

		switch (modules::rtmp::ToCommand(message_name))
		{
			OV_CASE_RETURN(modules::rtmp::Command::Connect, OnAmfConnect(message->header, document, transaction_id));
			OV_CASE_RETURN(modules::rtmp::Command::CreateStream, OnAmfCreateStream(message->header, document, transaction_id));
			// OV_CASE_RETURN(modules::rtmp::Command::DeleteStream, OnAmfDeleteStream(message->header, document, transaction_id);
			OV_CASE_RETURN(modules::rtmp::Command::DeleteStream, true);
			OV_CASE_RETURN(modules::rtmp::Command::Publish, OnAmfPublish(message->header, document, transaction_id));
			OV_CASE_RETURN(modules::rtmp::Command::ReleaseStream, true);
			OV_CASE_RETURN(modules::rtmp::Command::FCPublish, OnAmfFCPublish(message->header, document, transaction_id));
			//TODO(Dimiden): Check this message, This causes the stream to be deleted twice.
			OV_CASE_RETURN(modules::rtmp::Command::FCUnpublish, true);
			OV_CASE_RETURN(modules::rtmp::Command::Ping, true);

			default:
				// Commands not handled by OME are not treated as errors but simply ignored
				logaw("Not handled Amf0CommandMessage: %s (%.1f)", message_name.CStr(), transaction_id);
				break;
		}

		return true;
	}

	bool RtmpChunkHandler::HandleSetChunkSize(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		auto chunk_size = message->ReadPayloadAsU32();
		logai("ChunkSize is changed to %u (stream id: %u)", chunk_size, message->header->completed.stream_id);
		_chunk_parser.SetChunkSize(chunk_size);
		return true;
	}

	bool RtmpChunkHandler::HandleAcknowledgement(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		// OME doesn't use this message
		return true;
	}

	bool RtmpChunkHandler::HandleUserControl(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		auto data = message->payload;

		// RTMP Spec. v1.0 - 6.2. User Control Messages
		//
		// User Control messages SHOULD use message stream ID 0 (known as the
		// control stream) and, when sent over RTMP Chunk Stream, be sent on
		// chunk stream ID 2.
		auto message_stream_id = message->header->completed.stream_id;
		auto chunk_stream_id = message->header->basic_header.chunk_stream_id;
		if (
			(message_stream_id != 0) ||
			(chunk_stream_id != 2))
		{
			logae("Invalid id (message stream id: %u, chunk stream id: %u)", message_stream_id, chunk_stream_id);
			return false;
		}

		if (data->GetLength() < 2)
		{
			logae("Invalid user control message size (data length must greater than 2 bytes, but %zu)", data->GetLength());
			return false;
		}

		ov::ByteStream byte_stream(data);
		auto type = byte_stream.ReadBE16As<modules::rtmp::UserControlEventType>();

		switch (type)
		{
			case modules::rtmp::UserControlEventType::PingRequest: {
				if (byte_stream.IsRemained(sizeof(uint32_t)) == false)
				{
					logae("Invalid ping message size: %zu", data->GetLength());
					return false;
				}

				// +---------------+--------------------------------------------------+
				// | PingResponse  | The client sends this event to the server in     |
				// | (=7)          | response to the ping request. The event data is  |
				// |               | a 4-byte timestamp, which was received with the  |
				// |               | PingRequest request.                             |
				// +---------------+--------------------------------------------------+
				// ping response == event type (16 bits) + timestamp (32 bits)
				auto chunk_write_info = modules::rtmp::ChunkWriteInfo::Create(
					chunk_stream_id,
					modules::rtmp::MessageTypeID::UserControl,
					message_stream_id,
					sizeof(uint16_t) + sizeof(uint32_t));

				chunk_write_info->AppendPayload(modules::rtmp::UserControlEventType::PingResponse);
				chunk_write_info->AppendPayload(byte_stream.ReadBE32());

				OV_ASSERT2(chunk_write_info->GetPayloadLength() == 6);

				return SendMessage(chunk_write_info);
			}

			default:
				break;
		}

#if DEBUG
		logaw("Not handled user control message type: %s (%d)",
			  modules::rtmp::EnumToString(type),
			  ov::ToUnderlyingType(type));
#endif

		return true;
	}

	bool RtmpChunkHandler::HandleWindowAcknowledgementSize(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		auto acknowledgement_size = message->ReadPayloadAsU32();

		if (acknowledgement_size != 0)
		{
			_acknowledgement_size = acknowledgement_size / 2;
		}

		return true;
	}

	bool RtmpChunkHandler::HandleAmf0Data(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		OV_ASSERT2(message->payload->GetLength() == message->header->message_length);

		ov::ByteStream byte_stream(message->payload);
		modules::rtmp::AmfDocument document;
		auto decode_length = document.Decode(byte_stream);

		if (decode_length == 0)
		{
			logaw("Amf0DataMessage Document Length 0");
			return true;
		}

		logad("Received RTMP metadata\n%s", document.ToString().CStr());

		// Obtain the message name
		ov::String message_name;
		auto message_name_property = document.Get(0);
		auto message_name_type = (message_name_property != nullptr) ? message_name_property->GetType() : modules::rtmp::AmfTypeMarker::Undefined;
		if (message_name_type == modules::rtmp::AmfTypeMarker::String)
		{
			message_name = message_name_property->GetString();
		}

		// Obtain the data name
		ov::String data_name;
		auto data_name_property = document.Get(1);
		auto data_name_type = (data_name_property != nullptr) ? data_name_property->GetType() : modules::rtmp::AmfTypeMarker::Undefined;
		if (data_name_type == modules::rtmp::AmfTypeMarker::String)
		{
			data_name = data_name_property->GetString();
		}

		auto third_property = document.Get(2);
		auto third_type = (third_property != nullptr) ? third_property->GetType() : modules::rtmp::AmfTypeMarker::Undefined;

		switch (modules::rtmp::ToCommand(message_name))
		{
			case modules::rtmp::Command::SetDataFrame:
				if (::strcmp(data_name, modules::rtmp::EnumToString(modules::rtmp::Command::OnMetaData)) == 0)
				{
					if ((third_type == modules::rtmp::AmfTypeMarker::Object) || (third_type == modules::rtmp::AmfTypeMarker::EcmaArray))
					{
						OnAmfMetadata(message->header, third_property);
					}
					else
					{
						logaw("Data type is not object or ecma array");
					}
				}
				break;

			case modules::rtmp::Command::OnFI:
				// Not supported yet
				break;

			case modules::rtmp::Command::OnMetaData:
				if ((data_name_type == modules::rtmp::AmfTypeMarker::Object) || (data_name_type == modules::rtmp::AmfTypeMarker::EcmaArray))
				{
					OnAmfMetadata(message->header, data_name_property);
				}
				else
				{
					logaw("Data type is not object or ecma array");
				}
				break;

			default:
				break;
		}

		// Find it in Events
		if (CheckEvent(message->header, document) == false)
		{
			logad("There were no triggered events - Message(%s / %s)", message_name.CStr(), data_name.CStr());
		}

		return true;
	}

	bool RtmpChunkHandler::HandleAudio(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		if (_media_info.ignore_audio_packets)
		{
			return true;
		}

		const auto message_length = message->header->message_length;

		if (message_length == 0)
		{
			// Nothing to do
			logaw("0-byte audio message received");
			return true;
		}

		if (ov::InRange(static_cast<size_t>(message_length), AUDIO_DATA_MIN_SIZE, MAX_PACKET_SIZE) == false)
		{
			logae("Invalid audio payload size: %u", message_length);
			return false;
		}

		modules::flv::AudioParser parser(TRACK_ID_FOR_AUDIO);
		ov::BitReader reader(message->payload);

		if (parser.Parse(reader) == false)
		{
			logae("Could not parse RTMP audio data");
			return false;
		}

		if (reader.GetRemainingBits() > 0)
		{
			OV_ASSERT(false, "There are remaining bits in the audio message: %zu bytes (%zu bits)",
					  reader.GetRemainingBytes(),
					  reader.GetRemainingBits());
		}

		std::map<int, std::shared_ptr<RtmpTrack>> rtmp_track_to_send_map;

		for (auto &parsed_data : parser.GetDataList())
		{
			auto track_id = parsed_data->track_id;

			auto rtmp_track = _stream->GetRtmpTrack(track_id);

			if (rtmp_track == nullptr)
			{
				auto media_codec_id = ToMediaCodecId(parsed_data->sound_format).value_or(cmn::MediaCodecId::None);

				bool is_supported_codec = IsSupportedCodecForERTMP(media_codec_id);

				if (is_supported_codec == false)
				{
					logaw("Not supported audio codec in track: %s(%d) - %u",
						  cmn::GetCodecIdString(media_codec_id),
						  ov::ToUnderlyingType(media_codec_id),
						  track_id);
				}

				// If the track is not found, create a new one
				rtmp_track = RtmpTrack::Create(_stream->GetSharedPtrAs<RtmpStreamV2>(), track_id, parser.IsExHeader(), media_codec_id);
				rtmp_track->SetIgnored(is_supported_codec == false);

				_stream->AddRtmpTrack(rtmp_track);

				if (is_supported_codec)
				{
					auto media_track = rtmp_track->CreateMediaTrack(parser, parsed_data);

					if (media_track == nullptr)
					{
						logae("Failed to create media track for audio data with track id: %d", track_id);
						return false;
					}

					if (rtmp_track->IsFromExHeader() == false)
					{
						media_track->SetSampleRate(_media_info.audio_samplerate.value_or(0));
						media_track->SetBitrateByConfig(_media_info.audio_bitrate.value_or(0) * 1000);

						if (_media_info.audio_channel_layout.has_value())
						{
							media_track->GetChannel().SetLayout(_media_info.audio_channel_layout.value());
						}
					}

					_stream->AddTrack(media_track);
				}

				// Prevent negative track count when unexpected track is received
				_waiting_audio_track_count = std::max(0, _waiting_audio_track_count - 1);
			}

			if (rtmp_track->IsIgnored())
			{
				continue;
			}

			rtmp_track->Handle(message, parser, parsed_data);
			rtmp_track_to_send_map[track_id] = rtmp_track;
		}

		if (_stream->IsPublished() == false)
		{
			if (_stream->IsReadyToPublish() == false)
			{
				return true;
			}

			if (_stream->PublishStream() == false)
			{
				return false;
			}

			// If the publishing is completed, we need to flush the data that has been accumulated so far
			for (auto &rtmp_track_pair : _stream->_rtmp_track_map)
			{
				auto &track = rtmp_track_pair.second;

				if (track->IsIgnored())
				{
					continue;
				}

				rtmp_track_to_send_map[track->GetTrackId()] = track;
			}
		}

		OV_ASSERT2(_stream->IsPublished());

		for (auto &rtmp_track_pair : rtmp_track_to_send_map)
		{
			auto &rtmp_track = rtmp_track_pair.second;

			for (auto &media_packet : rtmp_track->GetMediaPacketList())
			{
				_stream->SendFrame(media_packet);
			}

			rtmp_track->ClearMediaPacketList();
		}

		if (_last_audio_pts_clock.IsStart() == false)
		{
			_last_audio_pts_clock.Start();
		}
		else
		{
			_last_audio_pts_clock.Update();
		}

		_stats.last_audio_timestamp = message->header->completed.timestamp;
		_stats.audio_frame_count++;

		return true;
	}

	bool RtmpChunkHandler::HandleVideo(const std::shared_ptr<const modules::rtmp::Message> &message)
	{
		if (_media_info.ignore_video_packets)
		{
			return true;
		}

		const auto message_length = message->header->message_length;

		if (message_length == 0)
		{
			// Nothing to do
			logaw("0-byte video message received");
			return true;
		}

		if (ov::InRange(static_cast<size_t>(message_length), VIDEO_DATA_MIN_SIZE, MAX_PACKET_SIZE) == false)
		{
			logae("Invalid video payload size: %u", message_length);
			return false;
		}

		modules::flv::VideoParser parser(TRACK_ID_FOR_VIDEO);
		ov::BitReader reader(message->payload);

		if (parser.Parse(reader) == false)
		{
			logae("Could not parse RTMP video data");
			return false;
		}

		if (reader.GetRemainingBits() > 0)
		{
			OV_ASSERT(false, "There are remaining bits in the video message: %zu bytes (%zu bits)",
					  reader.GetRemainingBytes(),
					  reader.GetRemainingBits());
		}

		std::map<int, std::shared_ptr<RtmpTrack>> rtmp_track_to_send_map;

		for (auto &parsed_data : parser.GetDataList())
		{
			auto track_id = parsed_data->track_id;

			auto rtmp_track = _stream->GetRtmpTrack(track_id);

			if (rtmp_track == nullptr)
			{
				auto media_codec_id = (parser.IsExHeader()
										   ? ToMediaCodecId(parsed_data->video_fourcc)
										   : ToMediaCodecId(parsed_data->video_codec_id.value()))
										  .value_or(cmn::MediaCodecId::None);

				bool is_supported_codec = IsSupportedCodecForERTMP(media_codec_id);

				if (is_supported_codec == false)
				{
					logaw("Not supported video codec in track: %s(%d) - %u",
						  cmn::GetCodecIdString(media_codec_id),
						  ov::ToUnderlyingType(media_codec_id),
						  track_id);
				}

				// If the track is not found, create a new one
				rtmp_track = RtmpTrack::Create(_stream->GetSharedPtrAs<RtmpStreamV2>(), track_id, parser.IsExHeader(), media_codec_id);
				rtmp_track->SetIgnored(is_supported_codec == false);

				_stream->AddRtmpTrack(rtmp_track);

				if (is_supported_codec)
				{
					auto media_track = rtmp_track->CreateMediaTrack(parser, parsed_data);

					if (media_track == nullptr)
					{
						logae("Failed to create media track for video data with track id: %d", track_id);
						return false;
					}

					if (rtmp_track->IsFromExHeader() == false)
					{
						media_track->SetWidth(_media_info.video_width);
						media_track->SetHeight(_media_info.video_height);
						media_track->SetFrameRateByConfig(_media_info.video_framerate);
						media_track->SetBitrateByConfig(_media_info.video_bitrate * 1000);
					}

					_stream->AddTrack(media_track);
				}

				// Prevent negative track count when unexpected track is received
				_waiting_video_track_count = std::max(0, _waiting_video_track_count - 1);
			}

			if (rtmp_track->IsIgnored())
			{
				continue;
			}

#if DEBUG
			if (parsed_data->video_metadata.has_value())
			{
				logtd("Metadata: \n%s", parsed_data->video_metadata.value().ToString().CStr());
			}
#endif	// DEBUG

			if (parsed_data->video_packet_type == modules::flv::VideoPacketType::Metadata)
			{
				auto metadata = parsed_data->video_metadata;

				if (metadata.has_value())
				{
					auto name = metadata->GetString(0).value_or("");

					logtw("Video metadata [%s] has been received, but not handled", name.CStr());
				}
				else
				{
					logaw("Video metadata is not present, but video packet type is Metadata");
				}

				continue;
			}

			rtmp_track->Handle(message, parser, parsed_data);
			rtmp_track_to_send_map[track_id] = rtmp_track;
		}

		if (_stream->IsPublished() == false)
		{
			if (_stream->IsReadyToPublish() == false)
			{
				return true;
			}

			if (_stream->PublishStream() == false)
			{
				return false;
			}

			// If the publishing is completed, we need to flush the data that has been accumulated so far
			for (auto &rtmp_track_pair : _stream->_rtmp_track_map)
			{
				auto &track = rtmp_track_pair.second;

				if (track->IsIgnored())
				{
					continue;
				}

				rtmp_track_to_send_map[track->GetTrackId()] = track;
			}
		}

		OV_ASSERT2(_stream->IsPublished());

		bool has_key_frame = false;

		for (auto &rtmp_track_pair : rtmp_track_to_send_map)
		{
			auto &rtmp_track = rtmp_track_pair.second;

			for (auto &media_packet : rtmp_track->GetMediaPacketList())
			{
				_stream->SendFrame(media_packet);

				has_key_frame = has_key_frame || media_packet->IsKeyFrame();
			}

			rtmp_track->ClearMediaPacketList();
		}

		if (_last_video_pts_clock.IsStart() == false)
		{
			_last_video_pts_clock.Start();
		}
		else
		{
			_last_video_pts_clock.Update();
		}

		// Statistics for debugging
		if (has_key_frame)
		{
			_stats.key_frame_interval = message->header->completed.timestamp - _stats.previous_key_frame_timestamp;
			_stats.previous_key_frame_timestamp = message->header->completed.timestamp;
		}

		_stats.last_video_timestamp = message->header->completed.timestamp;
		_stats.video_frame_count++;

		auto elapsed_ms = _stats.GetElapsedInMs();
		if (elapsed_ms >= (10 * 1000))
		{
			logi("RTMPProvider.Stat", "[%s/%s(%u)] RTMP Info - %s",
				 RTMP_STREAM_DESC,
				 _stats.GetStatsString(elapsed_ms).CStr());

			_stats.ResetCheckTime();
		}

		return true;
	}

	bool RtmpChunkHandler::HandleChunkMessage()
	{
		// Sequence of RTMP chunk messages
		//
		// +--------+                                       +-----+
		// | Client |                                       | OME |
		// +--------+                                       +-----+
		//    ------------------ << Handshake >> ------------------
		//      |                      ...                     |
		//    ------------------- << Connect >> -------------------
		//      |                                              |
		//      |--------------------------------------------> |
		//      |   SetChunkSize                               |
		//      |   Amf0Command("Connect")                     |
		//      | <--------------------------------------------|
		//      |                  WindowAcknowledgementSize   |
		//      |                           SetPeerBandwidth   |
		//      |--------------------------------------------> |
		//      |      WindowAcknowledgementSize               |
		//      | <--------------------------------------------|
		//      |                                StreamBegin   |
		//      |                               SetChunkSize   |
		//      |                                    _result   |
		//      |                      ...                     |
		//      |                                              |
		//    ------------------- << Publish >> -------------------
		//      |                                              |
		//      |--------------------------------------------> |
		//      |   releaseStream                              |
		//      |   Amf0Command("FCPublish")                   |
		//      |   Amf0Command("createStream")                |
		//      | <--------------------------------------------|
		//      |                                    _result   |
		//      |--------------------------------------------> |
		//      |   Amf0Command("publish")                     |
		//      | <--------------------------------------------|
		//      |                                StreamBegin   |
		//      |                       onStatus("...Start")   |
		//      |--------------------------------------------> |
		//      |   Amf0Command("@setDataFrame")               |
		//      |                      ...                     |
		while (true)
		{
			auto message = _chunk_parser.GetMessage();

			if ((message == nullptr) || (message->payload == nullptr))
			{
				break;
			}

			bool result = true;
			auto type_id = message->header->completed.type_id;

			switch (type_id)
			{
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::SetChunkSize, result = HandleSetChunkSize(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::Acknowledgement, result = HandleAcknowledgement(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::UserControl, result = HandleUserControl(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::WindowAcknowledgementSize, result = HandleWindowAcknowledgementSize(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::Audio, result = HandleAudio(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::Video, result = HandleVideo(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::Amf0Data, result = HandleAmf0Data(message));
				OV_CASE_BREAK(modules::rtmp::MessageTypeID::Amf0Command, result = HandleAmf0Command(message));

				default:
					logaw("Not handled RTMP message: %d (%s)", type_id, modules::rtmp::EnumToString(type_id));
					break;
			}

			if (result == false)
			{
				return false;
			}
		}

		return true;
	}

	int32_t RtmpChunkHandler::HandleData(const std::shared_ptr<const ov::Data> &data)
	{
		int32_t total_bytes_used = 0;
		std::shared_ptr<const ov::Data> current_data = data;

		while (current_data->IsEmpty() == false)
		{
			size_t bytes_used = 0;
			auto status = _chunk_parser.Parse(current_data, &bytes_used);

			total_bytes_used += bytes_used;

			switch (status)
			{
				case modules::rtmp::ChunkParser::ParseResult::Error:
					logae("An error occurred while parse RTMP data");
					return -1;

				case modules::rtmp::ChunkParser::ParseResult::NeedMoreData:
					break;

				case modules::rtmp::ChunkParser::ParseResult::Parsed:
					if (HandleChunkMessage() == false)
					{
						logad("HandleChunkMessage Fail");
						logap("Failed to import packet\n%s", current_data->Dump(current_data->GetLength()).CStr());

						return -1LL;
					}
					break;
			}

			current_data = current_data->Subdata(bytes_used);

			if (status == modules::rtmp::ChunkParser::ParseResult::NeedMoreData)
			{
				break;
			}
		}

		return total_bytes_used;
	}

	bool RtmpChunkHandler::OnTextData(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, const modules::rtmp::AmfDocument &document)
	{
		int64_t pts = 0;

		if (_last_video_pts > _last_audio_pts)
		{
			pts = _last_video_pts + _last_video_pts_clock.Elapsed();
		}
		else
		{
			pts = _last_audio_pts + _last_audio_pts_clock.Elapsed();
		}

		ov::ByteStream byte_stream;

		if (document.Encode(byte_stream) == false)
		{
			return false;
		}

		return _stream->SendDataFrame(pts, cmn::BitstreamFormat::AMF, cmn::PacketType::EVENT, byte_stream.GetDataPointer(), false);
	}

	bool RtmpChunkHandler::OnCuePoint(const std::shared_ptr<const modules::rtmp::ChunkHeader> &header, const modules::rtmp::AmfDocument &document)
	{
		ov::ByteStream byte_stream;
		if (document.Encode(byte_stream) == false)
		{
			return false;
		}

		int64_t pts = (_last_video_pts > _last_audio_pts)
						  ? (_last_video_pts + _last_video_pts_clock.Elapsed())
						  : (_last_audio_pts + _last_audio_pts_clock.Elapsed());

		return _stream->SendDataFrame(pts, cmn::BitstreamFormat::AMF, cmn::PacketType::EVENT, byte_stream.GetDataPointer(), false);
	}
}  // namespace pvd::rtmp
