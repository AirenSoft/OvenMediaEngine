//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./flv_video_parser.h"

#include <modules/bitstream/h265/h265_parser.h>
#include <modules/rtmp_v2/amf0/amf_document.h>

#include "./flv_private.h"

#undef OV_LOG_TAG
#define OV_LOG_TAG "Flv.Parser"

namespace modules
{
	namespace flv
	{
		std::shared_ptr<const ov::Data> GetPayload(bool is_multi_track, ov::BitReader &reader, uint24_t size_of_video_track, size_t size_of_video_track_offset)
		{
			static auto EMPTY_DATA = std::make_shared<ov::Data>();

			if (is_multi_track && (size_of_video_track_offset == 0))
			{
				OV_ASSERT2(false);
				return EMPTY_DATA;
			}

			size_t payload_size = 0;

			if (is_multi_track == false)
			{
				payload_size = reader.GetRemainingBytes();
			}
			else
			{
				// How many bytes have been read since the sizeOfVideoTrack field
				auto read_bytes_since_size_of_video_track = reader.GetByteOffset() - size_of_video_track_offset;
				payload_size = size_of_video_track - read_bytes_since_size_of_video_track;
			}

			if (payload_size > 0)
			{
				return reader.ReadBytes(payload_size);
			}

			return EMPTY_DATA;
		}

		std::shared_ptr<VideoData> VideoParser::ProcessExVideoTagHeader(ov::BitReader &reader, VideoFrameType frame_type, bool *process_video_body)
		{
			auto data = std::make_shared<VideoData>(frame_type);

			*process_video_body = true;

			// Interpret UB[4] bits as VideoPacketType instead of sound rate, size, and type.
			// videoPacketType = UB[4] as VideoPacketType	// at byte boundary after this read
			data->video_packet_type = reader.ReadAs<VideoPacketType>(4);
			logtp("videoPacketType (4 bits): %s (%d, 0x%04X)", EnumToString(data->video_packet_type), data->video_packet_type, data->video_packet_type);

			uint32_t mod_ex_data_size = 0;
			std::shared_ptr<ov::Data> mod_ex_data = nullptr;

			// Process each ModEx data packet
			while (data->video_packet_type == VideoPacketType::ModEx)
			{
				// Determine the size of the packet ModEx data (ranging from 1 to 256 bytes)
				// modExDataSize = UI8 + 1
				mod_ex_data_size = reader.ReadU8() + 1;
				logtp("modExDataSize (8 bits): %u (0x%02X + 1)", mod_ex_data_size, (mod_ex_data_size - 1));

				// If maximum 8-bit size is not sufficient, use a 16-bit value
				if (mod_ex_data_size == 256)
				{
					// modExDataSize = UI16 + 1;
					mod_ex_data_size = reader.ReadU16() + 1;
					logtp("modExDataSize (16 bits): %u (0x%04X + 1)", mod_ex_data_size, (mod_ex_data_size - 1));
				}

				// Fetch the packet ModEx data based on its determined size
				mod_ex_data = std::make_shared<ov::Data>();
				mod_ex_data->SetLength(mod_ex_data_size);

				// fetch the VideoPacketOptionType
				// videoPacketModExType = UB[4] as VideoPacketModExType
				auto video_packet_mod_ex_type = reader.ReadAs<VideoPacketModExType>(4);
				logtp("videoPacketModExType (4 bits): %s (%d, 0x%01X)", EnumToString(video_packet_mod_ex_type), video_packet_mod_ex_type, video_packet_mod_ex_type);

				// Update videoPacketType
				// videoPacketType = UB[4] as VideoPacketType	// at byte boundary after this read
				data->video_packet_type = reader.ReadAs<VideoPacketType>(4);

				if (video_packet_mod_ex_type == VideoPacketModExType::TimestampOffsetNano)
				{
					// This block processes TimestampOffsetNano to enhance RTMP timescale
					// accuracy and compatibility with formats like MP4, M2TS, and Safari's
					// Media Source Extensions. It ensures precise synchronization without
					// altering core RTMP timestamps, applying only to the current media
					// message. These adjustments enhance synchronization and timing
					// accuracy in media messages while preserving the core RTMP timestamp
					// integrity.
					//
					// NOTE:
					// - 1 millisecond (ms) = 1,000,000 nanoseconds (ns).
					// - Maximum value representable with 20 bits is 1,048,575 ns
					//   (just over 1 ms), allowing precise sub-millisecond adjustments.
					// - modExData must be at least 3 bytes, storing values up to 999,999 ns.

					// videoTimestampNanoOffset = bytesToUI24(modExData)
					data->video_timestamp_nano_offset = ov::BE24ToHost(reader.ReadU24());
					logtp("videoTimestampNanoOffset (24 bits): %u (0x%06X)", data->video_timestamp_nano_offset, data->video_timestamp_nano_offset);

					// TODO: Integrate this nanosecond offset into timestamp management
					// to accurately adjust the presentation time.
				}
			}

			if (
				(data->video_packet_type != VideoPacketType::Metadata) &&
				data->video_frame_type == VideoFrameType::Command)
			{
				// videoCommand = UI8 as VideoCommand
				data->video_command = reader.ReadU8As<VideoCommand>();
				logtp("videoCommand (8 bits): %s (%d, 0x%02X)", EnumToString(data->video_command), data->video_command, data->video_command);

				// ExVideoTagBody has no payload if we got here.
				// Set boolean to not try to process the video body.
				*process_video_body = false;
			}
			else if (data->video_packet_type == VideoPacketType::Multitrack)
			{
				_is_multitrack = true;
				logtp("isMultiTrack: true");
				// videoMultitrackType = UB[4] as AvMultitrackType
				_multitrack_type = reader.ReadAs<AvMultitrackType>(4);
				logtp("videoMultitrackType (4 bits): %s (%d, 0x%01X)", EnumToString(_multitrack_type), _multitrack_type, _multitrack_type);

				// Fetch VideoPacketType for all video tracks in the video message.
				// This fetch MUST not result in a VideoPacketType.Multitrack
				// videoPacketType = UB[4] as VideoPacketType
				data->video_packet_type = reader.ReadAs<VideoPacketType>(4);
				logtp("videoPacketType (4 bits): %s (%d, 0x%01X)", EnumToString(data->video_packet_type), data->video_packet_type, data->video_packet_type);

				if (_multitrack_type != AvMultitrackType::ManyTracksManyCodecs)
				{
					// The tracks are encoded with the same codec. Fetch the FOURCC for them
					// videoFourCc = FOURCC as VideoFourCc
					data->video_fourcc = reader.ReadU32BEAs<VideoFourCc>();
					logtp("videoFourCc (32 bits): %s (%08X)",
						  FourCcToString(data->video_fourcc.value()),
						  data->video_fourcc.value());
				}
			}
			else
			{
				// videoFourCc = FOURCC as VideoFourCc
				data->video_fourcc = reader.ReadU32BEAs<VideoFourCc>();
				logtp("videoFourCc (32 bits): %s (%08X)",
					  FourCcToString(data->video_fourcc.value()),
					  data->video_fourcc.value());
			}

			return data;
		}

