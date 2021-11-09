//==============================================================================
//
//  MpegTs Stream
//
//  Created by Hyunjun Jang
//  Moved by Getroot
//
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//==============================================================================

#include "mpegts_stream.h"

#include <base/info/media_extradata.h>
#include <base/mediarouter/media_type.h>
#include <modules/mpegts/mpegts_packet.h>
#include <orchestrator/orchestrator.h>

#include "base/info/application.h"
#include "base/provider/push_provider/application.h"
#include "modules/bitstream/aac/aac_adts.h"
#include "modules/bitstream/h265/h265_parser.h"
#include "modules/bitstream/nalu/nal_unit_splitter.h"
#include "modules/mpegts/mpegts_packet.h"
#include "mpegts_provider_private.h"

namespace pvd
{
	std::shared_ptr<MpegTsStream> MpegTsStream::Create(StreamSourceType source_type, uint32_t client_id, const info::VHostAppName &vhost_app_name, const ov::String &stream_name, const std::shared_ptr<ov::Socket> &client_socket, const ov::SocketAddress &remote_address, uint64_t lifetime_epoch_msec, const std::shared_ptr<PushProvider> &provider)
	{
		auto stream = std::make_shared<MpegTsStream>(source_type, client_id, vhost_app_name, stream_name, client_socket, remote_address, lifetime_epoch_msec, provider);
		if (stream != nullptr)
		{
			stream->Start();
		}
		return stream;
	}

	MpegTsStream::MpegTsStream(StreamSourceType source_type, uint32_t client_id, const info::VHostAppName &vhost_app_name, const ov::String &stream_name, std::shared_ptr<ov::Socket> client_socket, const ov::SocketAddress &remote_address, uint64_t lifetime_epoch_msec, const std::shared_ptr<PushProvider> &provider)
		: PushStream(source_type, client_id, provider),

		  _vhost_app_name(vhost_app_name)
	{
		SetName(stream_name);
		_remote = client_socket;
		SetMediaSource(ov::String::FormatString("%s://%s", ov::StringFromSocketType(client_socket->GetType()), remote_address.ToString(false).CStr()));
		_lifetime_epoch_msec = lifetime_epoch_msec;
	}

	MpegTsStream::~MpegTsStream()
	{
	}

	bool MpegTsStream::Start()
	{
		SetState(Stream::State::PLAYING);
		return PushStream::Start();
	}

	bool MpegTsStream::Stop()
	{
		if (GetState() == Stream::State::STOPPED)
		{
			return true;
		}

		if(_remote->GetState() == ov::SocketState::Connected)
		{
			_remote->Close();
		}

		return PushStream::Stop();
	}

	const std::shared_ptr<ov::Socket> &MpegTsStream::GetClientSock()
	{
		return _remote;
	}

	bool MpegTsStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
	{
		if(GetState() == Stream::State::ERROR || GetState() == Stream::State::STOPPED)
		{
			return false;
		}

		if (_lifetime_epoch_msec != 0 &&
			_remote->GetType() == ov::SocketType::Srt &&
			_lifetime_epoch_msec < ov::Clock::NowMSec())
		{
			// Expired
			logti("Stream has expired by signed policy (%s/%s)", _vhost_app_name.CStr(), GetName().CStr());
			Stop();
			return false;
		}

		std::lock_guard<std::shared_mutex> lock(_depacketizer_lock);
		_depacketizer.AddPacket(data);

		// Publish
		if (IsPublished() == false && _depacketizer.IsTrackInfoAvailable())
		{
			if (Publish() == false)
			{
				return false;
			}
		}

		if (IsPublished() == true)
		{
			while (_depacketizer.IsESAvailable())
			{
				auto es = _depacketizer.PopES();
				auto track = GetTrack(es->PID());

				if (track == nullptr)
				{
					logte("%s/%s(%d) received stream data, but track information could not be found.", GetApplicationName(), GetName().CStr(), GetId());
					return false;
				}

				if (es->IsVideoStream())
				{
					auto bitstream = cmn::BitstreamFormat::Unknown;
					auto packet_type = cmn::PacketType::NALU;

					switch (track->GetCodecId())
					{
						case cmn::MediaCodecId::H264:
							bitstream = cmn::BitstreamFormat::H264_ANNEXB;
							break;
						case cmn::MediaCodecId::H265: {
							bitstream = cmn::BitstreamFormat::H265_ANNEXB;

							// Check if bitstream is keyframe
							bool keyframe_flag = H265Parser::CheckKeyframe(es->Payload(), es->PayloadLength());
							if (keyframe_flag == true)
							{
								logtd("A Keyframe has been arrived");
							}

							// H265 Bitstream Parser Test
							auto nal_unit_list = NalUnitSplitter::Parse(es->Payload(), es->PayloadLength());
							if (nal_unit_list == nullptr)
							{
								logte("Could not parse bitstream into nal units");
							}
							else
							{
								for (uint32_t i = 0; i < nal_unit_list->GetCount(); i++)
								{
									auto nalu = nal_unit_list->GetNalUnit(i);

									H265NalUnitHeader header;
									if (H265Parser::ParseNalUnitHeader(nalu->GetDataAs<uint8_t>(), nalu->GetLength(), header) == false)
									{
										logte("Could not parse nal unit header");
									}
									else
									{
										logtd("H265 Nal Unit Header Parsed : id:%d len:%d", static_cast<int>(header.GetNalUnitType()), nalu->GetLength());
									}

									if (header.GetNalUnitType() == H265NALUnitType::SPS)
									{
										H265SPS sps;
										if (H265Parser::ParseSPS(nalu->GetDataAs<uint8_t>(), nalu->GetLength(), sps) == false)
										{
											logte("Could not parse sps");
										}
										else
										{
											logtd("SPS Parsed : %s", sps.GetInfoString().CStr());
										}
									}
								}
							}

							break;
						}
						default:
							bitstream = cmn::BitstreamFormat::Unknown;
							break;
					}

					auto data = std::make_shared<ov::Data>(es->Payload(), es->PayloadLength());
					auto media_packet = std::make_shared<MediaPacket>(GetMsid(),
																	  cmn::MediaType::Video,
																	  es->PID(),
																	  data,
																	  es->Pts(),
																	  es->Dts(),
																	  bitstream,
																	  packet_type);
					SendFrame(media_packet);
				}
				else if (es->IsAudioStream())
				{
					auto payload = es->Payload();
					auto payload_length = es->PayloadLength();

					auto data = std::make_shared<ov::Data>(payload, payload_length);
					auto media_packet = std::make_shared<MediaPacket>(GetMsid(),
																	  cmn::MediaType::Audio,
																	  es->PID(),
																	  data,
																	  es->Pts(),
																	  es->Dts(),
																	  cmn::BitstreamFormat::AAC_ADTS,
																	  cmn::PacketType::RAW);
					SendFrame(media_packet);
				}
			}
		}

		return true;
	}

	bool MpegTsStream::Publish()
	{
		std::map<uint16_t, std::shared_ptr<MediaTrack>> track_list;

		if (_depacketizer.GetTrackList(&track_list) == false)
		{
			logte("Cannot get track list from mpeg-ts depacketizer.");
			return false;
		}

		for (const auto &x : track_list)
		{
			auto track = x.second;
			AddTrack(track);
		}

		// Publish
		if (PublishChannel(_vhost_app_name) == false)
		{
			return false;
		}

		return true;
	}
}  // namespace pvd