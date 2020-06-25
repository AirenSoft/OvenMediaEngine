#pragma once

// This code is referenced from https://rosettacode.org/wiki/CRC-32#C

#include "base/common_types.h"

namespace ov
{
	class CRC
	{
	public:
		static uint32_t Crc32(uint32_t crc, const uint8_t *buf, size_t len)
		{
			static uint32_t table[256];
			static int have_table = 0;
			uint32_t rem;
			uint8_t octet;
			int i, j;
			const uint8_t *p, *q;
		
			/* This check is not thread safe; there is no mutex. */
			if (have_table == 0) 
			{
				/* Calculate CRC table. */
				for (i = 0; i < 256; i++) 
				{
					rem = i;  /* remainder from polynomial division */
					for (j = 0; j < 8; j++) 
					{
						if (rem & 1) 
						{
							rem >>= 1;
							rem ^= 0x04C11DB7;
						} 
						else
						{
							rem >>= 1;
						}
					}

					table[i] = rem;
				}
				have_table = 1;
			}
		
			crc = ~crc;
			q = buf + len;
			for (p = buf; p < q; p++) 
			{
				octet = *p;  /* Cast to unsigned octet. */
				crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
			}

			return ~crc;
		}
	};
}