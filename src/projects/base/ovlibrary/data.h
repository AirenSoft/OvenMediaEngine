//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./string.h"
#include "./assert.h"
#include "./enable_shared_from_this.h"
#include "./memory_utilities.h"

#include <memory>
#include <algorithm>

namespace ov
{
	// Dump utility

	// 데이터를 0001020304A0과 같은 hex string으로 변환
	String ToHexString(const void *data, ssize_t length);
	// 데이터를 00:01:02:A0 과 같이 delimeter를 낀 hex string으로 변환
	String ToHexStringWithDelimiter(const void *data, ssize_t length, char delimiter);


	// 데이터를 hex dump (디버깅 용도)
	String Dump(const void *data, ssize_t length, const char *title = nullptr, ssize_t offset = 0, ssize_t max_bytes = 1024, const char *line_prefix = nullptr) noexcept;

	template<typename T>
	String Dump(const T &data, const char *title = nullptr, ssize_t offset = 0, ssize_t max_bytes = 1024, const char *line_prefix = nullptr) noexcept
	{
		return Dump(&data, sizeof(T), title, offset, max_bytes, line_prefix);
	}

	// Data class
	class Data : public EnableSharedFromThis<Data>
	{
	protected:
		struct FactoryHelper;

	public:
		// Factory
		explicit Data(const FactoryHelper &helper)
			: Data()
		{
		}

		// shared_ptr로만 관리하므로, 복사/이동생성자는 필요 없음
		Data(const Data &data) = delete;
		Data(Data &&data) = delete;

		/// 비어있는 instance 생성
		///
		/// @return 생성된 Data instance
		static std::shared_ptr<Data> CreateData()
		{
			return std::make_shared<Data>(FactoryHelper());
		}

		/// capacity 만큼 미리 메모리를 할당하며 instance 생성
		static std::shared_ptr<Data> CreateData(ssize_t capacity)
		{
			auto instance = CreateData();

			if(instance->Reserve(capacity))
			{
				return instance;
			}

			return nullptr;
		}

		/// 다른 data로 부터 instance 생성
		///
		/// @param data 원본 data
		/// @param length 원본 data 길이
		/// @param copy_data 데이터를 실제로 복사할지 여부
		///
		/// @remarks copy_data가 true일 경우, 메모리를 새롭게 할당한 뒤 복사하므로 입력된 data가 유효하지 않게 되더라도 영향을 받지 않음.
		///          copy_data가 false일 경우, 별도로 메모리를 복사하지 않고 포인터만 참조하므로 입력된 data가 유효하지 않게 되면 문제가 발생함.
		///          또한, read-only로 동작하기 때문에 데이터를 조작하는 API(Write(), Append() 등)를 호출하면 실패함
		static std::shared_ptr<Data> CreateData(const void *data, ssize_t length, bool copy_data = true)
		{
			if(data == nullptr)
			{
				OV_ASSERT2(false);
				return nullptr;
			}

			if(copy_data)
			{
				auto instance = CreateData();

				// 데이터 복사
				if(instance->Append(data, length))
				{
					// 인스턴스 반환
					return instance;
				}

				// 데이터 추가 실패
			}
			else
			{
				auto instance = CreateData();

				// 데이터를 복사하지 않고, 참조만 함 (기록은 할 수 없음)
				instance->_reference_data = data;
				instance->_length = length;

				return instance;
			}

			return nullptr;
		}

		// copy-on-write 되는 instance 반환
		inline std::shared_ptr<Data> Clone(bool detach = true) const
		{
			if(IsReadOnly())
			{
				return CreateData(_reference_data, _length, true);
			}

			if(detach)
			{
				auto instance = Subdata(0L);

				if(instance->Detach())
				{
					return instance;
				}

				// detach 실패
				return nullptr;
			}

			return Subdata(0L);
		}

		inline bool IsReadOnly() const noexcept
		{
			return (_reference_data != nullptr);
		}

