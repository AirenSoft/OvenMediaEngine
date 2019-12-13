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
	class OvtApplication : public pvd::Application
	{
	public:
		static std::shared_ptr<OvtApplication> Create(const info::Application &application_info);

		explicit OvtApplication(const info::Application &info);

		~OvtApplication() override;

		std::shared_ptr<pvd::Stream> CreateStream(const std::shared_ptr<ov::Url> &url);

	private:
		bool Start() override;

		bool Stop() override;
	};
}