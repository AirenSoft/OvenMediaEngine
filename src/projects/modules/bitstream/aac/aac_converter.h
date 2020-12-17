#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "base/mediarouter/media_buffer.h"
#include <cstdint>

class AacConverter
{
public:
	static bool GetExtraDataFromLatm(const cmn::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata);
	static bool ConvertLatmToAdts(const cmn::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata);
	static std::shared_ptr<const ov::Data> ConvertAdtsToLatm(const std::shared_ptr<const ov::Data> &data, std::vector<size_t> *length_list);

protected:
	static std::shared_ptr<ov::Data> MakeAdtsHeader(uint8_t aac_profile, uint8_t aac_sample_rate, uint8_t aac_channels, int16_t data_length);
};