		/// 읽기 전용 포인터를 얻어옴.
		/// copy_data가 false인 상태에서 만들어진 instance일 경우, 반드시 원본 데이터가 유효한 상태여야 함
		///
		/// @return 읽기 전용 포인터
		inline const void *GetData() const
		{
			if(IsReadOnly())
			{
				return _reference_data;
			}

			return _data->data() + _offset;
		}

		template<typename T>
		inline const T *GetDataAs() const
		{
			return reinterpret_cast<const T *>(GetData());
		}

		/// 쓰기가 가능한 포인터를 얻어옴.
		/// copy_data가 false인 상태에서 만들어진 instance일 경우, 반드시 원본 데이터가 유효한 상태여야 함.
		/// 전달받은 포인터에는 (sizeof(T) * capacity) 이내에서 메모리를 수정해야 함.
		///
		/// @return 쓰기 가능한 포인터
		inline void *GetWritableData()
		{
			// 쓰기 버퍼를 준비하기 위해 copy-on-write 시도
			if(Detach() == false)
			{
				// 실패
				return nullptr;
			}

			return _data->data() + _offset;
		}

		template<typename T>
		inline T *GetWritableDataAs()
		{
			return reinterpret_cast<T *>(GetWritableData());
		}

		/// Data 인스턴스 안에 포함되어 있는 데이터의 크기
		///
		/// @return byte 단위의 데이터 크기
		inline ssize_t GetLength() const
		{
			return _length;
		}

		inline bool SetLength(ssize_t length)
		{
			if(IsReadOnly())
			{
				OV_ASSERT2(false);
				return false;
			}

			if(Reserve(length))
			{
				_data->resize((size_t)length);
				_length = length;
				return true;
			}

			return false;
		}

		/// capacity byte만큼 데이터가 저장될 수 있는 공간을 미리 확보.
		///
		/// @param capacity 할당할 메모리 크기를 결정하기 위한 개수 (최소한 capacity만큼 메모리를 할당함)
		///
		/// @return 메모리가 정상적으로 할당되었는지 여부
		///
		/// @remark 기존 데이터는 그대로 유지되며, _reference_data가 설정되어 있을 경우엔 동작하지 않음.
		///         본 메서드가 호출되는 순간, cow가 발생함
		bool Reserve(ssize_t capacity)
		{
			if(capacity < 0L)
			{
				OV_ASSERT2(capacity >= 0L);
				return false;
			}

			if(Detach() == false)
			{
				return false;
			}

			OV_ASSERT2(_data != nullptr);

			_data->reserve(static_cast<unsigned long>(capacity));

			return true;
		}

		/// 할당되어 있는 메모리 크기를 얻어옴.
		///
		/// @return 할당되어 있는 메모리 크기
		inline ssize_t GetCapacity() const noexcept
		{
			OV_ASSERT2(IsReadOnly() == false);

			return _data->capacity();
		}

		/// 버퍼에 있는 데이터 모두 삭제
		///
		/// @return 성공적으로 삭제되었는지 여부
		///
		/// @remarks SetLength()를 통해 크기를 조절할 경우 실제 메모리가 해제되지는 않으므로,
		///          메모리 해제를 원한다면 Release()를 호출해야 함.
		bool Clear() noexcept
		{
			if(IsReadOnly())
			{
				// read-only 데이터는 release 할 수 없음
				OV_ASSERT(false, "Cannot clear read-only data");
				return false;
			}

			if(Detach() == false)
			{
				// copy-on-write 실패
				return false;
			}

			_data->clear();

			return true;
		}

		bool Append(const void *data, ssize_t length)
		{
			if((data == nullptr) || (length < 0))
			{
				OV_ASSERT(false, "Invalid parameter: data: %p, length: %ld", data, length);
				return false;
			}

			if(Detach() == false)
			{
				return false;
			}

			const auto *source = static_cast<const uint8_t *>(data);

			_data->insert(_data->end(), source, source + length);
			_length += length;

			return true;
		}

		bool Append(const std::shared_ptr<const Data> &data)
		{
			return Append(data->GetData(), data->GetLength());
		}

