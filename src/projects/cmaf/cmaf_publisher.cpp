//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_publisher.h"
#include "config/config_manager.h"
#include "cmaf_application.h"
#include "cmaf_private.h"

std::shared_ptr<CmafPublisher> CmafPublisher::Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
                                                    const info::Application *application_info,
                                                    std::shared_ptr<MediaRouteInterface> router)
{
	auto publisher = std::make_shared<CmafPublisher>(application_info, router);

    if (!publisher->Start(http_server_manager))
    {
        logte("An error occurred while creating CmafPublisher");
        return nullptr;
    }

	return publisher;
}

//====================================================================================================
// CmafPublisher
//====================================================================================================
CmafPublisher::CmafPublisher(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: Publisher(application_info, std::move(router))
{

}

//====================================================================================================
// ~CmafPublisher
//====================================================================================================
CmafPublisher::~CmafPublisher()
{
	logtd("Cmaf publisher has been terminated finally");
}

//====================================================================================================
// Start
// - Publisher override
//====================================================================================================
bool CmafPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
    if(SupportedCodecCheck())
    {
        _supported_codec_check = true;
    }
    else
    {
        // log out put
        logtw("To output Cmaf, at least one of h264 or aac encoding setting information must be set.");
        _supported_codec_check = false;
    }

    auto host = _application_info->GetParentAs<cfg::Host>("Host");

    auto certificate = _application_info->GetCertificate();

    auto chain_certificate = _application_info->GetChainCertificate();

    auto port = host->GetPorts().GetCmafPort();

    auto publisher_info = _application_info->GetPublisher<cfg::CmafPublisher>();

    auto stream_server = std::make_shared<CmafStreamServer>();

    // CORS/Crossdomain.xml setting
    stream_server->SetCrossDomain(publisher_info->GetCrossDomains());

    stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

    // Cmaf Server Start
    bool result = stream_server->Start(ov::SocketAddress(host->GetIp(), port.GetPort()),
                         http_server_manager,
                         _application_info->GetName(),
                         publisher_info->GetThreadCount(),
                         publisher_info->GetSegmentDuration() * 1.5,
                         0,
                         0,
                         certificate,
                         chain_certificate);

    _stream_server = stream_server;

    if(!result)
    {
        return false;
    }

    logtd("Cmaf Publisher Create - prot(%d) count(%d) duration(%d) thread(%d)",
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
bool CmafPublisher::SupportedCodecCheck()
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
bool CmafPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections)
{
    if(_stream_server == nullptr)
        return false;

    return _stream_server->GetMonitoringCollectionData(collections);
}

//====================================================================================================
// OnCreateApplication
//====================================================================================================
std::shared_ptr<Application> CmafPublisher::OnCreateApplication(const info::Application *application_info)
{
	return CmafApplication::Create(application_info);
}

//====================================================================================================
// OnPlayListRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool CmafPublisher::OnPlayListRequest(const ov::String &app_name,
                                       const ov::String &stream_name,
                                       const ov::String &file_name,
                                       ov::String &play_list)
{
    if(!_supported_codec_check)
    {
        logtw("To output Cmaf, at least one of h264 or aac encoding setting information must be set.");
        return false;
    }

	auto stream = std::static_pointer_cast<CmafStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
        logtw("Cmaf Cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

    if(!stream->GetPlayList(play_list))
    {
        logtw("Cmaf get playlist fail (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        return false;
    }

    return true;
}

//====================================================================================================
// OnSegmentRequest
//  - SegmentStreamObserver Implementation
//====================================================================================================
bool CmafPublisher::OnSegmentRequest(const ov::String &app_name,
                                      const ov::String &stream_name,
                                      const ov::String &file_name,
                                      std::shared_ptr<ov::Data> &segment_data)
{
    if(!_supported_codec_check)
    {
        logtw("To output Cmaf, at least one of h264 or aac encoding setting information must be set.");
        return false;
    }

	auto stream = std::static_pointer_cast<CmafStream>(GetStream(app_name, stream_name));

	if(!stream)
	{
        logtw("Cmaf cannot find stream (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	if(!stream->GetSegment(file_name, segment_data))
    {
        logtw("Cmaf get segment fail (%s/%s/%s)", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        return false;
    }

	return true;
}
