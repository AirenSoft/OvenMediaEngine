#define OV_LOG_TAG "RtspProvider"

#include "rtsp_provider.h"
#include "rtsp_application.h"

using namespace common;

RtspProvider::RtspProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router) : Provider(application_info, router)
{
}

RtspProvider::~RtspProvider()
{
    Stop();
}

cfg::ProviderType RtspProvider::GetProviderType()
{
    return cfg::ProviderType::Rtsp;
}

std::shared_ptr<Application> RtspProvider::OnCreateApplication(const info::Application *application_info)
{
    return RtspApplication::Create(application_info);
}

bool RtspProvider::Start()
{
    provider_info_ = _application_info->GetProvider<cfg::RtspProvider>();

    if(provider_info_ == nullptr)
    {
        logte("Cannot initialize RtspProvider using config information");
        return false;
    }

    auto host = _application_info->GetParentAs<cfg::Host>("Host");

    if(host == nullptr)
    {
        OV_ASSERT2(false);
        return false;
    }

    int port = host->GetPorts().GetRtspProviderPort().GetPort();

    auto rtsp_address = ov::SocketAddress(host->GetIp(), static_cast<uint16_t>(port));

    logti("RTSP Provider is listening on %s...", rtsp_address.ToString().CStr());

    rtsp_server_ = std::make_shared<RtspServer>();


    rtsp_server_->AddObserver(RtspObserver::GetSharedPtr());
    rtsp_server_->Start(rtsp_address);

    return Provider::Start();
}

std::shared_ptr<RtspProvider> RtspProvider::Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
{
    auto provider = std::make_shared<RtspProvider>(application_info, router);
    provider->Start();
    return provider;
}

bool RtspProvider::OnStreamAnnounced(const ov::String &app_name, 
    const ov::String &stream_name,
    const RtspMediaInfo &media_info,
    info::application_id_t &application_id,
    uint32_t &stream_id)
{
    auto application = std::dynamic_pointer_cast<RtspApplication>(GetApplicationByName(app_name.CStr()));
    if(application == nullptr)
    {
        logte("Cannot find applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
        return false;
    }

    auto existing_stream = GetStreamByName(app_name, stream_name);
    if(existing_stream)
    {
        logti("Duplicate stream input(change) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());

        if(!rtsp_server_->Disconnect(app_name, existing_stream->GetId()))
        {
            logte("Disconnect fail - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
        }

        application->DeleteStream2(application->GetStreamByName(stream_name));
    }

    auto stream = application->MakeStream();
    if(stream == nullptr)
    {
        logte("Can not create stream - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
        return false;
    }

    stream->SetName(stream_name.CStr());

    for (const auto &track : media_info.tracks_)
    {
        auto payload_iterator = media_info.payloads_.find(track.second);
        if (payload_iterator != media_info.payloads_.end())
        {
            auto media_track = std::make_shared<MediaTrack>(payload_iterator->second);
            media_track->SetId(track.second);
            stream->AddTrack(media_track);
        }
    }
    application->CreateStream2(stream);

    application_id = application->GetId();
    stream_id = stream->GetId();

    logtd("Strem ready complete - app(%s/%u) stream(%s/%u)", app_name.CStr(), application_id, stream_name.CStr(), stream_id);

    return true;
}

bool RtspProvider::OnVideoData(info::application_id_t application_id,
    uint32_t stream_id,
    uint8_t track_id,
    uint32_t timestamp,
    const std::shared_ptr<std::vector<uint8_t>> &data,
    uint8_t flags,
    std::unique_ptr<FragmentationHeader> fragmentation_header)
{
    auto application = std::dynamic_pointer_cast<RtspApplication>(GetApplicationById(application_id));
    if(application == nullptr)
    {
        logte("Cannot find applicaton - app(%d) stream(%d)", application_id, stream_id);
        return false;
    }

    auto stream = application->GetStreamById(stream_id);
    if(stream == nullptr)
    {
        logte("Cannot find stream - app(%d) stream(%d)", application_id, stream_id);
        return false;
    }

    auto media_packet = std::make_unique<MediaPacket>(MediaType::Video,
        track_id,
        data->data(),
        data->size(),
        timestamp,
        timestamp,
        -1LL,
        flags & static_cast<uint8_t>(RtpVideoFlags::Keyframe) ?  MediaPacketFlag::Key : MediaPacketFlag::NoFlag);
    media_packet->SetFragHeader(std::move(fragmentation_header));
    application->SendFrame(stream, std::move(media_packet));
    return true;
}

bool RtspProvider::OnAudioData(info::application_id_t application_id,
    uint32_t stream_id,
    uint8_t track_id,
    uint32_t timestamp,
    const std::shared_ptr<std::vector<uint8_t>> &data)
{
    auto application = std::dynamic_pointer_cast<RtspApplication>(GetApplicationById(application_id));
    if(application == nullptr)
    {
        logte("Cannot find applicaton - app(%d) stream(%d)", application_id, stream_id);
        return false;
    }

    auto stream = application->GetStreamById(stream_id);
    if(stream == nullptr)
    {
        logte("Cannot find stream - app(%d) stream(%d)", application_id, stream_id);
        return false;
    }

    auto media_packet = std::make_unique<MediaPacket>(MediaType::Audio,
        track_id,
        data->data(),
        data->size(),
        timestamp,
        timestamp,
        -1LL,
        MediaPacketFlag::NoFlag);
    application->SendFrame(stream, std::move(media_packet));
    return true;
}

bool RtspProvider::OnStreamTeardown(info::application_id_t application_id, uint32_t stream_id)
{
    auto application = std::dynamic_pointer_cast<RtspApplication>(GetApplicationById(application_id));
    if(application == nullptr)
    {
        logte("Cannot find applicaton - app(%d) stream(%d)", application_id, stream_id);
        return false;
    }

    auto stream = application->GetStreamById(stream_id);
    if(stream == nullptr)
    {
        logte("Cannot find stream - app(%d) stream(%d)", application_id, stream_id);
        return false;
    }

    application->DeleteStream2(stream);
    return true;
}
