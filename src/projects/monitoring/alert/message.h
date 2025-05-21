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
			enum class Code : uint16_t
			{
				OK,

				// Ingress Codes
				INGRESS_BITRATE_LOW,
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

				// Internal Codes
				INTERNAL_QUEUE_CONGESTION
			};

			static std::shared_ptr<Message> CreateMessage(Code code, const ov::String &description)
			{
				auto message = std::make_shared<Message>();

				message->_code = code;
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

					OV_CASE_RETURN(Code::INTERNAL_QUEUE_CONGESTION, "INTERNAL_QUEUE_CONGESTION");
				}

				return "OK";
			}

			template <typename T>
			static ov::String DescriptionFromMessageCode(Code message_code, T config_value, T measured_value)
			{
				auto config = static_cast<T>(config_value);
				auto measured = static_cast<T>(measured_value);

				switch (message_code)
				{
					OV_CASE_RETURN(Code::OK, "The current status is good");

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