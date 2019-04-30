//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "url.h"

namespace cfg
{
    struct CrossDomain : public Item
    {
        const std::vector<Url> &GetUrls() const
        {
            return _url_list;
        }

    protected:
        void MakeParseList() const override
        {
            RegisterValue("Url", &_url_list);
        }

        std::vector<Url> _url_list;
    };
}