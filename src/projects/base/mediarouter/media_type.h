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
		Unknown = -1,
		H264_AVCC = 0,
		H264_ANNEXB,		// OME's default internal bitstream format for H264
		H264_RTP_RFC_6184,
		H265_ANNEXB,		// OME's default internal bitstream format for H265
		VP8,/*raw*/			// OME's default internal bitstream format for VP8
		VP8_RTP_RFC_7741,
		AAC_RAW,
		AAC_MPEG4_GENERIC,
		AAC_ADTS,			// OME's default internal bitstream format for AAC
		AAC_LATM,
		OPUS,/*raw*/		// OME's default internal bitstream format for OPUS
		OPUS_RTP_RFC_7587,
		JPEG,
		PNG,

		// For Data Track
		ID3v2,
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
		Flv,
		Aac,
		Mp3,
		Opus,
		Jpeg,
		Png,
	};

	enum class MediaCodecLibraryId : uint8_t
	{
		AUTO,
		OPENH264,
		BEAMR,
		NVENC,
		QSV,
		LIBVPX,
		FDKAAC,
		LIBOPUS,
		NB
	};

	static bool IsVideoCodec(cmn::MediaCodecId codec_id)
	{
		if (codec_id == cmn::MediaCodecId::H264 || codec_id == cmn::MediaCodecId::H265 || codec_id == cmn::MediaCodecId::Vp8 || codec_id == cmn::MediaCodecId::Flv ||
			codec_id == cmn::MediaCodecId::Vp9 || codec_id == cmn::MediaCodecId::Jpeg || codec_id == cmn::MediaCodecId::Png)
		{
			return true;
		}

		return false;
	}

	static bool IsImageCodec(cmn::MediaCodecId codec_id) {
		if (codec_id == cmn::MediaCodecId::Jpeg || 
		    codec_id == cmn::MediaCodecId::Png)
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

	static ov::String GetMeiaPacketTypeString(cmn::PacketType packet_type)
	{
		switch (packet_type)
		{
			case cmn::PacketType::OVT:
				return "OVT";
			case cmn::PacketType::RAW:
				return "RAW";
			case cmn::PacketType::SEQUENCE_HEADER:
				return "SEQUENCE_HEADER";
			case cmn::PacketType::NALU:
				return "NALU";
			case cmn::PacketType::VIDEO_EVENT:
				return "VIDEO_EVENT";
			case cmn::PacketType::AUDIO_EVENT:
				return "AUDIO_EVENT";
			default:
				return "Unknown";
		}
	}

	static ov::String GetMediaTypeString(cmn::MediaType media_type)
	{
		switch (media_type)
		{
			case cmn::MediaType::Video:
				return "Video";
			case cmn::MediaType::Audio:
				return "Audio";
			case cmn::MediaType::Data:
				return "Data";
			case cmn::MediaType::Subtitle:
				return "Subtitle";
			case cmn::MediaType::Attachment:
				return "Attachment";
			default:
				return "Unknown";
		}
	}

	static ov::String GetBitstreamFormatString(cmn::BitstreamFormat format) {
		switch (format) {
			case cmn::BitstreamFormat::H264_AVCC:
				return "H264_AVCC";
			case cmn::BitstreamFormat::H264_ANNEXB:
				return "H264_ANNEXB";
			case cmn::BitstreamFormat::H264_RTP_RFC_6184:
				return "H264_RTP_RFC_6184";
			case cmn::BitstreamFormat::H265_ANNEXB:
				return "H265_ANNEXB";
			case cmn::BitstreamFormat::VP8:
				return "VP8";
			case cmn::BitstreamFormat::VP8_RTP_RFC_7741:
				return "VP8_RTP_RFC_7741";
			case cmn::BitstreamFormat::AAC_RAW:
				return "AAC_RAW";
			case cmn::BitstreamFormat::AAC_MPEG4_GENERIC:
				return "AAC_MPEG4_GENERIC";
			case cmn::BitstreamFormat::AAC_ADTS:
				return "AAC_ADTS";
			case cmn::BitstreamFormat::AAC_LATM:
				return "AAC_LATM";
			case cmn::BitstreamFormat::OPUS:
				return "OPUS";
			case cmn::BitstreamFormat::OPUS_RTP_RFC_7587:
				return "OPUS_RTP_RFC_7587";
			case cmn::BitstreamFormat::JPEG:
				return "JPEG";
			case cmn::BitstreamFormat::PNG:
				return "PNG";
			case cmn::BitstreamFormat::ID3v2:
				return "ID3v2";
			default:
				return "Unknown";
		}
	}

	static cmn::MediaCodecLibraryId GetCodecLibraryIdByName(ov::String name)
	{
		name.MakeUpper();

		if (name.HasSuffix("_OPENH264"))
		{
			return cmn::MediaCodecLibraryId::OPENH264;
		}
		else if (name.HasSuffix("_BEAMR"))
		{
			return cmn::MediaCodecLibraryId::BEAMR;
		}
		else if (name.HasSuffix("_NVENC"))
		{
			return cmn::MediaCodecLibraryId::NVENC;
		}
		else if (name.HasSuffix("_QSV"))
		{
			return cmn::MediaCodecLibraryId::QSV;
		}
		else if (name.HasSuffix("_LIBVPX"))
		{
			return cmn::MediaCodecLibraryId::LIBVPX;
		}
		else if (name.HasSuffix("_FDKAAC"))
		{
			return cmn::MediaCodecLibraryId::FDKAAC;
		}

		return cmn::MediaCodecLibraryId::AUTO;
	}

	static ov::String GetStringFromCodecLibraryId(cmn::MediaCodecLibraryId id)
	{
		switch (id)
		{
			case cmn::MediaCodecLibraryId::OPENH264:
				return "OpenH264";
			case cmn::MediaCodecLibraryId::BEAMR:
				return "Beamr";
			case cmn::MediaCodecLibraryId::NVENC:
				return "nvenc";
			case cmn::MediaCodecLibraryId::QSV:
				return "qsv";
			case cmn::MediaCodecLibraryId::LIBVPX:
				return "libvpx";
			case cmn::MediaCodecLibraryId::FDKAAC:
				return "fdkaac";
			case cmn::MediaCodecLibraryId::LIBOPUS:
				return "libopus";				
			case cmn::MediaCodecLibraryId::AUTO:
			default:
				break;
		}

		return "Auto";
	}

	static ov::String GetStringFromCodecId(cmn::MediaCodecId id)
	{
		switch (id)
		{
			case cmn::MediaCodecId::H264:
				return "H264";
			case cmn::MediaCodecId::H265:
				return "H265";
			case cmn::MediaCodecId::Vp8:
				return "VP8";
			case cmn::MediaCodecId::Vp9:
				return "VP9";
			case cmn::MediaCodecId::Aac:
				return "AAC";
			case cmn::MediaCodecId::Opus:
				return "OPUS";
			case cmn::MediaCodecId::Jpeg:
				return "JPEG";
			case cmn::MediaCodecId::Png:
				return "PNG";
			default:
				break;
		}

		return "Unknown";
	}

	static cmn::MediaCodecId GetCodecIdByName(ov::String name)
	{
		name.MakeUpper();

		// Video codecs
		if (name == "H264" || name == "H264_OPENH264" || name == "H264_BEAMR" || name == "H264_NVENC" || name == "H264_QSV")
		{
			return cmn::MediaCodecId::H264;
		}
		else if (name == "H265" || name == "H265_NVENC" || name == "H265_QSV")
		{
			return cmn::MediaCodecId::H265;
		}
		else if (name == "VP8")
		{
			return cmn::MediaCodecId::Vp8;
		}
		else if (name == "VP9")
		{
			return cmn::MediaCodecId::Vp9;
		}
		else if (name == "FLV")
		{
			return cmn::MediaCodecId::Flv;
		}
		else if (name == "JPEG")
		{
			return cmn::MediaCodecId::Jpeg;
		}
		else if (name == "PNG")
		{
			return cmn::MediaCodecId::Png;
		}

		// Audio codecs
		if (name == "AAC")
		{
			return cmn::MediaCodecId::Aac;
		}
		else if (name == "MP3")
		{
			return cmn::MediaCodecId::Mp3;
		}
		else if (name == "OPUS")
		{
			return cmn::MediaCodecId::Opus;
		}

		return cmn::MediaCodecId::None;
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

	// Defines values for compatibility with FFMPEG SampleFormat
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
		enum class Layout : uint32_t
		{
			LayoutUnknown = 0x00000000U,				// AV_CH_LAYOUT_Unknown
			LayoutMono = 0x00000004U,					// AV_CH_LAYOUT_MONO
			LayoutStereo = (0x00000001U | 0x00000002U)	// AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT
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
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutMono, _count = 1, _name = "mono");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutStereo, _count = 2, _name = "stereo");
			}
		}

		void SetCount(uint32_t count)
		{
			_count = count;
			switch (_count)
			{
				OV_MEDIA_TYPE_SET_VALUE(0, _layout = Layout::LayoutUnknown, _name = "unknown");
				OV_MEDIA_TYPE_SET_VALUE(1, _layout = Layout::LayoutMono, _name = "mono");
				OV_MEDIA_TYPE_SET_VALUE(2, _layout = Layout::LayoutStereo, _name = "stereo");
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