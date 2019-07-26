#pragma once

#include <base/ovlibrary/ovlibrary.h>

namespace common
{
	// 미디어 타입
	enum class MediaType : int8_t
	{
		Unknown = -1,
		Video,
		Audio,
		Data,
		Subtitle,
		Attachment,

		Nb
	};

	// 디코딩된 비디오 프레임의 타입
	enum class PictureType : uint8_t
	{
		None = 0, ///< Undefined
		I,     ///< Intra
		P,     ///< Predicted
		B,     ///< Bi-dir predicted
		S,     ///< S(GMC)-VOP MPEG4
		SI,    ///< Switching Intra
		SP,    ///< Switching Predicted
		BI,    ///< BI type
	};

	// 미디어 코덱 아이디
	enum class MediaCodecId : uint8_t
	{
		None = 0,
		H264,
		Vp8,
		Vp9,
		Flv,
		Aac,
		Mp3,
		Opus
	};


	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 타입베이스
	//////////////////////////////////////////////////////////////////////////////////////////////////

	class Timebase
	{
	public:
		Timebase() :
			Timebase(0, 0)
		{
		}

		Timebase(int32_t num, int32_t den) :
			_num(num), _den(den)
		{
		}

		Timebase &operator =(const Timebase &T) noexcept
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

		int32_t GetNum()
		{
			return _num;
		}

		int32_t GetDen()
		{
			return _den;
		}

		double GetExpr()
		{
			return (double)_num / (double)_den;
		}

		ov::String ToString()
		{
			return ov::String::FormatString("%d/%d", GetNum(), GetDen());
		}

	private:
		// 1/1000
		int32_t _num;
		int32_t _den;
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 오디오 샘플 포맷
	//////////////////////////////////////////////////////////////////////////////////////////////////

	// FFMEPG SampleFotmat과 호환성 있도록 값을 정의함
	class AudioSample
	{
	public:
		// 샘플 포맷
		enum class Format : int8_t
		{
			None = -1,

			U8 = 0,          ///< unsigned 8 bits
			S16 = 1,         ///< signed 16 bits
			S32 = 2,         ///< signed 32 bits
			Flt = 3,         ///< float
			Dbl = 4,         ///< double

			U8P = 5,         ///< unsigned 8 bits, planar
			S16P = 6,        ///< signed 16 bits, planar
			S32P = 7,        ///< signed 32 bits, planar
			FltP = 8,        ///< float, planar
			DblP = 9,        ///< double, planar

			Nb
		};

		// 샘플 레이트
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

		AudioSample &operator =(const AudioSample &T) noexcept
		{
			_sample_size = T._sample_size;
			_format = T._format;
			_name = T._name;
			_rate = T._rate;

			return *this;
		}

#define OV_MEDIA_TYPE_SET_VALUE(type, ...) \
    case type: \
        __VA_ARGS__; \
        break

		void SetFormat(Format fmt)
		{
			_format = fmt;

			switch(_format)
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

		int32_t GetSampleSize()
		{
			return _sample_size;
		}

		AudioSample::Format GetFormat()
		{
			return _format;
		}

		void SetRate(Rate rate)
		{
			_rate = rate;
		}

		Rate GetRate()
		{
			return _rate;
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
	// 오디오 레이아웃
	//////////////////////////////////////////////////////////////////////////////////////////////////
	class AudioChannel
	{
	public:
		enum class Layout : uint32_t
		{
			LayoutMono = 0x00000004U, // AV_CH_LAYOUT_MONO
			LayoutStereo = (0x00000001U | 0x00000002U) // AV_CH_FRONT_LEFT|AV_CH_FRONT_RIGHT

			// TODO(SOULK) : 추가적인 레이아웃을 지원해야함
		};

	public:
		AudioChannel() = default;
		~AudioChannel() = default;

		AudioChannel &operator =(const AudioChannel &audio_channel) noexcept
		{
			_layout = audio_channel._layout;
			_count = audio_channel._count;
			_name = audio_channel._name;

			return *this;
		}

		void SetLayout(Layout layout)
		{
			_layout = layout;

			switch(_layout)
			{
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutStereo, _count = 2, _name = "stereo");
				OV_MEDIA_TYPE_SET_VALUE(Layout::LayoutMono, _count = 1, _name = "mono");
			}
		}

		// 채널 레이아웃 반환
		AudioChannel::Layout GetLayout() const
		{
			return _layout;
		}

		// 채널 개수 반환
		uint32_t GetCounts() const
		{
			return _count;
		}

		// 채널 레이아웃 명
		const char *GetName() const
		{
			return _name.c_str();
		}

	private:
		Layout _layout = Layout::LayoutStereo;
		uint32_t _count = 2;
		std::string _name = "stereo";
	};


}