		std::optional<rtmp::AmfDocument> VideoParser::ParseVideoMetadata(ov::BitReader &reader)
		{
			// FFmpeg sends metadata as AMF-encoded data
			//
			// ```
			// {  (AmfDocument)
			//     0: "colorInfo"  (String)
			//     1: {  (Object)
			//         colorConfig: {}  (Object)
			//     }
			// }
			auto data = reader.GetData();
			ov::ByteStream byte_stream(data);
			rtmp::AmfDocument document;

			if (data->IsEmpty() == false)
			{
				if (document.Decode(byte_stream))
				{
					auto read_bytes = (data->GetLength() - byte_stream.Remained());
					reader.SkipBytes(read_bytes);
					return document;
				}
				else
				{
					logte("Failed to decode metadata\n%s", data->Dump(16).CStr());
					OV_ASSERT2(false);
				}
			}
			else
			{
				logtw("Empty metadata received");
			}

			return std::nullopt;
		}

		bool VideoParser::ParseLegacyAvc(ov::BitReader &reader, const std::shared_ptr<VideoData> &data)
		{
			// Below code is based on the legacy FLV specification, `video_file_format_spec_v10`
			//
			// If CodecID == 7
			//   AVCVIDEOPACKET
			//
			// AVCVIDEOPACKET
			// Field             Type        Comment
			// ------------------------------------------------
			// AVCPacketType     UI8         0: AVC sequence header
			//                               1: AVC NALU
			//                               2: AVC end of sequence (lower level NALU
			//                               sequence ender is not required or supported)
			// CompositionTime   SI24        if AVCPacketType == 1
			//                                 Composition time offset
			//                               else
			//                                 0
			// Data              UI8[n]      if AVCPacketType == 0
			//                                 AVCDecoderConfigurationRecord
			//                               else if AVCPacketType == 1
			//                                 One or more NALUs (can be individual
			//                                 slices per FLV packets; that is, full frames
			//                                 are not strictly required)
			//                               else if AVCPacketType == 2
			//                                 Empty
			data->video_packet_type = reader.ReadU8As<VideoPacketType>();
			logtp("[legacy AVC] videoPacketType (8 bits): %s (%d, 0x%02X)", EnumToString(data->video_packet_type), data->video_packet_type, data->video_packet_type);
			data->composition_time_offset = reader.ReadS24BE();
			logtp("[legacy AVC] compositionTimeOffset (24 bits): %d (0x%06X)", data->composition_time_offset, data->composition_time_offset);

			data->payload = GetPayload(false, reader, 0, 0);
			logtp("[legacy AVC] payload: %zu bytes", data->payload->GetLength());

			return true;
		}

