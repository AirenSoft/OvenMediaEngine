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
#include "transcode_encode_info.h"

enum class PublisherType
{
    unknown,
    webrtc,
    rtmp,
    hls,
    dash
};

class PublisherInfo
{
public:
    PublisherInfo();
    virtual ~PublisherInfo();

    const PublisherType GetType() const noexcept;
    void SetType(PublisherType type);
    void SetTypeFromString(ov::String type_string);

    std::vector<std::shared_ptr<TranscodeEncodeInfo>> GetEncodes() const noexcept;
    void AddEncode(std::shared_ptr<TranscodeEncodeInfo> encode);

    const ov::String GetEncodesRef() const noexcept;
    void SetEncodesRef(ov::String encodes_ref);

    static const char *StringFromPublisherType(PublisherType publisher_type) noexcept;
    static const PublisherType PublisherTypeFromString(ov::String type_string) noexcept;

    ov::String ToString() const;

private:
    PublisherType _type;
    std::vector<std::shared_ptr<TranscodeEncodeInfo>> _encodes;

    ov::String _encodes_ref;
};
