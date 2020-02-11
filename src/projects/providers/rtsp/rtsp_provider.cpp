#define OV_LOG_TAG "RtspProvider"

#include "rtsp_provider.h"
#include "rtsp_application.h"

#include <orchestrator/orchestrator.h>

using namespace common;

namespace pvd
{
    RtspProvider::RtspProvider(const cfg::Server &server_config, std::shared_ptr<MediaRouteInterface> router) : Provider(server_config, router)
    {
    }

    RtspProvider::~RtspProvider()
    {
        Stop();
    }

    ProviderType RtspProvider::GetProviderType()
    {
        return ProviderType::Rtsp;
    }

    std::shared_ptr<pvd::Application> RtspProvider::OnCreateProviderApplication(const info::Application &application_info)
    {
        return RtspApplication::Create(application_info);
    }

    bool RtspProvider::OnDeleteProviderApplication(const info::Application &app_info)
    {
        return true;
    }

    bool RtspProvider::Start()
    {
        // Get Server & Host configuration
        auto server = GetServerConfig();

        auto rtsp_address = ov::SocketAddress(server.GetIp(), static_cast<uint16_t>(server.GetBind().GetProviders().GetRtspPort()));

        logti("RTSP Provider is listening on %s...", rtsp_address.ToString().CStr());

        rtsp_server_ = std::make_shared<RtspServer>();

        rtsp_server_->AddObserver(RtspObserver::GetSharedPtr());
        if(rtsp_server_->Start(rtsp_address) == false)
        {
            return false;
        }

        return Provider::Start();
    }

    std::shared_ptr<RtspProvider> RtspProvider::Create(const cfg::Server &server_config, std::shared_ptr<MediaRouteInterface> router)
    {
        auto provider = std::make_shared<RtspProvider>(server_config, router);
        provider->Start();
        return provider;
    }

    bool RtspProvider::OnStreamAnnounced(const std::string_view &address,
        const ov::String &app_name, 
        const ov::String &stream_name,
        const RtspMediaInfo &media_info,
        info::application_id_t &application_id,
        uint32_t &stream_id)
    {
        // Make an internal app name by domain_name
        ov::String domain_name(address.data(), address.size());
        auto internal_app_name = Orchestrator::GetInstance()->ResolveApplicationNameFromDomain(domain_name, app_name);

        auto application = std::dynamic_pointer_cast<RtspApplication>(GetApplicationByName(internal_app_name.CStr()));
        if(application == nullptr)
        {
            logte("Cannot find applicaton - app(%s) stream(%s)", internal_app_name.CStr(), stream_name.CStr());
            return false;
        }

        auto existing_stream = application->GetStreamByName(stream_name);
        if(existing_stream)
        {
            logti("Duplicate stream input(change) - app(%s) stream(%s)", internal_app_name.CStr(), stream_name.CStr());

            if(!rtsp_server_->Disconnect(internal_app_name, existing_stream->GetId()))
            {
                logte("Disconnect fail - app(%s) stream(%s)", internal_app_name.CStr(), stream_name.CStr());
            }

            application->NotifyStreamDeleted(application->GetStreamByName(stream_name));
        }

        auto stream = application->CreateStream();
        if(stream == nullptr)
        {
            logte("Can not create stream - app(%s) stream(%s)", internal_app_name.CStr(), stream_name.CStr());
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
        application->NotifyStreamCreated(stream);

        application_id = application->GetId();
        stream_id = stream->GetId();

        logtd("Strem ready complete - app(%s/%u) stream(%s/%u)", internal_app_name.CStr(), application_id, stream_name.CStr(), stream_id);

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
        media_packet->SetFragHeader(fragmentation_header.get());
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
            MediaPacketFlag::Key);
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

        application->NotifyStreamDeleted(stream);
        return true;
    }
}