//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#define OV_LOG_TAG                      "OvenMediaEngine"

// Version format: [MAJOR].[MINOR].[RELEASE]
// Major changes
#define OME_VERSION_MAJOR               0
// Minor changes
#define OME_VERSION_MINOR               9
// Total released count since OME project started
#define OME_VERSION_RELEASE             1
// Build date ([yy][mm][dd][hh])
#define OME_VERSION_BUILD               19060300

#define OME_STR_INTERNAL(x)             # x
#define OME_STR(x)                      OME_STR_INTERNAL(x)
#define OME_VERSION                     OME_STR(OME_VERSION_MAJOR) "." OME_STR(OME_VERSION_MINOR) "." OME_STR(OME_VERSION_RELEASE) " (build: " OME_STR(OME_VERSION_BUILD) ")"