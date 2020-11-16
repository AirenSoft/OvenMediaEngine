//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

namespace cmn
{
	enum class MediaType : int8_t
	{
		Unknown = -1,
		Video = 0,
		Audio,
		Data,
		Subtitle,
		Attachment,
		Nb
	};

	enum class BitstreamFormat : int8_t
	{
		Unknwon = -1,
		H264_AVCC = 0,
		H264_ANNEXB,
		H265_ANNEXB,
		VP8,
		AAC_LATM,
		AAC_ADTS,
		OPUS,
		JPEG,
		PNG
	};

	enum class PacketType : int8_t
	{
		Unknwon = -1,
		// This is a special purpose packet type, used by the ovt provider, 
		// and the "media router" delivers this type of packet to the publisher as it is without parsing.
		OVT = 0,	

		RAW, // AAC LATM, AAC ADTS, JPEG
		// H.264
		SEQUENCE_HEADER, // For H.264 AVCC, AAC LATM
		NALU, // For H.264 AVCC, ANNEXB	
	};

	enum class PictureType : uint8_t
	{
		None = 0,  ///< Undefined
		I,		   ///< Intra
		P,		   ///< Predicted
		B,		   ///< Bi-dir predicted
		S,		   ///< S(GMC)-VOP MPEG4
		SI,		   ///< Switching Intra
		SP,		   ///< Switching Predicted
		BI,		   ///< BI type
	};

	enum class MediaCodecId : uint8_t
	{
		None = 0,
		H264,
		H265,
		Vp8,
		Vp9,
		Flv,
		Aac,
		Mp3,
		Opus,
		Jpeg,
		Png
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Timebase
	//////////////////////////////////////////////////////////////////////////////////////////////////

	class Timebase
	{
	public:
		Timebase()
			: Timebase(0, 0)
		{
		}

		Timebase(int32_t num, int32_t den)
			: _num(num), _den(den)
		{
		}

		Timebase &operator=(const Timebase &T) noexcept
		{
			_num = T._num;
			_den = T._den;

			return *this;
		}

		void Set(int32_t num, int32_t den)
		{
			SetNum(num);
			SetDen(den);
		}

		void SetNum(int32_t num)
		{
			_num = num;
		}

		void SetDen(int32_t den)
		{
			_den = den;
		}

		int32_t GetNum() const
		{
			return _num;
		}

		int32_t GetDen() const
		{
			return _den;
		}

		double GetExpr() const
		{
			if (_den == 0)
			{
				return 0.0;
			}

			return (double)_num / (double)_den;
		}

		double GetTimescale() const
		{
			if(_num == 0)
			{
				return 0.0;
			}
			
			return (double)_den / (double)_num;
		}

		bool operator==(const Timebase &timebase) const
		{
			return (timebase._num == _num) && (timebase._den == _den);
		}

		bool operator!=(const Timebase &timebase) const
		{
			return operator==(timebase) == false;
		}

		ov::String GetStringExpr() const
		{
			return ov::String::FormatString("%d/%d", GetNum(), GetDen());
		}

		ov::String ToString() const
		{
			return std::move(GetStringExpr());
		}

	private:
		// 1/1000
		int32_t _num;
		int32_t _den;
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Audio sample format
	//////////////////////////////////////////////////////////////////////////////////////////////////

	// Defines values for compatibility with FFMPEG SampleFormat
	class AudioSample
	{
	public:
		// 샘플 포맷
		enum class Format : int8_t
		{
			None = -1,

			U8 = 0,   ///< unsigned 8 bits
			S16 = 1,  ///< signed 16 bits
			S32 = 2,  ///< signed 32 bits
			Flt = 3,  ///< float
			Dbl = 4,  ///< double

			U8P = 5,   ///< unsigned 8 bits, planar
			S16P = 6,  ///< signed 16 bits, planar
			S32P = 7,  ///< signed 32 bits, planar
			FltP = 8,  ///< float, planar
			DblP = 9,  ///< double, planar

			Nb
		};

		// Samplerates
		enum class Rate : int32_t
		{
			None = 0,
			R8000 = 8000,
			R11025 = 11025,
			R16000 = 16000,
			R22050 = 22050,
			R32000 = 32000,
			R44056 = 44056,
			R44100 = 44100,
			R47250 = 47250,
			R48000 = 48000,
			R50000 = 50000,
			R50400 = 50400,
			R88200 = 88200,
			R96000 = 96000,
			R176400 = 176400,
			R192000 = 192000,
			R352800 = 352800,
			R2822400 = 2822400,
			R5644800 = 5644800,

