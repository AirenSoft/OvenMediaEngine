//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

namespace mon
{
	namespace alrt
	{
		class Message
		{
		public:
			constexpr static const uint32_t INGRESS_CODE_MASK		 = 0x110000;
			constexpr static const uint32_t INGRESS_CODE_STATUS_MASK = INGRESS_CODE_MASK | 0x001000;
			constexpr static const uint32_t INGRESS_CODE_METRIC_MASK = INGRESS_CODE_MASK | 0x002000;
			constexpr static const uint32_t EGRESS_CODE_MASK		 = 0x120000;
			constexpr static const uint32_t EGRESS_CODE_STATUS_MASK	 = EGRESS_CODE_MASK | 0x001000;
			constexpr static const uint32_t EGRESS_CODE_READY_MASK	 = EGRESS_CODE_MASK | 0x002000;
			constexpr static const uint32_t INTERNAL_QUEUE_CODE_MASK = 0x140000;

			enum class Code : uint32_t
			{
				OK,

				// Ingress Codes
				INGRESS_STREAM_CREATED = INGRESS_CODE_STATUS_MASK,
				INGRESS_STREAM_PREPARED,
				INGRESS_STREAM_DELETED,
				INGRESS_STREAM_CREATION_FAILED_DUPLICATE_NAME,


				INGRESS_BITRATE_LOW = INGRESS_CODE_METRIC_MASK,
				INGRESS_BITRATE_HIGH,
				INGRESS_FRAMERATE_LOW,
				INGRESS_FRAMERATE_HIGH,
				INGRESS_WIDTH_SMALL,
				INGRESS_WIDTH_LARGE,
				INGRESS_HEIGHT_SMALL,
				INGRESS_HEIGHT_LARGE,
				INGRESS_SAMPLERATE_LOW,
				INGRESS_SAMPLERATE_HIGH,
				INGRESS_LONG_KEY_FRAME_INTERVAL,
				INGRESS_HAS_BFRAME,

				// Egress Codes
				EGRESS_STREAM_CREATED = EGRESS_CODE_STATUS_MASK,
				EGRESS_STREAM_PREPARED,
				EGRESS_STREAM_DELETED,

				EGRESS_STREAM_CREATION_FAILED_BY_OUTPUT_PROFILE,
				EGRESS_STREAM_CREATION_FAILED_BY_DECODER, 
				EGRESS_STREAM_CREATION_FAILED_BY_ENCODER, 
				EGRESS_STREAM_CREATION_FAILED_BY_FILTER,

				EGRESS_TRANSCODE_DECODING_FAILED,
				EGRESS_TRANSCODE_ENCODING_FAILED,
				EGRESS_TRANSCODE_FILTERING_FAILED,

				EGRESS_LLHLS_READY = EGRESS_CODE_READY_MASK,
				EGRESS_HLS_READY,

				// Internal Codes
				INTERNAL_QUEUE_CONGESTION = INTERNAL_QUEUE_CODE_MASK
			};

			static std::shared_ptr<Message> CreateMessage(Code code, const ov::String &description)
			{
				auto message		  = std::make_shared<Message>();

				message->_code		  = code;
				message->_description = description;

				return message;
			}

			Code GetCode() const
			{
				return _code;
			}

			const ov::String &GetDescription() const
			{
				return _description;
			}

