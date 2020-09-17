#include "rtcp_info.h"
#include <base/ovlibrary/byte_io.h>

#include "rtcp_private.h"

bool RtcpHeader::Parse(const uint8_t* buffer, const size_t buffer_size, size_t &block_size)
{
	if(buffer_size < RTCP_HEADER_SIZE)
	{
		logtd("RTCP block size is too small(%d bytes), block size must be bigger than 4 bytes", RTCP_HEADER_SIZE);
		return false;
	}

	_version = buffer[0] >> 6;
	if(_version != RTCP_HEADER_VERSION)
	{
		logtd("Invalid RTCP header : version must be 2");
		return false;
	}

	_has_padding = (buffer[0] & 0x20) != 0;
	_count_or_format = buffer[0] & 0x1F;
	_type = static_cast<RtcpPacketType>(buffer[1]);

	// RFC3550
	// The length of this RTCP packet in 32-bit words minus one,      
	// including the header and any padding.  (The offset of one makes
	// zero a valid length and avoids a possible infinite loop in      
	// scanning a compound RTCP packet, while counting 32-bit words      
	// avoids a validity check for a multiple of 4.)

	// (length + 1) * 4 ==> RTCP block size	
	// == 4*length + 4*1 ==> RTCP block size
	// RTCP Header size = 4
	// RTCP block size - RTCP Header size(4) = payload size
	block_size = (ByteReader<uint16_t>::ReadBigEndian(&buffer[2]) + 1) * 4;
	_payload_size = block_size - RTCP_HEADER_SIZE;
	_payload = buffer + RTCP_HEADER_SIZE;

	if(buffer_size < RTCP_HEADER_SIZE + _payload_size)
	{
		logtd("The rtcp block size is too small : block size(%d) must be bigger than (headers size + payload size : %d)", 
				buffer_size, RTCP_HEADER_SIZE + _payload_size);
		return false;
	}

	if(_has_padding)
	{
		if(_payload_size == 0)
		{
			logtd("Invalid RTCP header : the header has padding but payload size is 0");
			return false;
		}

		_padding_size = _payload[_payload_size - 1];
		if(_padding_size == 0)
		{	
			logtd("Invalid RTCP header : the header has padding but padding size is 0");
			return false;
		}

		if(_padding_size > _payload_size)
		{
			logtd("Invalid RTCP header : too many padding bytes(%d), it must be less than payload size (%d)", 
				_padding_size, _payload_size);
			return false;
		}

		_payload_size -= _padding_size;
	}

	return true;
}