#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/ovlibrary/ovlibrary.h>

#include <cstdint>

class H264Converter
{
public:
	static bool GetExtraDataFromAvcc(const cmn::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata);
	static bool ConvertAvccToAnnexb(cmn::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata);

	static std::shared_ptr<const ov::Data> ConvertAnnexbToAvcc(const std::shared_ptr<const ov::Data> &data);
};
