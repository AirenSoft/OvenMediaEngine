#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/ovlibrary/ovlibrary.h>

#include <cstdint>

// baseline & lvl 3.1 => profile-level-id=42e01f
#define H264_CONVERTER_DEFAULT_PROFILE "42e01f"

class H264Converter
{
public:
	static bool GetExtraDataFromAvcc(const cmn::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata);
	static bool ConvertAvccToAnnexb(cmn::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata);

	static ov::String GetProfileString(const std::vector<uint8_t> &codec_extradata);

	static std::shared_ptr<const ov::Data> ConvertAnnexbToAvcc(const std::shared_ptr<const ov::Data> &data);
};