		std::shared_ptr<HEVCDecoderConfigurationRecord> VideoParser::ParseHEVC(ov::BitReader &reader)
		{
			if ((reader.GetBitOffset() % 8) != 0)
			{
				OV_ASSERT2(false);
			}

			auto record = HEVCDecoderConfigurationRecord::ParseV2(reader);

			if (record == nullptr)
			{
				logte("FAILED TO PARSE!");
				OV_ASSERT2(false);
			}

			return record;
		}

		std::shared_ptr<VideoData> VideoParser::ProcessExVideoTagBody(ov::BitReader &reader, bool process_video_body, std::shared_ptr<VideoData> data)
		{
			uint24_t size_of_video_track = 0;
			size_t size_of_video_track_offset = 0;

			while (process_video_body)
			{
				if (_is_multitrack)
				{
					if (_multitrack_type == AvMultitrackType::ManyTracksManyCodecs)
					{
						// Each track has a codec assigned to it. Fetch the FOURCC for the next track.
						// videoFourCc = FOURCC as VideoFourCc
						data->video_fourcc = reader.ReadU32BEAs<VideoFourCc>();
						logtp("[MultiTrack/ManyTracksManyCodecs] videoFourCc (32 bits): %s (%08X)",
							  FourCcToString(data->video_fourcc.value()),
							  data->video_fourcc.value());
					}

					// Track Ordering:
					//
					// For identifying the highest priority (a.k.a., default track)
					// or highest quality track, it is RECOMMENDED to use trackId
					// set to zero. For tracks of lesser priority or quality, use
					// multiple instances of trackId with ascending numerical values.
					// The concept of priority or quality can have multiple
					// interpretations, including but not limited to bitrate,
					// resolution, default angle, and language. This recommendation
					// serves as a guideline intended to standardize track numbering
					// across various applications.
					// videoTrackId = UI8
					data->track_id = reader.ReadU8();
					logtp("[MultiTrack] videoTrackId (8 bits): %u", data->track_id.value());

					if (_multitrack_type != AvMultitrackType::OneTrack)
					{
						// The `sizeOfVideoTrack` specifies the size in bytes of the
						// current track that is being processed. This size starts
						// counting immediately after the position where the `sizeOfVideoTrack`
						// value is located. You can use this value as an offset to locate the
						// next video track in a multitrack system. The data pointer is
						// positioned immediately after this field. Depending on the MultiTrack
						// type, the offset points to either a `fourCc` or a `trackId.`
						// sizeOfVideoTrack = UI24
						size_of_video_track = reader.ReadU24BE();
						logtp("[MultiTrack] sizeOfVideoTrack (24 bits): %u", size_of_video_track);

						// Record the offset of the sizeOfVideoTrack field
						// to calculate the size of payload
						size_of_video_track_offset = reader.GetByteOffset();
						logtp("[MultiTrack] sizeOfVideoTrackOffset (8 bits): %zu", size_of_video_track_offset);
					}
				}

				if (data->video_packet_type == VideoPacketType::Metadata)
				{
					// The body does not contain video data; instead, it consists of AMF-encoded
					// metadata. The metadata is represented by a series of [name, value] pairs.
					// Currently, the only defined [name, value] pair is ["colorInfo", Object].
					// See the Metadata Frame section for more details on this object.
					//
					// For a deeper understanding of the encoding, please refer to the descriptions
					// of SCRIPTDATA and SCRIPTDATAVALUE in the FLV file specification.
					// videoMetadata = [VideoMetadata]
#if DEBUG
					auto current_bit_offset = reader.GetBitOffset();
#endif	// DEBUG
					data->video_metadata = ParseVideoMetadata(reader);
#if DEBUG
					auto used_bits = reader.GetBitOffset() - current_bit_offset;
					logtp("[Metadata] videoMetadata (AMF): %zu bytes (%zu bits)", (used_bits + 7) / 8, used_bits);
#endif	// DEBUG
				}

				if (data->video_packet_type == VideoPacketType::SequenceEnd)
				{
					// signals end of sequence
					logtp("[SequenceEnd]");
				}

				if (data->video_packet_type == VideoPacketType::SequenceStart)
				{
#if DEBUG
					auto current_bit_offset = reader.GetBitOffset();
#endif	// DEBUG
					if (data->video_fourcc == VideoFourCc::Vp8)
					{
						// body contains a VP8 configuration record to start the sequence
						// vp8Header = [VPCodecConfigurationRecord]
						logtp("[SequenceStart] VPCodecConfigurationRecord");
					}
					else if (data->video_fourcc == VideoFourCc::Vp9)
					{
						// body contains a VP9 configuration record to start the sequence
						// vp9Header = [VPCodecConfigurationRecord]
						logtp("[SequenceStart] VPCodecConfigurationRecord");
					}
					else if (data->video_fourcc == VideoFourCc::Av1)
					{
						// body contains a configuration record to start the sequence
						// av1Header = [AV1CodecConfigurationRecord]
						logtp("[SequenceStart] AV1CodecConfigurationRecord");
					}
					else if (data->video_fourcc == VideoFourCc::Avc)
					{
						// body contains a configuration record to start the sequence.
						// See ISO/IEC 14496-15:2019, 5.3.4.1 for the description of
						// the AVCDecoderConfigurationRecord.
						// avcHeader = [AVCDecoderConfigurationRecord]
						logtp("[SequenceStart] AVCDecoderConfigurationRecord");
					}
					else if (data->video_fourcc == VideoFourCc::Hevc)
					{
						auto current_bit_offset = reader.GetBitOffset();
						auto current_data = reader.GetData();

						// body contains a configuration record to start the sequence.
						// See ISO/IEC 14496-15:2022, 8.3.3.2 for the description of
						// the HEVCDecoderConfigurationRecord.
						// hevcHeader = [HEVCDecoderConfigurationRecord]
						logtp("[SequenceStart] HEVCDecoderConfigurationRecord");
						data->hevc_header = ParseHEVC(reader);

						if (data->hevc_header != nullptr)
						{
							auto used_bits = (reader.GetBitOffset() - current_bit_offset);
							auto used_bytes = (used_bits + 7) / 8;

							data->hevc_header_data = current_data->Subdata(0, used_bytes);
						}
					}
					else
					{
						if (data->video_fourcc.has_value())
						{
							logtw("Not supported video_fourcc: %s (%08X)",
								  FourCcToString(data->video_fourcc.value()),
								  data->video_fourcc.value());
						}
						else
						{
							logte("video_fourcc is not set");
							OV_ASSERT2(false);
						}

						return nullptr;
					}
#if DEBUG
					auto used_bits = reader.GetBitOffset() - current_bit_offset;
					logtp("[SequenceStart] ConfigurationRecord: %zu bytes (%zu bits)", (used_bits + 7) / 8, used_bits);
#endif	// DEBUG
				}

				if (data->video_packet_type == VideoPacketType::MPEG2TSSequenceStart)
				{
					if (data->video_fourcc == VideoFourCc::Av1)
					{
						// body contains a video descriptor to start the sequence
						// av1Header = [AV1VideoDescriptor]
						logtp("[MPEG2TSSequenceStart] AV1VideoDescriptor");
					}
				}

				if (data->video_packet_type == VideoPacketType::CodedFrames)
				{
#if DEBUG
					auto current_bit_offset = reader.GetBitOffset();
#endif	// DEBUG

					if (data->video_fourcc == VideoFourCc::Vp8)
					{
						// body contains series of coded full frames
						// vp8CodedData = [Vp8CodedData]
						logtp("[CodedFrames] Vp8CodedData");
					}
					else if (data->video_fourcc == VideoFourCc::Vp9)
					{
						// body contains series of coded full frames
						// vp9CodedData = [Vp9CodedData]
						logtp("[CodedFrames] Vp9CodedData");
					}
					else if (data->video_fourcc == VideoFourCc::Av1)
					{
						// body contains one or more OBUs representing a single temporal unit
						// av1CodedData = [Av1CodedData]
						logtp("[CodedFrames] Av1CodedData");
					}
					else if (data->video_fourcc == VideoFourCc::Avc)
					{
						// See ISO/IEC 14496-12:2015, 8.6.1 for the description of the composition
						// time offset. The offset in an FLV file is always in milliseconds.
						// compositionTimeOffset = SI24
						data->composition_time_offset = reader.ReadS24BE();
						logtp("[Coded Frames] AVC compositionTimeOffset (24 bits): %d (0x%06X)", data->composition_time_offset, data->composition_time_offset);

						// Body contains one or more NALUs; full frames are required
						// avcCodedData = [AvcCodedData]
						logtp("[CodedFrames] AvcCodedData");
					}
					else if (data->video_fourcc == VideoFourCc::Hevc)
					{
						// See ISO/IEC 14496-12:2015, 8.6.1 for the description of the composition
						// time offset. The offset in an FLV file is always in milliseconds.
						// compositionTimeOffset = SI24
						data->composition_time_offset = reader.ReadS24BE();
						logtp("[Coded Frames] HEVC compositionTimeOffset (24 bits): %d (0x%06X)", data->composition_time_offset, data->composition_time_offset);

						// Body contains one or more NALUs; full frames are required
						// hevcData = [HevcCodedData]
						logtp("[CodedFrames] HevcCodedData");
					}
					else
					{
						// Not supported video_packet_type
						return nullptr;
					}

					data->payload = GetPayload(
						_is_multitrack,
						reader,
						size_of_video_track,
						size_of_video_track_offset);

#if DEBUG
					auto used_bits = reader.GetBitOffset() - current_bit_offset;
					logtp("[CodedFrames] CodedData: %zu bytes (%zu bits)", (used_bits + 7) / 8, used_bits);
#endif	// DEBUG
				}

				if (data->video_packet_type == VideoPacketType::CodedFramesX)
				{
					// compositionTimeOffset is implied to equal zero. This is
					// an optimization to save putting SI24 value on the wire

					if (data->video_fourcc == VideoFourCc::Avc)
					{
						// Body contains one or more NALUs; full frames are required
						// avcCodedData = [AvcCodedData]
						logtp("[CodedFramesX] AvcCodedData");
						OV_ASSERT2(false);
						// TODO: Need to implement this
						return nullptr;
					}
					else if (data->video_fourcc == VideoFourCc::Hevc)
					{
						// Body contains one or more NALUs; full frames are required
						// hevcData = [HevcCodedData]
						logtp("[CodedFramesX] HevcCodedData");

#if DEBUG
						auto current_bit_offset = reader.GetBitOffset();
#endif	// DEBUG

						auto payload = GetPayload(
										   _is_multitrack,
										   reader,
										   size_of_video_track,
										   size_of_video_track_offset)
										   ->Clone();
						data->payload = payload;
#if DEBUG
						auto used_bits = reader.GetBitOffset() - current_bit_offset;
						logtp("[CodedFramesX] CodedData: %zu bytes (%zu bits)", (used_bits + 7) / 8, used_bits);
#endif	// DEBUG

						bool is_length_nalu_format = true;

						// Check if the payload is in <length><nalu>[<length><nalu>...] format
						{
							auto reader = ov::ByteStream(payload);

							while (reader.IsEmpty() == false)
							{
								if (reader.IsRemained(sizeof(uint32_t)) == false)
								{
									// Failed to read NALU length - probably another format or malformed data
									is_length_nalu_format = false;
									break;
								}

								auto nalu_length = reader.ReadBE32();
								if (reader.IsRemained(nalu_length) == false)
								{
									// Failed to read NALU - probably another format or malformed data
									is_length_nalu_format = false;
									break;
								}

								reader.Skip(nalu_length);
							}
						}

						if (is_length_nalu_format)
						{
							logtp("[CodedFramesX] The payload is in <length><nalu>[<length><nalu>...] format, converting to AnnexB format");

							// // We need to convert it to annexb format
							// // Overwrite the length bytes with AnnexB start code
							// auto remaining = payload->GetLength();
							// auto current_position = payload->GetWritableDataAs<uint8_t>();

							// // TODO: 여기서 START CODE가 오지 않는지 확인도 해야함

							// const auto start_code = ov::HostToBE(static_cast<uint32_t>(0x00000001));

							// while (remaining > 0)
							// {
							// 	auto length_part = reinterpret_cast<uint32_t *>(current_position);
							// 	auto length = ov::BEToHost(*length_part);

							// 	// Overwrite the length part with AnnexB start code
							// 	*length_part = start_code;

							// 	// Move the pointer to the next NALU
							// 	current_position += sizeof(uint32_t) + length;
							// 	remaining -= sizeof(uint32_t) + length;
							// }
						}
					}
					else
					{
						// Not supported video_packet_type
						logte("Not supported video_packet_type: %s", EnumToString(data->video_packet_type));
						return nullptr;
					}
				}

				if (
					_is_multitrack &&
					(_multitrack_type != AvMultitrackType::OneTrack) &&
					// positionDataPtrToNextVideoTrack(sizeOfVideoTrack)
					//
					// The position has been updated to the next video track by the reader
					true)
				{
					_data_list.push_back(data);

					data = std::make_shared<VideoData>(data->video_frame_type);

					continue;
				}

				// done processing video message
				break;
			}

			return data;
		}

