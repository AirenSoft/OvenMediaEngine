#include "bitstream_conv.h"


BitstreamConv::BitstreamConv()
{

}

BitstreamConv::~BitstreamConv()
{

}

void BitstreamConv::setType(ConvType type)
{
}

BitstreamConv::ResultType BitstreamConv::Convert(ConvType conv_type, const std::shared_ptr<ov::Data> &data)
{
	return ResultType::SuccessWithoutData;
}

BitstreamConv::ResultType BitstreamConv::Convert(ConvType conv_type, const std::shared_ptr<ov::Data> &data, int64_t &cts)
{
	return ResultType::SuccessWithoutData;
}
