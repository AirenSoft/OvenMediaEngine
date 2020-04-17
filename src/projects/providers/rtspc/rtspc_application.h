//
// Created by getroot on 19. 12. 9.
//

#pragma once

#include <base/ovlibrary/url.h>
#include <base/common_types.h>
#include <base/provider/application.h>
#include <base/provider/stream.h>

namespace pvd
{
	class RtspcApplication : public pvd::Application
	{
	protected:
		const char* GetApplicationTypeName() const override
		{
			return "RTSP Pull Provider";
		}

	public:
		static std::shared_ptr<RtspcApplication> Create(const info::Application &application_info);

		explicit RtspcApplication(const info::Application &info);

		~RtspcApplication() override;

		std::shared_ptr<pvd::Stream> CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list) override;
		std::shared_ptr<pvd::Stream> CreatePushStream(const uint32_t stream_id, const ov::String &stream_name) override {return nullptr;};

		MediaRouteApplicationConnector::ConnectorType GetConnectorType() override
		{
			return MediaRouteApplicationConnector::ConnectorType::Relay;
		}

	private:
		bool Start() override;

		bool Stop() override;
	};
}