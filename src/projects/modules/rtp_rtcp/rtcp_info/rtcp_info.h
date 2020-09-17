#pragma once
#include "base/ovlibrary/ovlibrary.h"

#define RTCP_HEADER_VERSION         (2)
#define RTCP_HEADER_SIZE            (4)
#define RTCP_REPORT_BLOCK_LENGTH    (24)
#define RTCP_MAX_BLOCK_COUNT        (0x1F)

// packet type
enum class RtcpPacketType : uint8_t
{
    SR = 200,   // Sender Report
    RR = 201,   // Receiver Report
    SDES = 202, // Source Description message
    BYE = 203,  // Bye message
    APP = 204,  // Application specfic RTCP
	RTPFB = 205, // Transport layer feedback message : RFC 4585
	PSFB = 206,	// Payload-specific feedback message : RFC 4585
	XR = 207,	// Extended Reports	: RFC 3611 (RTCP XR)
};

enum class RTPFBFMT : uint8_t
{
	NACK = 1,	// General Negative acknowledgements
	EXT = 31, 	// Reserved for future extensions
};

enum class PSFBFMT : uint8_t
{
	PLI = 1,	// Picture Loss Indication
	SLI = 2,	// Slice Loss Indication
	RPSI = 3,	// Reference Picture Selection Indication
	AFB = 15,	// Application Layer Feedback
	EXT = 31,	// Reserved for future extensions.
};

class RtcpHeader
{
public:
	bool Parse(const uint8_t* buffer, const size_t buffer_size, size_t &block_size);
	
	RtcpPacketType GetType() const {return _type;}
	uint8_t GetReportCount() const {return _count_or_format;}
	uint8_t GetFMT() const {return _count_or_format;}
	uint16_t GetLength() const {return _length;}
	const uint8_t* GetPayload() const {return _payload;}
	size_t GetPayloadSize() const {return _payload_size;}

private:
	uint8_t				_version = 2; // always 2
	bool				_has_padding = false;
	size_t				_padding_size = 0;
	uint8_t				_count_or_format = 0; // depends on packet type (if the packet type is fb then this valuable will be used as format)
	RtcpPacketType		_type;
	uint16_t			_length = 0;
	const uint8_t*		_payload = nullptr;
	size_t				_payload_size = 0;
};

// Base class of RTCP information
class RtcpInfo
{
public:
	virtual bool Parse(const RtcpHeader &packet) = 0;

	// RtcpInfo must provide packet type
	virtual RtcpPacketType GetPacketType() = 0;

	virtual uint8_t GetCount()
	{
		return _count_or_fmt;
	}
	// If the packet type is one of the feedback messages (205, 206) child must override this function
	virtual uint8_t GetFmt()
	{
		return _count_or_fmt;
	}
	
	// RtcpInfo must provide raw data
	virtual std::shared_ptr<ov::Data> GetData() = 0;

	virtual void DebugPrint() = 0;

protected:
	void SetCount(uint8_t count) {_count_or_fmt = count;}
	void SetFmt(uint8_t fmt) {_count_or_fmt = fmt;}

private:
	uint8_t		_count_or_fmt = 0;
};