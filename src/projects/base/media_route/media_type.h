#pragma once

#include <base/ovlibrary/ovlibrary.h>

//TODO: common_type 헤더에 넣음
//TODO: common_video 헤더와 통합해야함.
namespace MediaCommonType
{
	// 미디어 타입
	enum class MediaType : int32_t
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
	enum class PictureType : int32_t
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
	enum class MediaCodecId : int32_t
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
		AudioSample() : AudioSample(Format::None)
		{
		}

		AudioSample(Format fmt)
		{
			SetFormat(fmt);
		}

		~AudioSample()
		{
		}

		AudioSample &operator =(const AudioSample &T) noexcept
		{
			_sample_size = T._sample_size;
			_format = T._format;
			_name = T._name;
			_rate = T._rate;

			return *this;
		}

		void SetFormat(Format fmt)
		{
			_format = fmt;

			switch((int8_t)_format)
			{
				case (int8_t)Format::U8:
					_name = "u8", _sample_size = 1;
					break;
				case (int8_t)Format::S16:
					_name = "s16", _sample_size = 2;
					break;
				case (int8_t)Format::S32:
					_name = "s32", _sample_size = 4;
					break;
				case (int8_t)Format::Flt:
					_name = "flt", _sample_size = 4;
					break;
				case (int8_t)Format::Dbl:
					_name = "dbl", _sample_size = 8;
					break;
				case (int8_t)Format::U8P:
					_name = "u8p", _sample_size = 1;
					break;
				case (int8_t)Format::S16P:
					_name = "s16p", _sample_size = 2;
					break;
				case (int8_t)Format::S32P:
					_name = "s32p", _sample_size = 4;
					break;
				case (int8_t)Format::FltP:
					_name = "fltp", _sample_size = 4;
					break;
				case (int8_t)Format::DblP:
					_name = "dblp", _sample_size = 8;
					break;
				case (int8_t)Format::None:
				default:
					_name = "none", _sample_size = 0;
					break;
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

		// void SetRate(int32_t rate)
		// {
		//     _rate = (Rate)rate;
		// }

		// int32_t GetRate()
		// {
		//     return (int32_t)_rate;
		// }

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

		// 샘플 레이트
		Rate _rate;

		// 샘플 포맷
		Format _format;

		// 샘플 사이즈
		int32_t _sample_size;

		// 샘플 포맷명
		std::string _name;
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
		AudioChannel() : _layout(Layout::LayoutStereo), _count(2), _name("")
		{
		}

		~AudioChannel()
		{
		}

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
				case Layout::LayoutStereo:
					_count = 2;
					_name = "stereo";
					break;

				case Layout::LayoutMono:
					_count = 1;
					_name = "mono";
					break;
			}
		}

		// 채널 레이아웃 반환
		AudioChannel::Layout GetLayout()
		{
			return _layout;
		}

		// 채널 개수 반환
		int32_t GetCounts()
		{
			return _count;
		}

		// 채널 레이아웃 명
		const char *GetName()
		{
			return _name.c_str();
		}

	private:
		Layout _layout;
		int32_t _count;
		std::string _name;
	};


}