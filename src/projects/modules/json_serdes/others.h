//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/http/server/http_server.h>
namespace serdes
{
	Json::Value JsonFromError(const std::shared_ptr<http::HttpError> &error);
}
