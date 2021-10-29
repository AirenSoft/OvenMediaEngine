#pragma once

#include <base/ovlibrary/ovlibrary.h>

class RtpHeaderExtension
{
public:

	enum class HeaderType : uint8_t
	{
		ONE_BYTE_HEADER,
		TWO_BYTE_HEADER
	};

	RtpHeaderExtension(uint8_t id)
	{
		_id = id;
	}

	RtpHeaderExtension(uint8_t id, std::shared_ptr<ov::Data> data)
	{
		_id = id;
		SetData(data);
	}

	uint8_t GetId()
	{
		return _id;
	}

	// If child class updates it's value, the child must call UpdateData function
	void UpdateData()
	{
		_updated = true;
	}

	// Serealize
	std::shared_ptr<ov::Data> Marshal(HeaderType type)
	{
		if(_updated == false)
		{
			return _element;
		}

		auto header_length = (type == HeaderType::ONE_BYTE_HEADER ? 1 : 2);

		auto data = GetData(type);
		if(data == nullptr || data->GetLength() == 0)
		{
			return nullptr;
		}

		_element = std::make_shared<ov::Data>();
		_element->SetLength(header_length + data->GetLength());
		auto buffer = _element->GetWritableDataAs<uint8_t>();

		if(type == HeaderType::ONE_BYTE_HEADER)
		{
			if(data->GetLength() > 0xF + 1)
			{
				// cannot convert to ONE_BYTE_HEADER type
				return nullptr;
			}

			// https://datatracker.ietf.org/doc/html/rfc8285#section-4.2
			// The 4-bit length is the number, minus one, of data bytes of this
  			// header extension element following the one-byte header.
			buffer[0] = _id << 4 | (data->GetLength() - 1);
			memcpy(&buffer[1], data->GetData(), data->GetLength());
		}
		else
		{
			if(data->GetLength() > 0xFF)
			{
				// cannot convert to ONE_BYTE_HEADER type
				return nullptr;
			}

			buffer[0] = _id;
			buffer[1] = static_cast<uint8_t>(data->GetLength());
			memcpy(&buffer[2], data->GetData(), data->GetLength());
		}

		_updated = false;

		return _element;
	}
	
protected:
	// child class must implement below functions
	virtual std::shared_ptr<ov::Data> GetData(HeaderType type)
	{
		return _payload_data;
	}

	virtual bool SetData(const std::shared_ptr<ov::Data> &data)
	{
		_payload_data = data;
		return true;
	}

private:
	uint8_t		_id;
	std::shared_ptr<ov::Data> _payload_data = nullptr;
	std::shared_ptr<ov::Data> _element = nullptr;

	bool _updated = true;
};