//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================

#include "h264_sei.h"

#include <base/ovlibrary/bit_reader.h>
#include <base/ovlibrary/bit_writer.h>
#include <base/ovlibrary/hex.h>
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "H264SEI"


std::shared_ptr<ov::Data> H264SEI::Serialize()
{
	// ------------------------------
	// SEI Payload Data
	// ------------------------------	
	ov::BitWriter sei_payload_data(1024);

	// Payload Data : UUID + (TimeCode)
	switch (_payload_data_type)
	{
		case PayloadDataType::RAW:
			break;
		case PayloadDataType::TYPE1:
		{
			// Write UUID
			sei_payload_data.WriteData(OME_PAYLOAD_DATA_TYPE1_UUID, sizeof(OME_PAYLOAD_DATA_TYPE1_UUID));

			// Write TimeCode
			sei_payload_data.WriteBytes<uint64_t>(_payload_time_code);			
			break;
		}
	}

	if(_payload_data != nullptr)
	{
		// Write Data
		sei_payload_data.WriteData(_payload_data->GetDataAs<uint8_t>(), _payload_data->GetLength());
	}

	// ------------------------------
	// SEI Payload
	// ------------------------------	

	// Calculate Payload Size
	auto payload_size = sei_payload_data.GetDataSize();

	ov::BitWriter writer(payload_size + 3);  // payload data + payload_size(max 2)  + rbsp

	// Payload Type (8)
	writer.WriteBytes<uint8_t>(_payload_type);

	// Payload Size 
	auto remaining_size = payload_size;
	while(remaining_size >= 255)
	{
		writer.WriteBytes<uint8_t>(255);
		remaining_size -= 255;
	}
	writer.WriteBytes<uint8_t>(remaining_size);

	// Payload Data
	writer.WriteData(sei_payload_data.GetData(), sei_payload_data.GetDataSize());

	// RBSP_trailing_bits
	writer.WriteBits(8, _rbsp_trailing_bits);

	return writer.GetDataObject();
}


ov::String H264SEI::GetInfoString()
{
	ov::String info;

	info.AppendFormat("Payload Type: %s (%u, ", PayloadTypeToString(_payload_type).CStr(), static_cast<uint8_t>(_payload_type));
	info.AppendFormat("Payload Data (Type): %s, ", PayloadDataTypeToString(_payload_data_type).CStr());
	info.AppendFormat("Payload Data (Timecode): %llu, ", _payload_time_code);
	if(_payload_data != nullptr)
	{
		info.AppendFormat("Payload Data (Data): %d bytes\n%s", _payload_data->GetLength(), _payload_data->Dump(nullptr, nullptr).CStr());
	}

	return info;
}