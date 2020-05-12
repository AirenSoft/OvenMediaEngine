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
	public:
		static std::shared_ptr<RtspcApplication> Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info);

		explicit RtspcApplication(const std::shared_ptr<Provider> &provider, const info::Application &info);

		~RtspcApplication() override;

		std::shared_ptr<pvd::Stream> CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list) override;
		std::shared_ptr<pvd::Stream> CreatePushStream(const uint32_t stream_id, const ov::String &stream_name) override {return nullptr;};

		MediaRouteApplicationConnector::ConnectorType GetConnectorType() override
		{
			return MediaRouteApplicationConnector::ConnectorType::Provider;
		}

	private:
		bool Start() override;

		bool Stop() override;
	};
}