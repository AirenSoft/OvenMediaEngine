//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./data.h"
#include "./assert.h"
#include "./ovlibrary_private.h"

#define __STDC_FORMAT_MACROS

#include <cinttypes>
#include <cctype>
#include <cmath>

namespace ov
{
	//TODO: 성능 최적화 필요
	String ToHexStringWithDelimiter(const void *data, ssize_t length, char delimiter)
	{
		String dump;

		const auto *buffer = static_cast<const uint8_t *>(data);

		for(int index = 0; index < length; index++)
		{
			dump.AppendFormat("%02X", *buffer);

			if(index < length - 1)
			{
				dump.Append(delimiter);
			}
			buffer++;
		}

		return dump;
	}

	String ToHexString(const void *data, ssize_t length)
	{
		String dump;

		const auto *buffer = static_cast<const uint8_t *>(data);

		for(int index = 0; index < length; index++)
		{
			dump.AppendFormat("%02X", *buffer);
			buffer++;
		}

		return dump;
	}

	String Dump(const void *data, ssize_t length, const char *title, ssize_t offset, ssize_t max_bytes, const char *line_prefix) noexcept
	{
		if(offset > length)
		{
			offset = length;
		}

		String dump;
		const char *buffer = ((const char *)data + offset);
		max_bytes = std::min((length - offset), max_bytes);

		// 최대 1MB 까지 덤프 가능
		int dump_bytes = (int)std::min(max_bytes, (1024L * 1024L));

		if(title == nullptr)
		{
			title = "Memory dump of";
		}

		if(line_prefix == nullptr)
		{
			line_prefix = "";
		}

		if(offset > 0)
		{
			// ========== xxxxx 0x12345678 + 0xABCDEF01 (102400 / 1024000) ==========
			dump.AppendFormat("%s========== %s 0x%08X + 0x%08X (%d/%" PRIi64 " bytes) ==========", line_prefix, title, data, offset, dump_bytes, length);
		}
		else
		{
			// ========== xxxxx 0x12345678 (102400 / 1024000) ==========
			dump.AppendFormat("%s========== %s 0x%08X (%d/%" PRIi64 " bytes) ==========", line_prefix, title, data, dump_bytes, length);
		}

		if(dump_bytes == 0L)
		{
			dump.AppendFormat("\n%s(No data to show)", line_prefix);
		}
		else
		{
			String ascii;
			int padding = (int)(log((double)dump_bytes) / log(16.0)) + 1;
			String number = String::FormatString("\n%s%%0%dX | ", line_prefix, padding);

			if(dump_bytes > 0)
			{
				dump.AppendFormat(number, 0L);
			}

			// 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F | 0123456789ABCDEF
			for(int index = 0; index < dump_bytes; index++)
			{
				if((index > 0) && (index % 16 == 0))
				{
					dump.AppendFormat("| %s", ascii.CStr());

					if(index <= (dump_bytes - 1))
					{
						dump.AppendFormat(number, index);
					}

					ascii = "";
				}

				char character = buffer[index];

				if(!isprint(character))
				{
					character = '.';
				}

				ascii.Append(character);
				dump.AppendFormat("%02X ", (unsigned char)(buffer[index]));
			}

			if(ascii.IsEmpty() == false)
			{
				if(dump_bytes % 16 > 0)
				{
					String padding;

					padding.PadRight((16 - (dump_bytes % 16)) * 3);

					dump.Append(padding);
				}

				dump.AppendFormat("| %s", ascii.CStr());
			}
		}

		return dump;
	}
}