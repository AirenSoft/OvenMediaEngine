#include <base/ovlibrary/byte_io.h>
#include "rtp_depacketizer_generic_audio.h"

std::shared_ptr<ov::Data> RtpDepacketizerGenericAudio::ParseAndAssembleFrame(std::vector<std::shared_ptr<ov::Data>> payload_list)
{
	if(payload_list.size() <= 0)
	{
		return nullptr;
	}

	auto bitstream = std::make_shared<ov::Data>();

	for(const auto &payload : payload_list)
	{
		bitstream->Append(payload);
	}

	return bitstream;
}