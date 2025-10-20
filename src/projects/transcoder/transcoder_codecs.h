//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/mediarouter/media_type.h>
#include <base/ovlibrary/ovlibrary.h>

namespace tc
{
	class CodecMetrics
	{
	public:
		CodecMetrics()
		{
			Reset();
		}
		~CodecMetrics() = default;

		void Reset()
		{
			_active_decoder = 0;
			_active_encoder = 0;
			_load_decoder	= 0;
			_load_encoder	= 0;
		}

		void IncreaseActiveDecoder()
		{
			_active_decoder++;
		}
		void DecreaseActiveDecoder()
		{
			if (_active_decoder > 0)
				_active_decoder--;
		}

		void IncreaseActiveEncoder()
		{
			_active_encoder++;
		}
		void DecreaseActiveEncoder()
		{
			if (_active_encoder > 0)
				_active_encoder--;
		}

		void SetLoadDecoder(double load)
		{
			_load_decoder = load;
		}

		void SetLoadEncoder(double load)
		{
			_load_encoder = load;
		}

	public:
		int32_t _active_decoder;
		int32_t _active_encoder;
		double _load_decoder;
		double _load_encoder;
	};

	class CodecInfo
	{
	public:
		CodecInfo() = delete;
		CodecInfo(const ov::String &display_name, cmn::MediaType media_type, cmn::MediaCodecModuleId module_id, cmn::DeviceId device_id, const ov::String &bus_id, const std::vector<cmn::MediaCodecId> &supported_codecs, bool is_decoder, bool is_encoder, bool is_hwaccel)
			: _display_name(display_name),
			  _media_type(media_type),
			  _module_id(module_id),
			  _device_id(device_id),
			  _bus_id(bus_id),
			  _supported_codecs(supported_codecs),
			  _is_encoder(is_encoder),
			  _is_decoder(is_decoder),
			  _is_hwaccel(is_hwaccel)
		{
		}
		~CodecInfo() = default;

		ov::String GetName() const
		{
			return ov::String::FormatString("%s:%d", cmn::GetCodecModuleIdString(_module_id), _device_id);
		}

		ov::String GetBusId() const
		{
			return _bus_id;
		}

		ov::String GetDisplayName() const
		{
			return _display_name;
		}	

		cmn::MediaCodecModuleId GetModuleId() const
		{
			return _module_id;
		}

		cmn::MediaType GetMediaType() const
		{
			return _media_type;
		}

		cmn::DeviceId GetDeviceId() const
		{
			return _device_id;
		}

		CodecMetrics& GetMetrics() 
		{
			return _metrics;
		}

		std::vector<cmn::MediaCodecId> GetSupportedCodecs() const
		{
			return _supported_codecs;
		}

		bool isEncoder() const
		{
			return _is_encoder;
		}
		
		bool isDecoder() const
		{
			return _is_decoder;
		}

		bool isHwAccel() const
		{
			return _is_hwaccel;
		}
		
		ov::String GetInfoString() const;

	private:
		ov::String _display_name;
		cmn::MediaType _media_type;
		cmn::MediaCodecModuleId _module_id;
		cmn::DeviceId _device_id;
		ov::String _bus_id;
		std::vector<cmn::MediaCodecId> _supported_codecs;
		CodecMetrics _metrics;

		bool _is_encoder;
		bool _is_decoder;
		bool _is_hwaccel;
	};

	class TranscodeCodecs : public ov::Singleton<TranscodeCodecs>
	{
	public:
		TranscodeCodecs();
		~TranscodeCodecs() = default;

		void Register(const CodecInfo &codec_info)
		{
			_codecs.push_back(std::make_shared<CodecInfo>(codec_info));
		}

		const std::vector<std::shared_ptr<CodecInfo>> &GetCodecs() const
		{
			return _codecs;
		}

		std::shared_ptr<CodecInfo> GetCodec(bool coder_type, cmn::MediaCodecId codec_id, cmn::MediaCodecModuleId module_id, cmn::DeviceId device_id) const
		{
			auto media_type = cmn::IsVideoCodec(codec_id) ? cmn::MediaType::Video : cmn::IsImageCodec(codec_id) ? cmn::MediaType::Video
																												: cmn::MediaType::Audio;

			for (const auto &codec : _codecs)
			{
				if (codec->GetMediaType() == media_type &&
					codec->GetModuleId() == module_id &&
					codec->GetDeviceId() == device_id &&
					(coder_type ? codec->isEncoder() : codec->isDecoder()) &&
					std::find(codec->GetSupportedCodecs().begin(), codec->GetSupportedCodecs().end(), codec_id) != codec->GetSupportedCodecs().end())
				{
					return codec;
				}
			}

			return nullptr;
		}

		// coder_type: true - encoder, false - decoder
		bool Activate(bool coder_type, cmn::MediaCodecId codec_id, cmn::MediaCodecModuleId module_id, cmn::DeviceId device_id);
		bool Deactivate(bool coder_type, cmn::MediaCodecId codec_id, cmn::MediaCodecModuleId module_id, cmn::DeviceId device_id);

		ov::String GetInfoString() const;

	private:
		std::vector<std::shared_ptr<CodecInfo>> _codecs;
	};
}  // namespace tc
