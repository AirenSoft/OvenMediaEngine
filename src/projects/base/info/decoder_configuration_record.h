#pragma once

#include <base/ovlibrary/ovlibrary.h>

class DecoderConfigurationRecord
{
public:
	std::shared_ptr<ov::Data> GetData()
	{
		if (_updated)
		{
			_data = Serialize();
			_updated = false;
		}

		return _data;
	}

	virtual bool Parse(const std::shared_ptr<ov::Data> &data) = 0;
	virtual bool IsValid() const = 0;
	virtual bool Equals(const std::shared_ptr<DecoderConfigurationRecord> &other) = 0;

	// RFC 6381
	virtual ov::String GetCodecsParameter() const = 0;

protected:
	virtual std::shared_ptr<ov::Data> Serialize() = 0;

	// Set serialized data
	void SetData(const std::shared_ptr<ov::Data> &data)
	{
		_data = data;
		_updated = false;
	}

	void UpdateData()
	{
		_updated = true;
	}

private:
	std::shared_ptr<ov::Data> _data = nullptr;
	bool _updated = true;
};