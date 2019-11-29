#include "relay_datastructure.h"

RelayPacket::RelayPacket(RelayPacketType type)
{
    this->type = type;
}

RelayPacket::RelayPacket(const ov::Data *data)
{
    if(data->GetLength() == sizeof(RelayPacket))
    {
        ::memcpy(this, data->GetData(), sizeof(RelayPacket));

        if(GetDataSize() > RelayPacketDataSize)
        {
            OV_ASSERT(GetDataSize() <= RelayPacketDataSize, "Data size %d exceedes %d", GetDataSize(), RelayPacketDataSize);
            _data_size = ov::HostToBE16(RelayPacketDataSize);
        }
    }
    else
    {
        OV_ASSERT(data->GetLength() == sizeof(RelayPacket), "Invalid data size: %zu, expected: %zu", data->GetLength(), sizeof(RelayPacket));
    }
}

RelayPacketType RelayPacket::GetType() const
{
    return type;
}

void RelayPacket::SetTransactionId(uint32_t transaction_id)
{
    _transaction_id = ov::HostToBE32(transaction_id);
}

uint32_t RelayPacket::GetTransactionId() const
{
    return ov::BE32ToHost(_transaction_id);
}

void RelayPacket::SetApplicationId(uint32_t app_id)
{
    _application_id = ov::HostToBE32(app_id);
}

uint32_t RelayPacket::GetApplicationId() const
{
    return ov::BE32ToHost(_application_id);
}

void RelayPacket::SetStreamId(uint32_t stream_id)
{
    _stream_id = ov::HostToBE32(stream_id);
}

uint32_t RelayPacket::GetStreamId() const
{
    return ov::BE32ToHost(_stream_id);
}

void RelayPacket::SetMediaType(int8_t media_type)
{
    _media_type = media_type;
}

int8_t RelayPacket::GetMediaType() const
{
    return _media_type;
}

void RelayPacket::SetTrackId(uint32_t track_id)
{
    _track_id = ov::HostToBE32(track_id);
}

uint32_t RelayPacket::GetTrackId() const
{
    return ov::BE32ToHost(_track_id);
}

void RelayPacket::SetPts(uint64_t pts)
{
    _pts = ov::HostToBE64(pts);
}

uint64_t RelayPacket::GetPts() const
{
    return ov::BE64ToHost(_pts);
}

void RelayPacket::SetDts(int64_t dts)
{
    _dts = ov::HostToBE64(dts);
}

int64_t RelayPacket::GetDts() const
{
    return ov::BE64ToHost(_dts);
}

void RelayPacket::SetDuration(uint64_t duration)
{
    _duration = ov::HostToBE64(duration);
}

uint64_t RelayPacket::GetDuration() const
{
    return ov::BE64ToHost(_duration);
}

void RelayPacket::SetFlag(uint8_t flag)
{
    _flag = flag;
}

uint8_t RelayPacket::GetFlag() const
{
    return _flag;
}

void RelayPacket::SetData(const void *data, uint16_t length)
{
    if(length <= RelayPacketDataSize)
    {
        ::memcpy(_data, data, length);
        _data_size = ov::HostToBE16(length);
    }
    else
    {
        OV_ASSERT2(length <= RelayPacketDataSize);
    }
}

const void *RelayPacket::GetData() const
{
    return _data;
}

void *RelayPacket::GetData()
{
    return _data;
}

void RelayPacket::SetDataSize(uint16_t data_size)
{
    _data_size = ov::HostToBE16(data_size);
}

const uint16_t RelayPacket::GetDataSize() const
{
    return ov::BE16ToHost(_data_size);
}

void RelayPacket::SetStart(bool start)
{
    _start_indicator = static_cast<uint8_t>(start ? 1 : 0);
}

void RelayPacket::SetEnd(bool end)
{
    _end_indicator = static_cast<uint8_t>(end ? 1 : 0);
}

bool RelayPacket::IsEnd() const
{
    return (_end_indicator == 1);
}

bool RelayPacket::IsStart() const
{
    return (_start_indicator == 1);
}