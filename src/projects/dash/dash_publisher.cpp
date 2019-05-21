//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_publisher.h"
#include "config/config_manager.h"
#include "dash_application.h"
#include "dash_private.h"

std::shared_ptr<DashPublisher> DashPublisher::Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                                                    const info::Application *application_info,
                                                    std::shared_ptr<MediaRouteInterface> router)
{
	auto publisher = std::make_shared<DashPublisher>(application_info, router);

    publisher->Start(http_server_manager);

	return publisher;
}

//====================================================================================================
// DashPublisher
//====================================================================================================
DashPublisher::DashPublisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: Publisher(application_info, std::move(router))
{

}

//====================================================================================================
// ~DashPublisher
//====================================================================================================
DashPublisher::~DashPublisher()
{
	logtd("Dash publisher has been terminated finally");
}

//====================================================================================================
// Start
// - Publisher override
//====================================================================================================
bool DashPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
    if(SupportedCodecCheck())
    {
        _supported_codec_check = true;
    }
    else
    {
        // log out put
        logtw("To output DASH, at least one of h264 or aac encoding setting information must be set.");
        _supported_codec_check = false;
    }

    auto host = _application_info->GetParentAs<cfg::Host>("Host");

    auto certificate = _application_info->GetCertificate();

    auto chain_certificate = _application_info->GetChainCertificate();

    auto port = host->GetPorts().GetDashPort();

    auto publisher_info = _application_info->GetPublisher<cfg::DashPublisher>();

    auto stream_server = std::make_shared<DashStreamServer>();

    // CORS/Crossdomain.xml setting
    stream_server->SetCrossDomain(publisher_info->GetCrossDomains());

    stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

    // DASH Server Start
    stream_server->Start(ov::SocketAddress(host->GetIp(), port.GetPort()),
                         http_server_manager,
                         _application_info->GetName(),
                         publisher_info->GetThreadCount(),
                         publisher_info->GetSegmentDuration() * 1.5,
                         publisher_info->GetSendBufferSize(),
                         publisher_info->GetRecvBufferSize(),
                         certificate,
                         chain_certificate);

    _stream_server = stream_server;

    logtd("Dash Publisher Create - prot(%d) count(%d) duration(%d) thread(%d)",
          port.GetPort(),
          publisher_info->GetSegmentCount(),
          publisher_info->GetSegmentDuration(),
          publisher_info->GetThreadCount());

    return Publisher::Start();
}

//====================================================================================================
// SupportedCodecCheck
// - supproted codec(h264/aac) check
// - one more setting
//====================================================================================================
bool DashPublisher::SupportedCodecCheck()
{
    for(const auto & stream : _application_info->GetStreamList())
    {
        const auto &profiles = stream.GetProfiles();
        for(const auto &profile : profiles)
        {
            cfg::StreamProfileUse use = profile.GetUse();
            ov::String profile_name = profile.GetName();

            for(const auto &encode : _application_info->GetEncodes())
            {
                if(encode.IsActive()  && encode.GetName() == profile_name)
                {
                    // video codec
                    if((use == cfg::StreamProfileUse::Both || use == cfg::StreamProfileUse::VideoOnly) &&
                       (encode.GetVideoProfile() != nullptr && encode.GetVideoProfile()->GetCodec() == "h264"))
                            return true;

                    // audio codec
                    if((use == cfg::StreamProfileUse::Both || use == cfg::StreamProfileUse::AudioOnly) &&
                       (encode.GetAudioProfile() != nullptr && encode.GetAudioProfile()->GetCodec() == "aac"))
                        return true;
                }
            }
        }
    }

    return false;
}

//====================================================================================================
// monitoring data pure virtual function
// - collections vector must be insert processed
//====================================================================================================
bool DashPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections)
{
    if(_stream_server == nullptr)
        return false;

    return _stream_server->GetMonitoringCollectionData(collections);
}

//====================================================================================================
// OnCreateApplication
//====================================================================================================
std::shared_ptr<Application> DashPublisher::OnCreateApplication(const info::Application *application_info)
{
	return DashApplication::Create(application_info);
}

//====================================================================================================
// OnPlayListRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool DashPublisher::OnPlayListRequest(const ov::String &app_name,
                                       const ov::String &stream_name,
                                       const ov::String &file_name,
                                       ov::String &play_list)
{
    if(!_supported_codec_check)
    {
        logtw("To output DASH, at least one of h264 or aac encoding setting information must be set.");
        return false;
    }

	auto stream = std::static_pointer_cast<DashStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
        logtw("Dash Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

    if(!stream->GetPlayList(play_list))
    {
        logtw("Dash get playlist fail (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        return false;
    }

    return true;
}

//====================================================================================================
// OnSegmentRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool DashPublisher::OnSegmentRequest(const ov::String &app_name,
                                      const ov::String &stream_name,
                                      const ov::String &file_name,
                                      std::shared_ptr<ov::Data> &segment_data)
{
    if(!_supported_codec_check)
    {
        logtw("To output DASH, at least one of h264 or aac encoding setting information must be set.");
        return false;
    }

	auto stream = std::static_pointer_cast<DashStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
        logtw("Dash cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	if(!stream->GetSegment(file_name, segment_data))
    {
        logtw("Dash get segment fail (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        return false;
    }

	return true;
}
