#pragma once

#include "base/common_types.h"

#include "base/provider/application.h"
#include "base/provider/stream.h"

using namespace pvd;

class RtspApplication : public Application
{
protected:
	const char* GetApplicationTypeName() const override
	{
		return "RTSP Provider";
	}

public:
	static std::shared_ptr<RtspApplication> Create(const info::Application &application_info);

	explicit RtspApplication(const info::Application &info);
	
	~RtspApplication() override = default;

	std::shared_ptr<pvd::Stream> CreateStream();

private:
};