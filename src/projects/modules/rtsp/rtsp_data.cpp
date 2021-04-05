#include "rtsp_data.h"

std::tuple<std::shared_ptr<RtspData>, int> RtspData::Parse(const std::shared_ptr<const ov::Data> &data)
{
	return {nullptr, -1};
}

RtspData::RtspData(uint8_t channel_id, const std::shared_ptr<ov::Data> &data)
{
	_channel_id = channel_id;
	_data = data;
}

uint8_t RtspData::GetChannelId()
{
	return _channel_id;
}

std::shared_ptr<ov::Data> RtspData::GetData()
{
	return _data;
}