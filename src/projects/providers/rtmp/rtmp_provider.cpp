//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <config/config.h>
#include <unistd.h>
#include <iostream>

#include "rtmp_application.h"
#include "rtmp_provider.h"
#include "rtmp_stream.h"
#include <base/info/media_extradata.h>

#define OV_LOG_TAG "RtmpProvider"

using namespace common;

std::shared_ptr<RtmpProvider> RtmpProvider::Create(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto provider = std::make_shared<RtmpProvider>(server_config, router);
	if (!provider->Start())
	{
		logte("An error occurred while creating RtmpProvider");
		return nullptr;
	}
	return provider;
}

RtmpProvider::RtmpProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Provider(server_config, router)
{
	logtd("Created Rtmp Provider module.");
}

RtmpProvider::~RtmpProvider()
{
	logti("Terminated Rtmp Provider module.");
}

bool RtmpProvider::Start()
{
	// Get Server & Host configuration
	auto server = GetServerConfig();

	auto rtmp_address = ov::SocketAddress(server.GetIp(), static_cast<uint16_t>(server.GetBind().GetProviders().GetRtmpPort()));

	// Create RtmpServer
	_rtmp_server = std::make_shared<RtmpServer>();

	// Connect RtmpServer to Observer
	_rtmp_server->AddObserver(RtmpObserver::GetSharedPtr());

	if (!_rtmp_server->Start(rtmp_address))
	{
		return false;
	}

	logti("RTMP Server has started listening on %s...", rtmp_address.ToString().CStr());

	return Provider::Start();
}

bool RtmpProvider::Stop()
{
	_rtmp_server->RemoveObserver(RtmpObserver::GetSharedPtr());
	_rtmp_server->Stop();
	return Provider::Stop();
}

std::shared_ptr<pvd::Application> RtmpProvider::OnCreateProviderApplication(const info::Application &application_info)
{
	return RtmpApplication::Create(pvd::Provider::GetSharedPtrAs<pvd::Provider>(), application_info);
}

bool RtmpProvider::OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application)
{
	// Notify the RtmpServer that the application will be deleted
	return _rtmp_server->Disconnect(application->GetName());
}