		bool VideoParser::Parse(ov::BitReader &reader)
		{
			try
			{
				// https://veovera.org/docs/enhanced/enhanced-rtmp-v2#enhanced-video

				// Check if isExVideoHeader flag is set to 1, signaling enhanced RTMP
				// video mode. In this case, VideoCodecId's 4-bit unsigned binary (UB[4])
				// should not be interpreted as a codec identifier. Instead, these
				// UB[4] bits should be interpreted as VideoPacketType.
				//
				// isExVideoHeader = UB[1]
				_is_ex_header = reader.ReadBit();
				logtp("isExVideoHeader (1 bit): %s (%d)", _is_ex_header ? "true" : "false", _is_ex_header);
				// videoFrameType = UB[3] as VideoFrameType
				auto video_frame_type = reader.ReadAs<VideoFrameType>(3);
				logtp("videoFrameType (3 bits): %s (%d, 0x%08X)", EnumToString(video_frame_type), video_frame_type, video_frame_type);

				if (_is_ex_header)
				{
					logtp("Enhanced video header found");

					bool process_video_body;

					auto video_data = ProcessExVideoTagHeader(reader, video_frame_type, &process_video_body);

					video_data = ProcessExVideoTagBody(reader, process_video_body, video_data);

					if (video_data == nullptr)
					{
						return false;
					}

					_data_list.push_back(video_data);
				}
				else
				{
					logtp("Legacy video header found");

					// Utilize the VideoCodecId values and the bitstream description
					// as defined in the legacy [FLV] specification. Refer to this
					// version for the proper implementation details.
					//
					//   videoCodecId = UB[4] as VideoCodecId
					auto video_data = std::make_shared<VideoData>(video_frame_type);
					video_data->video_codec_id = reader.ReadAs<VideoCodecId>(4);

					if (video_data->video_codec_id != VideoCodecId::Avc)
					{
						logte("Unsupported video codec id: %s", EnumToString(video_data->video_codec_id));
						return false;
					}

					if (ParseLegacyAvc(reader, video_data) == false)
					{
						return false;
					}

					_data_list.push_back(video_data);
				}

				return true;
			}
			catch (const ov::BitReaderError &e)
			{
				switch (static_cast<ov::BitReaderError::Code>(e.GetCode()))
				{
					case ov::BitReaderError::Code::NotEnoughBitsToRead:
						logtw("Not enough bits to read: %s - probably malformed video data", e.GetMessage().CStr());
						break;

					case ov::BitReaderError::Code::NotEnoughSpaceInBuffer:
						// Internal server error
						logte("Not enough space in video buffer: %s", e.GetMessage().CStr());
						OV_ASSERT2(false);
						break;

					case ov::BitReaderError::Code::FailedToReadBits:
						// Internal server error
						logte("Failed to read bits for video: %s", e.GetMessage().CStr());
						OV_ASSERT2(false);
						break;
				}
			}

			return false;
		}
	}  // namespace flv
}  // namespace modules
