//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG                          "HttpServer"

#define HTTP_CHECK_METHOD(method, flag)     OV_CHECK_FLAG((uint16_t)method, (uint16_t)flag)
