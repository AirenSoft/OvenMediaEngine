//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "byte_stream.h"

namespace ov
{
	ByteStream::ByteStream(Data *data)
		: _data(data),
		  _read_only_data(_data),

		  _offset(0L)
	{
	}

	ByteStream::ByteStream(const Data *data)
		: _data(nullptr),
		  _read_only_data(data),

		  _offset(0L)
	{
	}

	ByteStream::ByteStream(const ByteStream &stream)
		: _data(stream._data),
		  _read_only_data(stream._read_only_data),

		  _offset(stream._offset)
	{
	}

	ByteStream::~ByteStream()
	{
	}

	bool ByteStream::Write(const void *data, size_t bytes) noexcept
	{
		if(_data == nullptr)
		{
			OV_ASSERT(false, "Cannot write to read-only data");
			return false;
		}

		if((_offset + bytes) > _data->GetLength())
		{
			// 데이터가 저장될 공간이 없으므로, 메모리를 확장한 뒤,
			if(_data->SetLength(static_cast<size_t>(_offset + bytes)) == false)
			{
				return false;
			}

			// 데이터 복사
		}
		else
		{
			// _data에 데이터가 충분히 들어갈 공간이 있음
		}

		::memcpy(_data->GetWritableDataAs<uint8_t>() + _offset, data, bytes);
		_offset += bytes;

		return true;
	}

	bool ByteStream::Append(const void *data, size_t bytes) noexcept
	{
		if(_data == nullptr)
		{
			OV_ASSERT(false, "Cannot write to read-only data");
			return false;
		}

		return _data->Append(reinterpret_cast<const uint8_t *>(data), bytes);
	}

	size_t ByteStream::Remained() const noexcept
	{
		return Remained<uint8_t>();
	}

	bool ByteStream::IsRemained(size_t bytes) const noexcept
	{
		return Remained() >= bytes;
	}

	Data *ByteStream::GetData() noexcept
	{
		OV_ASSERT2(_data != nullptr);

		return _data;
	}

	std::shared_ptr<const Data> ByteStream::GetRemainData() const noexcept
	{
		return _read_only_data->Subdata(_offset);
	}

	off_t ByteStream::GetOffset() const noexcept
	{
		return _offset;
	}

	bool ByteStream::SetOffset(off_t offset) noexcept
	{
		OV_ASSERT(offset >= 0L, "offset must greater equal than 0: %ld", offset);

		if(offset < 0)
		{
			return false;
		}

		if(offset < static_cast<off_t>(_read_only_data->GetLength()))
		{
			// 그냥 offset만 변경하면 됨
		}
		else
		{
			// _data에 새로운 데이터를 추가 한 뒤 offset을 변경해야 함

			if(_data == nullptr)
			{
				// read-only
				OV_ASSERT(false, "Cannot set offset to read-only data");
				return false;
			}

			if(_data->SetLength(static_cast<size_t>(offset)) == false)
			{
				return false;
			}
		}

		_offset = offset;

		return true;
	}

	bool ByteStream::PushOffset() noexcept
	{
		_offset_stack.push_back(GetOffset());

		return true;
	}

	bool ByteStream::PopOffset() noexcept
	{
		if(_offset_stack.empty() == false)
		{
			off_t offset = _offset_stack.back();

			_offset_stack.pop_back();

			return SetOffset(offset);
		}

		OV_ASSERT(false, "There is no pushed offset");

		return false;
	}

	String ByteStream::Dump(size_t max_bytes, const char *title) const noexcept
	{
		return _read_only_data->Dump(title, _offset, max_bytes);
	}
}