bool RtmpProvider::OnStreamReadyComplete(const ov::String &app_name,
										 const ov::String &stream_name,
										 std::shared_ptr<RtmpMediaInfo> &media_info,
										 info::application_id_t &application_id,
										 uint32_t &stream_id)
{
	// Find applicaiton, if there is no application, it will disconnect RTMP connection.
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationByName(app_name.CStr()));
	if (application == nullptr)
	{
		logte("Cannot Find Applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	auto provider_info = application->GetProvider<cfg::RtmpProvider>();
	if (provider_info == nullptr)
	{
		logte("Cannot Find ProviderInfo from Applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	// Handle duplicated stream name
	auto exist_stream = GetStreamByName(app_name, stream_name);
	if (exist_stream != nullptr)
	{
		if (provider_info->IsBlockDuplicateStreamName())
		{
			logti("Duplicate Stream Input(reject) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			return false;
		}
		else
		{
			uint32_t old_stream_id = exist_stream->GetId();

			logti("Duplicate Stream Input(change) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());

			// session close
			if (!_rtmp_server->Disconnect(app_name, old_stream_id))
			{
				logte("Disconnect Fail - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			}

			// delete new stream
			application->DeleteStream(exist_stream);
		}
	}

	std::vector<std::shared_ptr<MediaTrack>> tracks;

	if (media_info->video_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		// Video is fixed on Track 0
		new_track->SetId(0);
		new_track->SetMediaType(MediaType::Video);
		new_track->SetCodecId(MediaCodecId::H264);
		new_track->SetWidth((uint32_t)media_info->video_width);
		new_track->SetHeight((uint32_t)media_info->video_height);
		new_track->SetFrameRate(media_info->video_framerate);
		// Kbps -> bps
		new_track->SetBitrate(media_info->video_bitrate * 1000);

		if (media_info->avc_sps && media_info->avc_pps)
		{
			new_track->SetCodecExtradata(std::move(H264Extradata().AddSps(*media_info->avc_sps).AddPps(*media_info->avc_pps).Serialize()));
		}
	
		// I know RTMP uses 1/1000 timebase, however, this timebase was used due to low precision.
		// new_track->SetTimeBase(1, 1000);
		new_track->SetTimeBase(1, 90000);

		// A value to change to 1/90000 from 1/1000
		double video_scale = 90000.0 / 1000.0;
		new_track->SetVideoTimestampScale(video_scale);
		
		tracks.push_back(new_track);
	}

	if (media_info->audio_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		// Video is fixed on Track 1
		new_track->SetId(1);
		new_track->SetMediaType(MediaType::Audio);
		new_track->SetCodecId(MediaCodecId::Aac);
		new_track->SetSampleRate(media_info->audio_samplerate);
		new_track->GetSample().SetFormat(common::AudioSample::Format::S16);
		// Kbps -> bps
		new_track->SetBitrate(media_info->audio_bitrate * 1000);
		// new_track->SetSampleSize(conn->_audio_samplesize);

		if (media_info->audio_channels == 1)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutMono);
		}
		else if (media_info->audio_channels == 2)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutStereo);
		}

		// I know RTMP uses 1/1000 timebase, however, this timebase was used due to low precision.
		// new_track->SetTimeBase(1, 1000);
		new_track->SetTimeBase(1, media_info->audio_samplerate);

		// A value to change to 1/sample_rate from 1/1000
		double  audio_scale = (double)(media_info->audio_samplerate) / 1000.0;
		new_track->SetAudioTimestampScale(audio_scale);

		tracks.push_back(new_track);
	}

	auto stream = application->CreateStream(stream_name, tracks);
	if (stream == nullptr)
	{
		logte("can not create stream - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	// Notify stream id to the Rtmp Server
	stream_id = stream->GetId();

	logtd("Strem ready complete - app(%s/%u) stream(%s/%u)", app_name.CStr(), application->GetId(), stream_name.CStr(), stream->GetId());

	return true;
}

bool RtmpProvider::OnVideoData(info::application_id_t application_id,
							   uint32_t stream_id,
							   int64_t timestamp,
							   RtmpFrameType frame_type,
							   const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(application_id));
	if (application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
	if(stream_metrics != nullptr)
	{
		stream_metrics->IncreaseBytesIn(data->GetLength());
	}

	// TODO: This is a memory copy overhead code.
	// Change is required in a way that does not copy memory.
	const std::shared_ptr<ov::Data> &new_data = data->Clone();

	// In the case of the h264 codec, the pts(presentation timestamp) value is calculated using the cts(composition timestamp) value.
	// The timestamp used by RTMP is DTS.
	// formula:
	// 		cts = pts - dts
	// 		pts = dts + cts

	// TODO: It is currently fixed to h264.
	// Depending on the codec, the bitstream conversion must be processed.


	int64_t cts = 0;
	stream->ConvertToVideoData( new_data, cts);
	if(new_data->GetLength() <= 0)
	{
		return true;
	}

	int64_t dts = timestamp;
	int64_t pts = dts + cts;

	// logte("timestamp = %lld, video.pts=%10lld, scale=%f, size : %d", timestamp, pts, stream->GetVideoTimestampScale(), data->GetLength());
	// logte("Video timestamp = %lld, pts = %lld, dts = %lld, size : %d, duration : %lld", timestamp, pts, dts, data->GetLength(), duration);
	// Change the timebase specification: 1/1000 -> 1/90000

	// The video track is in track 0 always
	auto video_track = stream->GetTrack(0);
	if(video_track == nullptr)
	{
		logte("There is no video track information.");
		return false;
	}

	dts *= video_track->GetVideoTimestampScale();
	pts *= video_track->GetVideoTimestampScale();

	auto pbuf = std::make_shared<MediaPacket>(MediaType::Video,
											  0,
											  new_data,
											  // The timestamp used by RTMP is DTS. PTS will be recalculated later
											  pts, // PTS
											  dts, // DTS
											  // RTMP doesn't know frame's duration
											  -1LL,
											  // duration,
											  frame_type == RtmpFrameType::VideoIFrame ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

	application->SendFrame(stream, std::move(pbuf));

	return true;
}

bool RtmpProvider::OnAudioData(info::application_id_t application_id,
							   uint32_t stream_id,
							   int64_t timestamp,
							   RtmpFrameType frame_type,
							   const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(application_id));

	if (application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
	if(stream_metrics != nullptr)
	{
		stream_metrics->IncreaseBytesIn(data->GetLength());
	}

	// TODO: This is a memory copy overhead code.
	// Change is required in a way that does not copy memory.
	const std::shared_ptr<ov::Data> &new_data = data->Clone();

	// TODO: It is currently fixed to AAC.
	// Depending on the codec, the bitstream conversion must be processed.
	if(stream->ConvertToAudioData(new_data) == 0)
	{
		return true;
	}

	// Change the timebase specification: 1/1000 -> 1/Samplerate
	int64_t pts = timestamp;
	int64_t dts = timestamp;
	// logte("timestamp = %lld, audio.pts=%10lld, scale=%f, size : %d", timestamp, dts, stream->GetAudioTimestampScale(), data->GetLength());
	// logte("Audio timestamp = %lld, pts = %lld, dts = %lld, size : %d, duration : %lld", timestamp, pts, dts, data->GetLength(), duration);

	// The audio track is in track 1 always
	auto audio_track = stream->GetTrack(1);
	if(audio_track == nullptr)
	{
		logte("There is no audio track information.");
		return false;
	}

	pts *= audio_track->GetAudioTimestampScale();
	dts *= audio_track->GetAudioTimestampScale();

	auto pbuf = std::make_shared<MediaPacket>(MediaType::Audio,
											  1,
											  new_data,
											  // The timestamp used by RTMP is DTS. PTS will be recalculated later
											  pts,  // PTS
											  dts,  // DTS
											  // RTMP doesn't know frame's duration
											 -1LL,
											  MediaPacketFlag::Key);
	
	application->SendFrame(stream, std::move(pbuf));

	return true;
}

// Called from RTMP Server
bool RtmpProvider::OnDeleteStream(info::application_id_t app_id, uint32_t stream_id)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(app_id));
	if (application == nullptr)
	{
		logtd("cannot find application %u/%u", app_id, stream_id);
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logtd("cannot find stream %u/%u", app_id, stream_id);
		return false;
	}

	// Notify MediaRouter that the stream has been deleted.
	return application->DeleteStream(stream);
}