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
		static std::shared_ptr<RtspcApplication> Create(const info::Application &application_info);

		explicit RtspcApplication(const info::Application &info);

		~RtspcApplication() override;

		std::shared_ptr<pvd::Stream> CreateStream(const ov::String &stream_name, const std::vector<ov::String> &url_list);

		MediaRouteApplicationConnector::ConnectorType GetConnectorType() override
		{
			return MediaRouteApplicationConnector::ConnectorType::Relay;
		}

	private:
		bool Start() override;

		bool Stop() override;
	};
}