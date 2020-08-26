#include "h265_sps.h"

bool H265Sps::Parse(const uint8_t *sps_bitstream, size_t length, H264Sps &sps)
{

	return true;
}

unsigned int H265Sps::GetWidth() const
{
	return _width;
}
unsigned int H265Sps::GetHeight() const
{
	return _height;
}
uint8_t H265Sps::GetProfile() const
{
	return _profile;
}
uint8_t H265Sps::GetCodecLevel() const
{
	return _codec_level;
}
unsigned int H265Sps::GetFps() const
{
	return _fps;
}
unsigned int H265Sps::GetId() const
{
	return _id;
}
unsigned int H265Sps::GetMaxNrOfReferenceFrames() const
{
	return _max_nr_of_reference_frames;
}

ov::String H265Sps::GetInfoString()
{
	ov::String out_str = ov::String::FormatString("\n[H264Sps]\n");

	out_str.AppendFormat("\tProfile(%d)\n", GetProfile());
	out_str.AppendFormat("\tCodecLevel(%d)\n", GetCodecLevel());
	out_str.AppendFormat("\tWidth(%d)\n", GetWidth());
	out_str.AppendFormat("\tHeight(%d)\n", GetHeight());
	out_str.AppendFormat("\tFps(%d)\n", GetFps());
	out_str.AppendFormat("\tId(%d)\n", GetId());
	out_str.AppendFormat("\tMaxNrOfReferenceFrames(%d)\n", GetMaxNrOfReferenceFrames());
	out_str.AppendFormat("\tAspectRatio(%d:%d)\n", _aspect_ratio._width, _aspect_ratio._height);

	return out_str;
}
