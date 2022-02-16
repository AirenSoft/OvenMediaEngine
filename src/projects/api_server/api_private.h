//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG "APIServer"

#define ThrowIfVirtualIsReadOnly()                                                                   \
	do                                                                                               \
	{                                                                                                \
		if (vhost->IsReadOnly())                                                                     \
		{                                                                                            \
			throw http::HttpError(http::StatusCode::Forbidden, "The VirtualHost is read-only: [%s]", \
								  vhost->GetName().CStr());                                          \
		}                                                                                            \
	} while (false)
