//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================


#include "file_application.h"

#include "file_private.h"
#include "file_stream.h"

namespace pvd
{
	std::shared_ptr<FileApplication> FileApplication::Create(const std::shared_ptr<PullProvider> &provider, const info::Application &application_info)
	{
		auto application = std::make_shared<FileApplication>(provider, application_info);
		application->Start();

		return application;
	}

	FileApplication::FileApplication(const std::shared_ptr<PullProvider> &provider, const info::Application &info)
		: PullApplication(provider, info)
	{
	}

	FileApplication::~FileApplication()
	{
	}

	std::shared_ptr<pvd::PullStream> FileApplication::CreateStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list, const std::shared_ptr<pvd::PullStreamProperties> &properties)
	{
		return FileStream::Create(GetSharedPtrAs<pvd::PullApplication>(), stream_id, stream_name, url_list, properties);
	}

	bool FileApplication::Start()
	{
		return pvd::PullApplication::Start();
	}

	bool FileApplication::Stop()
	{
		return pvd::PullApplication::Stop();
	}
}  // namespace pvd