			Nb
		};

	public:
		AudioSample() = default;

		explicit AudioSample(Format fmt)
		{
			SetFormat(fmt);
		}

		~AudioSample() = default;

		AudioSample &operator=(const AudioSample &T) noexcept
		{
			_sample_size = T._sample_size;
			_format = T._format;
			_name = T._name;
			_rate = T._rate;

			return *this;
		}

#define OV_MEDIA_TYPE_SET_VALUE(type, ...) \
	case type:                             \
		__VA_ARGS__;                       \
		break

		void SetFormat(Format fmt)
		{
			_format = fmt;

			switch (_format)
			{
				OV_MEDIA_TYPE_SET_VALUE(Format::U8, _name = "u8", _sample_size = 1);
				OV_MEDIA_TYPE_SET_VALUE(Format::S16, _name = "s16", _sample_size = 2);
				OV_MEDIA_TYPE_SET_VALUE(Format::S32, _name = "s32", _sample_size = 4);
				OV_MEDIA_TYPE_SET_VALUE(Format::Flt, _name = "flt", _sample_size = 4);
				OV_MEDIA_TYPE_SET_VALUE(Format::Dbl, _name = "dbl", _sample_size = 8);
				OV_MEDIA_TYPE_SET_VALUE(Format::U8P, _name = "u8p", _sample_size = 1);
				OV_MEDIA_TYPE_SET_VALUE(Format::S16P, _name = "s16p", _sample_size = 2);
				OV_MEDIA_TYPE_SET_VALUE(Format::S32P, _name = "s32p", _sample_size = 4);
				OV_MEDIA_TYPE_SET_VALUE(Format::FltP, _name = "fltp", _sample_size = 4);
				OV_MEDIA_TYPE_SET_VALUE(Format::DblP, _name = "dblp", _sample_size = 8);

				default:
					OV_MEDIA_TYPE_SET_VALUE(Format::None, _name = "none", _sample_size = 0);
			}
		}

		// Unit: Bytes
		int32_t GetSampleSize() const
		{
			return _sample_size;
		}
		
		AudioSample::Format GetFormat() const
		{
			return _format;
		}

		void SetRate(Rate rate)
		{
			_rate = rate;
		}

		Rate GetRate() const
		{
			return _rate;
		}

		int32_t GetRateNum() const
		{
			return static_cast<std::underlying_type<AudioSample::Rate>::type>(_rate);
		}

		const char *GetName()
		{
			return _name.c_str();
		}

		// Sample rate
		Rate _rate = Rate::None;

		// Sample format
		Format _format = Format::None;

		// Sample size
		int32_t _sample_size = 0;

		// Sample format name
		std::string _name = "none";
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Audio layout
	//////////////////////////////////////////////////////////////////////////////////////////////////
	class AudioChannel
	{
	public:
		enum class Layout : uint32_t
		{
			LayoutUnknown = 0x00000000U,				// AV_CH_LAYOUT_Unknown
			LayoutMono = 0x00000004U,					// AV_CH_LAYOUT_MONO
			LayoutStereo = (0x00000001U | 0x00000002U)  // AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT

			// TODO(SOULK) : Need to support additional layout
		};

	public:
		AudioChannel() = default;
		~AudioChannel() = default;

		AudioChannel &operator=(const AudioChannel &audio_channel) noexcept
		{
			_layout = audio_channel._layout;
			_count = audio_channel._count;
			_name = audio_channel._name;

			return *this;
		}

		void SetLayout(Layout layout)
		{
			_layout = layout;

			switch (_layout)
			{
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutUnknown, _count = 0, _name = "unknown");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutStereo, _count = 2, _name = "stereo");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutMono, _count = 1, _name = "mono");
			}
		}

		// channel layout
		AudioChannel::Layout GetLayout() const
		{
			return _layout;
		}

		// channel count
		uint32_t GetCounts() const
		{
			return _count;
		}

		// the name of channel layout
		const char *GetName() const
		{
			switch (_layout)
			{
				case Layout::LayoutUnknown:
					[[fallthrough]];
				default:
					return "unknown";

				case Layout::LayoutStereo:
					return "stereo";

				case Layout::LayoutMono:
					return "mono";
			}
		}

	private:
		Layout _layout = Layout::LayoutStereo;
		uint32_t _count = 2;
		std::string _name = "stereo";
	};

}  // namespace cmn