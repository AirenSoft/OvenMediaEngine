//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "transcode_decode_info.h"

enum class ProviderType
{
    unknown,
    rtmp,
    webrtc,
    mpegts
};

class ProviderInfo
{
public:
    ProviderInfo();
    virtual ~ProviderInfo();

    const ProviderType GetType() const noexcept;
    void SetType(ProviderType type);
    void SetTypeFromString(ov::String type_string);

    std::shared_ptr<TranscodeDecodeInfo> GetDecode() const noexcept;
    void SetDecode(std::shared_ptr<TranscodeDecodeInfo> decode);

    const ov::String GetDecodeRef() const noexcept;
    void SetDecodeRef(ov::String decode_ref);

    // Utilities
    static const char *StringFromProviderType(ProviderType provider_type) noexcept;
    static const ProviderType ProviderTypeFromString(ov::String type_string) noexcept;

    ov::String ToString() const;

private:
    ProviderType _type;
    std::shared_ptr<TranscodeDecodeInfo> _decode;

    ov::String _decode_ref;
};
