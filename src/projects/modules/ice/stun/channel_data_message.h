#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/byte_io.h>

// https://www.rfc-editor.org/rfc/rfc8656.html#name-the-channeldata-message

#define FIXED_TURN_CHANNEL_HEADER_SIZE	4

class ChannelDataMessage
{
public:
	enum LastErrorCode : uint8_t
	{
		NOT_USED = 0,
		SUCCESS,
		INVALID_DATA,
		NOT_ENOUGH_DATA
	};

	bool LoadHeader(const ov::Data &stream)
	{
		if(stream.GetLength() < FIXED_TURN_CHANNEL_HEADER_SIZE)
		{
			_last_error_code = LastErrorCode::NOT_ENOUGH_DATA;
			return false;
		}

		auto buffer = stream.GetDataAs<uint8_t>();
		_channel_number = ByteReader<uint16_t>::ReadBigEndian(&buffer[0]);
		_data_length = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);

		if(stream.GetLength() < static_cast<size_t>(_data_length - FIXED_TURN_CHANNEL_HEADER_SIZE))
		{
			_last_error_code = LastErrorCode::NOT_ENOUGH_DATA;
			return false;
		}

		return true;
	}

	bool Load(const std::shared_ptr<const ov::Data> &stream)
	{
		return Load(*stream);
	}

	bool Load(const ov::Data &stream)
	{
		if(LoadHeader(stream) == false)
		{
			return false;
		}

		auto buffer = stream.GetDataAs<uint8_t>();
		_data = std::make_shared<ov::Data>(&buffer[0], _data_length);

		_last_error_code = SUCCESS;
		return true;
	}

	LastErrorCode GetLastErrorCode()
	{
		return _last_error_code;
	}

	uint16_t GetChannelNumber()
	{
		return _channel_number;
	}
	uint16_t GetDataLength()
	{
		return _data_length;
	}
	std::shared_ptr<ov::Data> GetData()
	{
		return _data;
	}

private:
	uint16_t	_channel_number = 0;
	uint16_t	_data_length = 0;
	std::shared_ptr<ov::Data>	_data = nullptr;

	LastErrorCode _last_error_code = LastErrorCode::NOT_USED;
};