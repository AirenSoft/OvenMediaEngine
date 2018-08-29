//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory.h>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <map>
#include <vector>

namespace ov
{
	class Data;

	class String
	{
	public:
		String() = default;
		String(const char *string, ssize_t length = -1); // NOLINT

		// copy constructor
		String(const String &str);

		// move constructor
		String(String &&str) noexcept;

		~String();

		operator const char *() const noexcept; // NOLINT
		const char *CStr() const noexcept;
		// 수정 가능한 버퍼 반환. 반드시 SetLength()를 호출하였거나, 길이를 확실하게 알고 있는 경우에만 이 버퍼를 받아서 사용해야 함
		char *GetBuffer() noexcept;

		// 문자열 조작 API
		String &operator =(const String &buffer) noexcept;
		String &operator =(const char *buffer) noexcept;
		const String &operator +=(const char *buffer) noexcept;
		String operator +(const String &other) noexcept;

		// 문자 추가
		bool Prepend(char c);
		bool Prepend(const char *string, ssize_t length = -1);

		bool Append(char c);
		bool Append(const char *string, ssize_t length = -1);

		ssize_t AppendFormat(const char *format, ...);
		ssize_t AppendVFormat(const char *format, va_list list);
		ssize_t Format(const char *format, ...);
		ssize_t VFormat(const char *format, va_list list);

		static String FormatString(const char *format, ...);

		ssize_t IndexOf(char c, ssize_t start_position = 0) const noexcept;
		ssize_t IndexOf(const char *str, ssize_t start_position = 0) const noexcept;
		// start_position 부터 시작해서 0번째 글자까지 탐색함. -1일 경우 문자열 길이로 대체
		ssize_t IndexOfRev(char c, ssize_t start_position = -1) const noexcept;

		// in-place utilities
		void PadLeft(ssize_t length, char pad = ' ');
		void PadRight(ssize_t length, char pad = ' ');
		void MakeUpper();
		void MakeLower();

		// out-of-place utilties
		String Replace(const char *old_token, const char *new_token) const;
		String Substring(ssize_t start, ssize_t length = -1) const;
		String Trim() const;

		String PadLeftString(ssize_t length, char pad = ' ') const;
		String PadRightString(ssize_t length, char pad = ' ') const;
		String UpperCaseString() const;
		String LowerCaseString() const;

		std::vector<String> Split(const char *separator) const;
		std::vector<String> Split(const char *string, const char *separator) const;

		static String Join(const std::vector<String> &list, const char *seperator);

		bool HasPrefix(String prefix) const;
		bool HasSuffix(String suffix) const;

		String Left(ssize_t length) const;
		String Right(ssize_t length) const;

		// index번째 글자를 가져옴
		char Get(ssize_t index) const;
		char operator [](ssize_t index) const;

		// 비교 연산
		bool operator !=(const char *buffer) const;
		bool operator ==(const String &str) const;
		bool operator ==(const char *buffer) const;
		bool operator <(const String &string) const;
		bool operator >(const String &string) const;

		// 할당된 메모리 크기를 얻어옴
		ssize_t GetCapacity() const noexcept;
		// 메모리 공간만 미리 할당해 놓음
		// 보통은 GetBuffer() 를 사용하기 위해 호출함
		bool SetCapacity(ssize_t length) noexcept;

		// 문자열 길이 얻어옴
		ssize_t GetLength() const noexcept;
		// 문자열 길이를 강제로 조절함
		// 만약, 새로운 길이가 더 길다면 길어진 만큼 0으로 채움 (따라서, 보통은 의미 없음)
		// 만약, 새로운 길이가 더 짧다면 문자열을 자름 (대부분 이 용도로 사용함)
		bool SetLength(ssize_t length) noexcept;

		// 길이가 0인 문자열로 만듦
		bool Clear();

		bool IsEmpty() const noexcept;

		std::shared_ptr<Data> ToData(bool include_null_char = true) const;

	protected:
		bool Alloc(ssize_t length, bool alloc_exactly = false) noexcept;
		bool Release() noexcept;

	private:
		char *_buffer = nullptr;

		// 실제 데이터가 있는 길이
		ssize_t _length = 0;

		// 거의 지수 형태로 증가함
		ssize_t _capacity = 0;
	};

	struct CaseInsensitiveComparator
	{
		bool operator ()(const String &s1, const String &s2) const
		{
			return s1.UpperCaseString() < s2.UpperCaseString();
		}
	};
}
