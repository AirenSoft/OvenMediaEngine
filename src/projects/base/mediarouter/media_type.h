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
	enum class MediaRouterStreamType : int8_t
	{
		UNKNOWN = -1,
		INBOUND,
		OUTBOUND
	};

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
		Unknown = -1,
		H264_AVCC = 0,
		H264_ANNEXB,		// OME's default internal bitstream format for H264
		H264_RTP_RFC_6184,
		HVCC, // H.265 HVCC
		H265_ANNEXB,		// OME's default internal bitstream format for H265
		H265_RTP_RFC_7798,
		VP8,/*raw*/			// OME's default internal bitstream format for VP8
		VP8_RTP_RFC_7741,
		AAC_RAW,
		AAC_MPEG4_GENERIC,
		AAC_ADTS,			// OME's default internal bitstream format for AAC
		AAC_LATM,
		OPUS,/*raw*/		// OME's default internal bitstream format for OPUS
		OPUS_RTP_RFC_7587,
		MP3,
		JPEG,
		PNG,
		WEBP,

		// For Data Track
		ID3v2,
		OVEN_EVENT, // OvenMediaEngine defined event
		CUE,
		AMF, 				// AMF0
		SEI,				// H.264/H.265 SEI
		SCTE35				// SCTE35
	};

	enum class PacketType : int8_t
	{
		Unknown = -1,
		// This is a special purpose packet type, used by the ovt provider, 
		// and the "media router" delivers this type of packet to the publisher as it is without parsing.
		OVT = 0,	

		RAW, // AAC RAW, AAC ADTS, JPEG
		// H.264
		SEQUENCE_HEADER, // For H.264 AVCC, AAC RAW
		NALU, // For H.264 AVCC, ANNEXB	

		// For Data Track
		EVENT,
		VIDEO_EVENT,
		AUDIO_EVENT,
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
		Av1,
		Flv,
		Aac,
		Mp3,
		Opus,
		Jpeg,
		Png,
		Webp
	};

	enum class MediaCodecModuleId : uint8_t
	{
		None = 0,
		DEFAULT,	// SW
		OPENH264,	// SW
		BEAMR,		// SW
		X264,		// SW		
		NVENC,		// HW
		QSV,		// HW
		XMA,		// HW
		NILOGAN,	// HW
		LIBVPX,		// SW
		FDKAAC,		// SW
		LIBOPUS,	// SW
		NB
	};

	enum class KeyFrameIntervalType : uint8_t
	{
		FRAME = 0,
		TIME
	};

	static bool IsVideoCodec(cmn::MediaCodecId codec_id)
	{
		if (codec_id == cmn::MediaCodecId::H264 ||
			codec_id == cmn::MediaCodecId::H265 ||
			codec_id == cmn::MediaCodecId::Vp8 ||
			codec_id == cmn::MediaCodecId::Vp9 ||
			codec_id == cmn::MediaCodecId::Flv ||
			codec_id == cmn::MediaCodecId::Vp9)
		{
			return true;
		}

		return false;
	}

	static bool IsImageCodec(cmn::MediaCodecId codec_id)
	{
		if (codec_id == cmn::MediaCodecId::Jpeg ||
			codec_id == cmn::MediaCodecId::Png ||
			codec_id == cmn::MediaCodecId::Webp)
		{
			return true;
		}

		return false;
	}

	static bool IsAudioCodec(cmn::MediaCodecId codec_id)
	{
		if (codec_id == cmn::MediaCodecId::Aac ||
			codec_id == cmn::MediaCodecId::Mp3 ||
			codec_id == cmn::MediaCodecId::Opus)
		{
			return true;
		}

		return false;
	}

	constexpr const char *GetMediaPacketTypeString(cmn::PacketType packet_type)
	{
		switch (packet_type)
		{
			OV_CASE_RETURN_ENUM_STRING(PacketType, Unknown);
			OV_CASE_RETURN_ENUM_STRING(PacketType, OVT);
			OV_CASE_RETURN_ENUM_STRING(PacketType, RAW);
			OV_CASE_RETURN_ENUM_STRING(PacketType, SEQUENCE_HEADER);
			OV_CASE_RETURN_ENUM_STRING(PacketType, NALU);
			// Dimiden - Kept this value as it was returning `Unknown`
			// before refactoring due to not being handled in the case statement
			OV_CASE_RETURN(PacketType::EVENT, "Unknown");
			OV_CASE_RETURN_ENUM_STRING(PacketType, VIDEO_EVENT);
			OV_CASE_RETURN_ENUM_STRING(PacketType, AUDIO_EVENT);
		}

		return "Unknown";
	}

	constexpr const char *GetMediaTypeString(cmn::MediaType media_type)
	{
		switch (media_type)
		{
			OV_CASE_RETURN_ENUM_STRING(MediaType, Unknown);
			OV_CASE_RETURN_ENUM_STRING(MediaType, Video);
			OV_CASE_RETURN_ENUM_STRING(MediaType, Audio);
			OV_CASE_RETURN_ENUM_STRING(MediaType, Data);
			OV_CASE_RETURN_ENUM_STRING(MediaType, Subtitle);
			OV_CASE_RETURN_ENUM_STRING(MediaType, Attachment);
			// Dimiden - Kept this value as it was returning `Unknown`
			// before refactoring due to not being handled in the case statement
			OV_CASE_RETURN(MediaType::Nb, "Unknown");
		}

		return "Unknown";
	}

	constexpr const char *GetBitstreamFormatString(cmn::BitstreamFormat format)
	{
		switch (format)
		{
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, Unknown);
			OV_CASE_RETURN(BitstreamFormat::H264_AVCC, "AVCC");
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, H264_ANNEXB);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, H264_RTP_RFC_6184);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, H265_ANNEXB);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, H265_RTP_RFC_7798);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, HVCC);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, VP8);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, VP8_RTP_RFC_7741);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, AAC_RAW);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, AAC_MPEG4_GENERIC);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, AAC_ADTS);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, AAC_LATM);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, MP3);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, OPUS);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, OPUS_RTP_RFC_7587);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, JPEG);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, PNG);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, WEBP);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, ID3v2);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, OVEN_EVENT);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, CUE);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, AMF);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, SEI);
			OV_CASE_RETURN_ENUM_STRING(BitstreamFormat, SCTE35);
		}

		return "Unknown";
	}

	static cmn::MediaCodecModuleId GetCodecModuleIdByName(ov::String name)
	{
		name.MakeUpper();

		if (name.HasSuffix("_OPENH264") || name.HasSuffix("OPENH264"))
		{
			return cmn::MediaCodecModuleId::OPENH264;
		}
		else if (name.HasSuffix("_BEAMR") || name.HasSuffix("BEAMR"))
		{
			return cmn::MediaCodecModuleId::BEAMR;
		}
		else if (name.HasSuffix("_NVENC") || name.HasSuffix("_NV") || name.HasSuffix("NV") || name.HasSuffix("NVENC"))
		{
			return cmn::MediaCodecModuleId::NVENC;
		}
		else if (name.HasSuffix("_QSV") || name.HasSuffix("QSV"))
		{
			return cmn::MediaCodecModuleId::QSV;
		}
		else if (name.HasSuffix("_NILOGAN") || name.HasSuffix("NILOGAN"))
		{
			return cmn::MediaCodecModuleId::NILOGAN;
		}
		else if (name.HasSuffix("_XMA") || name.HasSuffix("XMA"))
		{
			return cmn::MediaCodecModuleId::XMA;
		}		
		else if (name.HasSuffix("_LIBVPX") || name.HasSuffix("LIBVPX"))
		{
			return cmn::MediaCodecModuleId::LIBVPX;
		}
		else if (name.HasSuffix("_FDKAAC") || name.HasSuffix("FDKAAC"))
		{
			return cmn::MediaCodecModuleId::FDKAAC;
		}
		else if (name.HasSuffix("_X264") || name.HasSuffix("X264") )
		{
			return cmn::MediaCodecModuleId::X264;
		}
		else if (name.HasSuffix("_DEFAULT") || name.HasSuffix("DEFAULT"))
		{
			return cmn::MediaCodecModuleId::DEFAULT;
		}

		return cmn::MediaCodecModuleId::None;
	}

	constexpr const char *GetStringFromCodecModuleId(MediaCodecModuleId id)
	{
		switch (id)
		{
			OV_CASE_RETURN(MediaCodecModuleId::DEFAULT, "default");
			OV_CASE_RETURN(MediaCodecModuleId::OPENH264, "openh264");
			OV_CASE_RETURN(MediaCodecModuleId::BEAMR, "beamr");
			OV_CASE_RETURN(MediaCodecModuleId::NVENC, "nvenc");
			OV_CASE_RETURN(MediaCodecModuleId::QSV, "qsv");
			OV_CASE_RETURN(MediaCodecModuleId::NILOGAN, "nilogan");
			OV_CASE_RETURN(MediaCodecModuleId::XMA, "xma";);
			OV_CASE_RETURN(MediaCodecModuleId::LIBVPX, "libvpx");
			OV_CASE_RETURN(MediaCodecModuleId::FDKAAC, "fdkaac");
			OV_CASE_RETURN(MediaCodecModuleId::LIBOPUS, "libopus");
			OV_CASE_RETURN(MediaCodecModuleId::X264, "x264");
			OV_CASE_RETURN(MediaCodecModuleId::None, "none");
			// Dimiden - Kept this value as it was returning `none`
			// before refactoring due to not being handled in the case statement
			OV_CASE_RETURN(MediaCodecModuleId::NB, "none");
		}

		return "none";
	}

	static bool IsSupportHWAccels(cmn::MediaCodecModuleId id)
	{
		switch(id)
		{
			case cmn::MediaCodecModuleId::NVENC:
			case cmn::MediaCodecModuleId::QSV:
			case cmn::MediaCodecModuleId::XMA:
			case cmn::MediaCodecModuleId::NILOGAN:
				return true;
			default:
				break;
		}

		return false;
	}

	constexpr const char *GetCodecIdToString(cmn::MediaCodecId id)
	{
		switch (id)
		{
			OV_CASE_RETURN_ENUM_STRING(MediaCodecId, None);
			// Video codecs
			OV_CASE_RETURN_ENUM_STRING(MediaCodecId, H264);
			OV_CASE_RETURN_ENUM_STRING(MediaCodecId, H265);
			OV_CASE_RETURN(MediaCodecId::Vp8, "VP8");
			OV_CASE_RETURN(MediaCodecId::Vp9, "VP9");
			OV_CASE_RETURN(MediaCodecId::Flv, "FLV");
			OV_CASE_RETURN(MediaCodecId::Av1, "AV1");
			// Image codecs
			OV_CASE_RETURN(MediaCodecId::Jpeg, "JPEG");
			OV_CASE_RETURN(MediaCodecId::Png, "PNG");
			OV_CASE_RETURN(MediaCodecId::Webp, "WEBP");
			// Audio codecs
			OV_CASE_RETURN(MediaCodecId::Aac, "AAC");
			OV_CASE_RETURN(MediaCodecId::Mp3, "MP3");
			OV_CASE_RETURN(MediaCodecId::Opus, "OPUS");
		}

		return "Unknown";
	}

	static cmn::MediaCodecId GetCodecIdByName(ov::String name)
	{
		name.MakeUpper();

		// Video codecs
		if (name.HasPrefix("H264"))
		{
			return cmn::MediaCodecId::H264;
		}
		else if (name.HasPrefix("H265"))
		{
			return cmn::MediaCodecId::H265;
		}
		else if (name.HasPrefix("VP8"))
		{
			return cmn::MediaCodecId::Vp8;
		}
		else if (name.HasPrefix("VP9"))
		{
			return cmn::MediaCodecId::Vp9;
		}
		else if (name.HasPrefix("FLV"))
		{
			return cmn::MediaCodecId::Flv;
		}
		// Image codecs
		else if (name.HasPrefix("JPEG"))
		{
			return cmn::MediaCodecId::Jpeg;
		}
		else if (name.HasPrefix("PNG"))
		{
			return cmn::MediaCodecId::Png;
		}
		else if (name.HasPrefix("WEBP"))
		{
			return cmn::MediaCodecId::Webp;
		}		
		// Audio codecs
		else if (name.HasPrefix("AAC"))
		{
			return cmn::MediaCodecId::Aac;
		}
		else if (name.HasPrefix("MP3"))
		{
			return cmn::MediaCodecId::Mp3;
		}
		else if (name.HasPrefix("OPUS"))
		{
			return cmn::MediaCodecId::Opus;
		}

		return cmn::MediaCodecId::None;
	}

	static ov::String GetKeyFrameIntervalTypeToString(cmn::KeyFrameIntervalType type)
	{
		switch (type)
		{
			case cmn::KeyFrameIntervalType::FRAME:
				return "frame";
			case cmn::KeyFrameIntervalType::TIME:
				return "time";
			default:
				return "unknown";
		}
	}

	static cmn::KeyFrameIntervalType GetKeyFrameIntervalTypeByName(ov::String type)
	{
		type.MakeLower();

		if (type == "frame")
		{
			return cmn::KeyFrameIntervalType::FRAME;
		}
		else if (type == "time")
		{
			return cmn::KeyFrameIntervalType::TIME;
		}

		return cmn::KeyFrameIntervalType::FRAME;
	}

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
			if (_num == 0)
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
			return GetStringExpr();
		}

	private:
		// 1/1000
		int32_t _num;
		int32_t _den;
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Audio sample format
	//////////////////////////////////////////////////////////////////////////////////////////////////

	// Defines values for compatibility with FFmpeg SampleFormat
	class AudioSample
	{
	public:
		// 샘플 포맷
		enum class Format : int8_t
		{
			None = -1,

			U8 = 0,	  ///< unsigned 8 bits
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
			return ov::ToUnderlyingType(_rate);
		}

		const char *GetName() const
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
		// Defines values for compatibility with FFmpeg ChannelName
		enum class Channel : uint32_t
		{
			None 				= 0x00000000U,
			FrontLeft 			= 0x00000001U,
			FrontRight 			= 0x00000002U,
			FrontCenter 		= 0x00000004U,
			LowFrequency 		= 0x00000008U,
			BackLeft 			= 0x00000010U,
			BackRight 			= 0x00000020U,
			FrontLeftOfCenter 	= 0x00000040U,
			FrontRightOfCenter 	= 0x00000080U,
			BackCenter 			= 0x00000100U,
			SideLeft 			= 0x00000200U,
			SideRight 			= 0x00000400U,
			TopCenter 			= 0x00000800U,
			TopFrontLeft 		= 0x00001000U,
			TopFrontCenter 		= 0x00002000U,
			TopFrontRight 		= 0x00004000U,
			TopBackLeft 		= 0x00008000U,
			TopBackCenter 		= 0x00010000U,
			TopBackRight 		= 0x00020000U,
			StereoLeft 			= 0x20000000U,
			StereoRight 		= 0x40000000U
		};

		// Defines values for compatibility with FFmpeg ChannelLayout
		enum class Layout : uint32_t
		{
			LayoutUnknown 			= (uint32_t)Channel::None,				
			LayoutMono 				= (uint32_t)Channel::FrontCenter,
			LayoutStereo 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight,
			Layout2Point1 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::LowFrequency,
			Layout21 				= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::BackCenter,
			LayoutSurround 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter,
			Layout3Point1  			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::LowFrequency,
			Layout4Point0  			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackCenter,
			Layout4Point1 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackCenter | (uint32_t)Channel::LowFrequency,
			Layout22 				= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight,
			LayoutQuad 				= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight,
			Layout5Point0 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight,
			Layout5Point1 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::LowFrequency,
			Layout5Point0Back 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight,
			Layout5Point1Back 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight | (uint32_t)Channel::LowFrequency,
			Layout6Point0 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::BackCenter,
			Layout6Point0Front 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::FrontLeftOfCenter | (uint32_t)Channel::FrontRightOfCenter,
			LayoutHexagonal 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight | (uint32_t)Channel::BackCenter,
			Layout6Point1 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::LowFrequency | (uint32_t)Channel::BackCenter,
			Layout6Point1Back 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight | (uint32_t)Channel::LowFrequency | (uint32_t)Channel::BackCenter,
			Layout6Point1Front 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::FrontLeftOfCenter | (uint32_t)Channel::FrontRightOfCenter | (uint32_t)Channel::LowFrequency,
			Layout7Point0 			= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight,
			Layout7Point0Front 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::FrontLeftOfCenter | (uint32_t)Channel::FrontRightOfCenter,
			Layout7Point1 		  	= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::LowFrequency | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight,
			Layout7Point1Wide 	  	= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::LowFrequency | (uint32_t)Channel::FrontLeftOfCenter | (uint32_t)Channel::FrontRightOfCenter,
			Layout7Point1WideBack	= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight | (uint32_t)Channel::LowFrequency | (uint32_t)Channel::FrontLeftOfCenter | (uint32_t)Channel::FrontRightOfCenter,
			LayoutOctagonal 		= (uint32_t)Channel::FrontLeft | (uint32_t)Channel::FrontRight | (uint32_t)Channel::FrontCenter | (uint32_t)Channel::SideLeft | (uint32_t)Channel::SideRight | (uint32_t)Channel::BackLeft | (uint32_t)Channel::BackRight | (uint32_t)Channel::BackCenter,
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
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutUnknown, 			_count = 0, _name = "unknown");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutMono, 			_count = 1, _name = "mono");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutStereo, 			_count = 2, _name = "stereo");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout2Point1, 			_count = 3, _name = "2.1");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout21, 				_count = 3, _name = "3.0(back)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutSurround, 		_count = 3, _name = "3.0");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout3Point1, 			_count = 4, _name = "3.1");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout4Point0, 			_count = 4, _name = "4.0");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout4Point1, 			_count = 5, _name = "4.1");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout22, 				_count = 4, _name = "quad(side)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutQuad, 			_count = 4, _name = "quad");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout5Point0, 			_count = 5, _name = "5.0(side)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout5Point1, 			_count = 6, _name = "5.1(side)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout5Point0Back, 		_count = 5, _name = "5.0");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout5Point1Back, 		_count = 6, _name = "5.1");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout6Point0, 			_count = 6, _name = "6.0");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout6Point0Front, 	_count = 6, _name = "6.0(front)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutHexagonal, 		_count = 6, _name = "hexagonal");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout6Point1, 			_count = 7, _name = "6.1");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout6Point1Back, 		_count = 7, _name = "6.1(back)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout6Point1Front, 	_count = 7, _name = "6.1(front)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout7Point0, 			_count = 7, _name = "7.0");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout7Point0Front, 	_count = 7, _name = "7.0(front)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout7Point1, 			_count = 8, _name = "7.1");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout7Point1Wide, 		_count = 8, _name = "7.1(wide-side)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::Layout7Point1WideBack, 	_count = 8, _name = "7.1(wide)");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutOctagonal, 		_count = 8, _name = "octagonal");
			}
		}

		// If it is set as channel count, it is used as the default layout.
		void SetCount(uint32_t count)
		{
			_count = count;
			switch (_count)
			{
				OV_MEDIA_TYPE_SET_VALUE(0, _layout = Layout::LayoutUnknown, _name = "unknown");
				OV_MEDIA_TYPE_SET_VALUE(1, _layout = Layout::LayoutMono, _name = "mono");
				OV_MEDIA_TYPE_SET_VALUE(2, _layout = Layout::LayoutStereo, _name = "stereo");
				OV_MEDIA_TYPE_SET_VALUE(3, _layout = Layout::Layout2Point1, _name = "2.1");
				OV_MEDIA_TYPE_SET_VALUE(4, _layout = Layout::Layout4Point0, _name = "4.0");
				OV_MEDIA_TYPE_SET_VALUE(5, _layout = Layout::Layout5Point0Back, _name = "5.0");
				OV_MEDIA_TYPE_SET_VALUE(6, _layout = Layout::Layout5Point1Back, _name = "5.1");
				OV_MEDIA_TYPE_SET_VALUE(7, _layout = Layout::Layout6Point1, _name = "6.1");
				OV_MEDIA_TYPE_SET_VALUE(8, _layout = Layout::Layout7Point1, _name = "7.1");
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
			return _name.c_str();
		}

	private:
		Layout _layout = Layout::LayoutStereo;
		uint32_t _count = 2;
		std::string _name = "stereo";
	};

}  // namespace cmn