			static ov::String StringFromMessageCode(Code message_code)
			{
				switch (message_code)
				{
					OV_CASE_RETURN(Code::OK, "OK");

					OV_CASE_RETURN(Code::INGRESS_STREAM_CREATED, "INGRESS_STREAM_CREATED");
					OV_CASE_RETURN(Code::INGRESS_STREAM_PREPARED, "INGRESS_STREAM_PREPARED");
					OV_CASE_RETURN(Code::INGRESS_STREAM_DELETED, "INGRESS_STREAM_DELETED");
					OV_CASE_RETURN(Code::INGRESS_STREAM_CREATION_FAILED_DUPLICATE_NAME, "INGRESS_STREAM_CREATION_FAILED_DUPLICATE_NAME");

					OV_CASE_RETURN(Code::INGRESS_BITRATE_LOW, "INGRESS_BITRATE_LOW");
					OV_CASE_RETURN(Code::INGRESS_BITRATE_HIGH, "INGRESS_BITRATE_HIGH");
					OV_CASE_RETURN(Code::INGRESS_FRAMERATE_LOW, "INGRESS_FRAMERATE_LOW");
					OV_CASE_RETURN(Code::INGRESS_FRAMERATE_HIGH, "INGRESS_FRAMERATE_HIGH");
					OV_CASE_RETURN(Code::INGRESS_WIDTH_SMALL, "INGRESS_WIDTH_SMALL");
					OV_CASE_RETURN(Code::INGRESS_WIDTH_LARGE, "INGRESS_WIDTH_LARGE");
					OV_CASE_RETURN(Code::INGRESS_HEIGHT_SMALL, "INGRESS_HEIGHT_SMALL");
					OV_CASE_RETURN(Code::INGRESS_HEIGHT_LARGE, "INGRESS_HEIGHT_LARGE");
					OV_CASE_RETURN(Code::INGRESS_SAMPLERATE_LOW, "INGRESS_SAMPLERATE_LOW");
					OV_CASE_RETURN(Code::INGRESS_SAMPLERATE_HIGH, "INGRESS_SAMPLERATE_HIGH");
					OV_CASE_RETURN(Code::INGRESS_LONG_KEY_FRAME_INTERVAL, "INGRESS_LONG_KEY_FRAME_INTERVAL");
					OV_CASE_RETURN(Code::INGRESS_HAS_BFRAME, "INGRESS_HAS_BFRAME");

					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATED, "EGRESS_STREAM_CREATED");
					OV_CASE_RETURN(Code::EGRESS_STREAM_PREPARED, "EGRESS_STREAM_PREPARED");
					OV_CASE_RETURN(Code::EGRESS_STREAM_DELETED, "EGRESS_STREAM_DELETED");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_OUTPUT_PROFILE, "EGRESS_STREAM_CREATION_FAILED_BY_OUTPUT_PROFILE");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_DECODER, "EGRESS_STREAM_CREATION_FAILED_BY_DECODER");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_ENCODER, "EGRESS_STREAM_CREATION_FAILED_BY_ENCODER");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_FILTER, "EGRESS_STREAM_CREATION_FAILED_BY_FILTER");
					OV_CASE_RETURN(Code::EGRESS_TRANSCODE_DECODING_FAILED, "EGRESS_TRANSCODE_DECODING_FAILED");
					OV_CASE_RETURN(Code::EGRESS_TRANSCODE_ENCODING_FAILED, "EGRESS_TRANSCODE_ENCODING_FAILED");
					OV_CASE_RETURN(Code::EGRESS_TRANSCODE_FILTERING_FAILED, "EGRESS_TRANSCODE_FILTERING_FAILED");

					OV_CASE_RETURN(Code::EGRESS_LLHLS_READY, "EGRESS_LLHLS_READY");
					OV_CASE_RETURN(Code::EGRESS_HLS_READY, "EGRESS_HLS_READY");

					OV_CASE_RETURN(Code::INTERNAL_QUEUE_CONGESTION, "INTERNAL_QUEUE_CONGESTION");
				}

				return "OK";
			}

			static ov::String DescriptionFromMessageCode(Code message_code)
			{
				return DescriptionFromMessageCode(message_code, 0, 0);
			}

			template <typename T>
			static ov::String DescriptionFromMessageCode(Code message_code, T config_value, T measured_value)
			{
				auto config	  = static_cast<T>(config_value);
				auto measured = static_cast<T>(measured_value);

				switch (message_code)
				{
					OV_CASE_RETURN(Code::OK, "The current status is good");

					OV_CASE_RETURN(Code::INGRESS_STREAM_CREATED, "A new ingress stream has been created");
					OV_CASE_RETURN(Code::INGRESS_STREAM_PREPARED, "A ingress stream has been prepared");
					OV_CASE_RETURN(Code::INGRESS_STREAM_DELETED, "A ingress stream has been deleted");
					OV_CASE_RETURN(Code::INGRESS_STREAM_CREATION_FAILED_DUPLICATE_NAME, "Failed to create stream because the specified stream name is already in use");

					OV_CASE_RETURN(Code::INGRESS_BITRATE_LOW,
								   ov::String::FormatString("The ingress stream's current bitrate (%d bps) is lower than the configured bitrate (%d bps)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_BITRATE_HIGH,
								   ov::String::FormatString("The ingress stream's current bitrate (%d bps) is higher than the configured bitrate (%d bps)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_FRAMERATE_LOW,
								   ov::String::FormatString("The ingress stream's current framerate (%.2f fps) is lower than the configured framerate (%.2f fps)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_FRAMERATE_HIGH,
								   ov::String::FormatString("The ingress stream's current framerate (%f fps) is higher than the configured framerate (%f fps)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_WIDTH_SMALL,
								   ov::String::FormatString("The ingress stream's width (%d) is smaller than the configured width (%d)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_WIDTH_LARGE,
								   ov::String::FormatString("The ingress stream's width (%d) is larger than the configured width (%d)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_HEIGHT_SMALL,
								   ov::String::FormatString("The ingress stream's height (%d) is smaller than the configured height (%d)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_HEIGHT_LARGE,
								   ov::String::FormatString("The ingress stream's height (%d) is larger than the configured height (%d)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_SAMPLERATE_LOW,
								   ov::String::FormatString("The ingress stream's current samplerate (%d) is lower than the configured samplerate (%d)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_SAMPLERATE_HIGH,
								   ov::String::FormatString("The ingress stream's current samplerate (%d) is higher than the configured samplerate (%d)", measured, config));
					OV_CASE_RETURN(Code::INGRESS_LONG_KEY_FRAME_INTERVAL,
								   ov::String::FormatString("The ingress stream's current keyframe interval (%.1f seconds) is too long. Please use a keyframe interval of %.1f seconds or less", measured, config));
					OV_CASE_RETURN(Code::INGRESS_HAS_BFRAME,
								   ov::String::FormatString("There are B-Frames in the ingress stream"));

					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATED, "A new egress stream has been created");
					OV_CASE_RETURN(Code::EGRESS_STREAM_PREPARED, "A egress stream has been prepared");
					OV_CASE_RETURN(Code::EGRESS_STREAM_DELETED, "A egress stream has been deleted");

					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_OUTPUT_PROFILE, "Failed to create egress stream because the output profile configuration is invalid");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_DECODER, "Failed to create egress stream because the decoder could not be created");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_ENCODER, "Failed to create egress stream because the encoder could not be created");
					OV_CASE_RETURN(Code::EGRESS_STREAM_CREATION_FAILED_BY_FILTER, "Failed to create egress stream because the filter could not be created");

					OV_CASE_RETURN(Code::EGRESS_TRANSCODE_DECODING_FAILED, "Failed to decode frames for the egress stream");
					OV_CASE_RETURN(Code::EGRESS_TRANSCODE_ENCODING_FAILED, "Failed to encode frames for the egress stream");
					OV_CASE_RETURN(Code::EGRESS_TRANSCODE_FILTERING_FAILED, "Failed to filter frames for the egress stream");

					OV_CASE_RETURN(Code::EGRESS_LLHLS_READY, "LLHLS stream is ready to play - initial segment(s) have been generated");
					OV_CASE_RETURN(Code::EGRESS_HLS_READY, "HLS stream is ready to play - initial segment(s) have been generated");

					OV_CASE_RETURN(Code::INTERNAL_QUEUE_CONGESTION,
								   ov::String::FormatString("Internal queue(s) is currently congested"));
				}

				return "The current status is good";
			}

		private:
			Code _code = Code::OK;
			ov::String _description;
		};
	}  // namespace alrt
}  // namespace mon