		/// this 데이터의 일부 영역만 참조하는 Data instance 생성
		///
		/// @param offset 원본 데이터에서 참조할 offset
		/// @param length 원본 데이터에서 참조할 바이트 수 (생략시 offset부터 나머지 데이터)
		///
		/// @return 생성된 Data instance
		///
		/// @remarks
		/// 다른 Data instance가 입력될 경우, 기본적으로 copy-on-write 모드로 동작함.
		/// 즉, 원본 데이터나 this에 아무런 조작을 하지 않을 경우 포인터만 보관하고 있으며,
		/// 다른 Data instance의 데이터 혹은 this instance의 데이터가 변경되면 그 때 메모리 복사가 이루어짐.
		std::shared_ptr<Data> Subdata(ssize_t offset, ssize_t length = -1L) const
		{
			if((_data == nullptr) || (IsReadOnly()))
			{
				// 참조 모드 이거나, 아직 할당이 덜 되었다면
				OV_ASSERT2(false);
				return nullptr;
			}

			if(length == -1L)
			{
				// length이 -1로 들어오면, 남은 데이터 전부를 subdata로 함
				length = _length - offset;
			}

			if((offset < 0) || ((offset + length) > _length) || (length < 0))
			{
				// offset 또는 length가 잘못 지정되어 있음
				OV_ASSERT(false, "Invalid offset or length: offset: %ld, length: %ld (current data: %ld bytes)", offset, length, _length);
				return nullptr;
			}

			auto instance = CreateData();

			// 원본 데이터 지정
			instance->_data = _data;

			// offset과 length 지정
			instance->_offset = _offset + offset;
			instance->_length = length;

			return instance;
		}

		String Dump() const noexcept
		{
			return Dump(nullptr, 0L, 1024L, nullptr);
		}

		String Dump(const char *title, const char *line_prefix) const noexcept
		{
			return Dump(title, 0L, 1024L, line_prefix);
		}

		String Dump(const char *title, ssize_t offset = 0L, ssize_t max_bytes = 1024L, const char *line_prefix = nullptr) const noexcept
		{
			return ov::Dump(GetData(), GetLength(), title, offset, max_bytes, line_prefix);
		}

		String ToString() const
		{
			return ov::ToHexString(static_cast<const uint8_t *>(GetData()) + _offset, GetLength());
		}

	protected:
		Data()
			: _reference_data(nullptr),

			  _data(nullptr),

			  _offset(0L),
			  _length(0L)
		{
			_data = std::make_shared<std::vector<uint8_t>>();
		}

		// Data instance를 factory로만 생성할 수 있게 하는 임시 구조체
		// (생성자가 protected일 때, 일반적인 방법으로는 std::make_shared()로 객체를 생성할 수 없음)
		struct FactoryHelper
		{
		};

		// copy on write 해야 할 시점에 호출함
		bool Detach()
		{
			if(IsReadOnly())
			{
				OV_ASSERT(false, "Data is read only");
				return false;
			}

			OV_ASSERT2(_data != nullptr);

			if(_data.use_count() == 1)
			{
				// 데이터가 없거나, 혼자 참조하고 있는 중

				// detach가 필요 없는 상태
				return true;
			}

			// 예전 데이터의 _offset 부터 _length 만큼만 복사
			auto old_data = _data;
			auto begin = old_data->begin() + _offset;
			auto end = old_data->begin() + _offset + _length;

			// detach()될 경우, offset부분부터 새롭게 데이터를 복사하므로, 0으로 초기화
			_offset = 0L;

			_data = std::make_shared<std::vector<uint8_t>>(begin, end);
			OV_ASSERT2(_data != nullptr);

			_data->reserve(old_data->capacity());

			return (_data != nullptr);
		}

		// 데이터 보관용 변수

		// 참조 데이터 (이 데이터가 유효하면(not null) read-only로 동작함)
		const void *_reference_data;

		// 실제 할당된 데이터 (offset을 주어 subdata를 생성한 경우, _current_data와 _data 포인터가 다를 수 있음)
		std::shared_ptr<std::vector<uint8_t>> _data;
		ssize_t _offset;

		// 저장된 데이터 길이
		ssize_t _length;
	};
};