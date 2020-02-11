#pragma once

#include <unistd.h>
#include <stdint.h>
#include <memory>
#include <unordered_map>

#include <base/ovlibrary/ovlibrary.h>

#include "base/provider/provider.h"
#include "base/provider/application.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"

#include "rtsp_server.h"
#include "rtsp_observer.h"

namespace pvd
{
    class RtspProvider : public pvd::Provider, public RtspObserver
    {
    public:
        explicit RtspProvider(const cfg::Server &server_config, std::shared_ptr<MediaRouteInterface> router);
        ~RtspProvider() override;

        ProviderType GetProviderType() override;
        bool Start() override;

        static std::shared_ptr<RtspProvider> Create(const cfg::Server &server_config, std::shared_ptr<MediaRouteInterface> router);

        // RtspObserver
        bool OnStreamAnnounced(const std::string_view &address,
            const ov::String &app_name, 
            const ov::String &stream_name,
            const RtspMediaInfo &media_info,
            info::application_id_t &application_id,
            uint32_t &stream_id) override;
        bool OnVideoData(info::application_id_t application_id,
            uint32_t stream_id,
            uint8_t track_id,
            uint32_t timestamp,
            const std::shared_ptr<std::vector<uint8_t>> &data,
            uint8_t flags,
            std::unique_ptr<FragmentationHeader> fragmentation_header) override;
        bool OnAudioData(info::application_id_t application_id,
            uint32_t stream_id,
            uint8_t track_id,
            uint32_t timestamp,
            const std::shared_ptr<std::vector<uint8_t>> &data) override;
        bool OnStreamTeardown(info::application_id_t application_id, uint32_t stream_id) override;

    protected:
        std::shared_ptr<pvd::Application> OnCreateProviderApplication(const info::Application &application_info) override;
        bool OnDeleteProviderApplication(const info::Application &app_info) override;

    private:
        std::shared_ptr<RtspServer> rtsp_server_;
    };
}  // namespace pvd