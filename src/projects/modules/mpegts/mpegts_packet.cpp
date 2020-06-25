//==============================================================================
//
//  MPEGTS Packet
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_packet.h"
#include <base/ovlibrary/byte_io.h>
#include <base/ovlibrary/memory_utilities.h>

#define OV_LOG_TAG	"MPEGTS_PACKET"

namespace mpegts
{
	MpegTsPacket::MpegTsPacket()
	{
		_data = std::make_shared<ov::Data>(MPEGTS_MIN_PACKET_SIZE);
		_buffer = _data->GetWritableDataAs<uint8_t>();
	}

	MpegTsPacket::MpegTsPacket(const std::shared_ptr<ov::Data> &data)
	{
		if(data->GetLength() < MPEGTS_MIN_PACKET_SIZE)
		{
			return;
		}

		_data = data;
		_buffer = _data->GetWritableDataAs<uint8_t>();
	}

	MpegTsPacket::~MpegTsPacket()
	{

	}

	// Getter
	uint8_t MpegTsPacket::SyncByte()
	{
		return _sync_byte;
	}

	bool MpegTsPacket::TransportErrorIndicator()
	{
		return _transport_error_indicator;
	}

	bool MpegTsPacket::PayloadUnitStartIndicator()
	{
		return _payload_unit_start_indicator;
	}

	uint16_t MpegTsPacket::PacketIdentifier()
	{
		return _packet_identifier;
	}

	uint8_t MpegTsPacket::TransportScramblingControl()
	{
		return _transport_scrambling_control;
	}

	uint8_t MpegTsPacket::AdaptationFieldControl()
	{
		return _adaptation_field_control;
	}

	bool MpegTsPacket::HasAdaptationField()
	{
		// 01: No adaptation_field, payload only
		// 10: Adaptation_field only, no payload
		// 11: Adaptation_field followed by payload
		return OV_GET_BIT(_adaptation_field_control, 1);
	}
	
	bool MpegTsPacket::HasPayload()
	{
		// 01: No adaptation_field, payload only
		// 10: Adaptation_field only, no payload
		// 11: Adaptation_field followed by payload
		return OV_GET_BIT(_adaptation_field_control, 0);
	}

	uint8_t MpegTsPacket::ContinuityCounter()
	{
		return _continuity_counter;
	}

	const AdaptationField& MpegTsPacket::GetAdaptationField()
	{
		return _adaptation_field;
	}

	const uint8_t* MpegTsPacket::Payload()
	{
		return _payload;
	}

	size_t MpegTsPacket::PayloadLength()
	{
		return _payload_length;
	}

	uint32_t MpegTsPacket::Parse()
	{
		// already parsed
		if(_ts_parser != nullptr)
		{
			return 0;
		}

		// this time, ome only supports for 188 bytes mpegts packet
		if(_data->GetLength() < MPEGTS_MIN_PACKET_SIZE)
		{
			return 0;
		}

		_ts_parser = std::make_shared<BitReader>(_buffer, _data->GetLength());

		//  76543210  76543210  76543210  76543210
		// [ssssssss][tpTPPPPP][PPPPPPPP][SSaacccc]...

		_sync_byte = _ts_parser->ReadBytes<uint8_t>();
		_transport_error_indicator = _ts_parser->ReadBoolBit();
		if(_transport_error_indicator)
		{
			// error
			return 0;	
		}

		_payload_unit_start_indicator = _ts_parser->ReadBoolBit();
		_transport_priority = _ts_parser->ReadBit();
		_packet_identifier = _ts_parser->ReadBits<uint16_t>(13);
		_transport_scrambling_control = _ts_parser->ReadBits<uint8_t>(2);
		_adaptation_field_control = _ts_parser->ReadBits<uint8_t>(2);
		_continuity_counter = _ts_parser->ReadBits<uint8_t>(4);
		
		if(HasAdaptationField())
		{
			if(ParseAdaptationHeader() == false)
			{
				logte("Could not parse adaptation header");
				return 0;
			}
		}

		if(HasPayload())
		{
			ParsePayload();
		}
		
		// Now, it must be 188 bytes
		return _ts_parser->BytesConsumed();
	}

	bool MpegTsPacket::ParseAdaptationHeader()
	{
		_adaptation_field._length = _ts_parser->ReadBytes<uint8_t>();
		
		_ts_parser->StartSection();

		if(_adaptation_field._length > 0)
		{
			_adaptation_field._discontinuity_indicator = _ts_parser->ReadBoolBit();
			_adaptation_field._random_access_indicator = _ts_parser->ReadBoolBit();
			_adaptation_field._elementary_stream_priority_indicator = _ts_parser->ReadBoolBit();

			// 5 flags
			_adaptation_field._pcr_flag = _ts_parser->ReadBoolBit();
			_adaptation_field._opcr_flag = _ts_parser->ReadBoolBit();
			_adaptation_field._splicing_point_flag = _ts_parser->ReadBoolBit();
			_adaptation_field._transport_private_data_flag = _ts_parser->ReadBoolBit();
			_adaptation_field._adaptation_field_extension_flag = _ts_parser->ReadBoolBit();

			// Need to parse pcr, opcr, splicing_point_flag, _transport_private_data_flag, _adaptation_field_extension_flag
			if(_adaptation_field._pcr_flag == true)
			{
				_adaptation_field._pcr._base = _ts_parser->ReadBits<uint64_t>(33);
				_adaptation_field._pcr._reserved = _ts_parser->ReadBits<uint8_t>(6);
				_adaptation_field._pcr._extension = _ts_parser->ReadBits<uint16_t>(9);
			}

			if(_adaptation_field._opcr_flag == true)
			{
				// We don't use it now, skip for splicing point flag
				_ts_parser->SkipBytes(6);
			}

			if(_adaptation_field._splicing_point_flag == true)
			{
				_adaptation_field._splice_countdown = _ts_parser->ReadBytes<uint8_t>();
			}

			if(_adaptation_field._transport_private_data_flag)
			{
				// We don't use it now
			}

			if(_adaptation_field._adaptation_field_extension_flag)
			{
				// We don't use it now
			}
		}	
		
		// It may contain 
		auto skip_bytes = _adaptation_field._length - _ts_parser->BytesSetionConsumed();

		return _ts_parser->SkipBytes(skip_bytes);
	}

	bool MpegTsPacket::ParsePayload()
	{
		_payload = _ts_parser->CurrentPosition();
		_payload_length = _packet_size - _ts_parser->BytesConsumed();
		
		// Just skip A packet
		return _ts_parser->SkipBytes(_payload_length);
	}
}