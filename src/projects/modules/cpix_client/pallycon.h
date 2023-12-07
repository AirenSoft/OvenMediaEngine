//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <modules/containers/bmff/cenc.h>

namespace cpix
{
    class Pallycon
    {
    public:
        bool GetKeyInfo(const ov::String &url, const ov::String &token, 
                        const ov::String &content_id, const bmff::DRMSystem &drm_system, const bmff::CencProtectScheme &protect_scheme,
                        bmff::CencProperty &cenc_property);

    private:
        bool MakeRequestBody(const ov::String &content_id, const bmff::DRMSystem &drm_system, const bmff::CencProtectScheme &protect_scheme, ov::String &request_body);
        bool ParseResponse(const std::shared_ptr<ov::Data> &data, bmff::CencProperty &cenc_property);
    };
}