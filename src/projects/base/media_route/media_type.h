#pragma once

//TODO: common_type 헤더에 넣음
//TODO: common_video 헤더와 통합해야함.
namespace MediaCommonType 
{
    // 미디어 타입
    enum class MediaType : int32_t {
        MEDIA_TYPE_UNKNOWN = -1, 
        MEDIA_TYPE_VIDEO,
        MEDIA_TYPE_AUDIO,
        MEDIA_TYPE_DATA,          
        MEDIA_TYPE_SUBTITLE,
        MEDIA_TYPE_ATTACHMENT,   
        MEDIA_TYPE_NB
    };

    // 디코딩된 비디오 프레임의 타입
    enum class PictureType : int32_t {
        PICTURE_TYPE_NONE = 0, ///< Undefined
        PICTURE_TYPE_I,     ///< Intra
        PICTURE_TYPE_P,     ///< Predicted
        PICTURE_TYPE_B,     ///< Bi-dir predicted
        PICTURE_TYPE_S,     ///< S(GMC)-VOP MPEG4
        PICTURE_TYPE_SI,    ///< Switching Intra
        PICTURE_TYPE_SP,    ///< Switching Predicted
        PICTURE_TYPE_BI,    ///< BI type
    };

     // 미디어 코덱 아이디
    enum class MediaCodecId : int32_t {
        CODEC_ID_NONE = 0,
        CODEC_ID_H264,
        CODEC_ID_VP8,
        CODEC_ID_VP9,
        CODEC_ID_FLV,
        CODEC_ID_AAC,
        CODEC_ID_MP3,
        CODEC_ID_OPUS
    };


    //////////////////////////////////////////////////////////////////////////////////////////////////
    // 타입베이스
    //////////////////////////////////////////////////////////////////////////////////////////////////
    
    class Timebase {
        public:
            Timebase() : 
                Timebase(0, 0) {}
            Timebase(int32_t num, int32_t den) : 
                _num(num), _den(den) {}

            Timebase &operator =(const Timebase &T) noexcept
            {
                _num = T._num;
                _den = T._den;

                 return *this;
            }

            void Set(int32_t num, int32_t den) {
                SetNum(num); SetDen(den);
            }
            void SetNum(int32_t num) {
                _num = num;
            }
            void SetDen(int32_t den) {
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
            
            // 1/1000
            int32_t    _num;
            int32_t    _den;
    };



    //////////////////////////////////////////////////////////////////////////////////////////////////
    // 오디오 샘플 포맷
    //////////////////////////////////////////////////////////////////////////////////////////////////

    // FFMEPG SampleFotmat과 호환성 있도록 값을 정의함
    class AudioSample {
        public:
            // 샘플 포맷
            enum class Format : int8_t 
            {
                SAMPLE_FMT_NONE = -1,

                SAMPLE_FMT_U8 = 0,          ///< unsigned 8 bits
                SAMPLE_FMT_S16 = 1,         ///< signed 16 bits
                SAMPLE_FMT_S32 = 2,         ///< signed 32 bits
                SAMPLE_FMT_FLT = 3,         ///< float
                SAMPLE_FMT_DBL = 4,         ///< double

                SAMPLE_FMT_U8P = 5,         ///< unsigned 8 bits, planar
                SAMPLE_FMT_S16P = 6,        ///< signed 16 bits, planar
                SAMPLE_FMT_S32P = 7,        ///< signed 32 bits, planar
                SAMPLE_FMT_FLTP = 8,        ///< float, planar
                SAMPLE_FMT_DBLP = 9,        ///< double, planar

                SAMPLE_FMT_NB           
            };

            // 샘플 레이트
            enum class Rate : int32_t
            {
                SAMPLE_RATE_NONE = 0,
                SAMPLE_RATE_8000 = 8000,
                SAMPLE_RATE_11025 = 11025,
                SAMPLE_RATE_16000 = 16000,
                SAMPLE_RATE_22050 = 22050,
                SAMPLE_RATE_32000 = 32000,
                SAMPLE_RATE_44056 = 44056,
                SAMPLE_RATE_44100 = 44100,
                SAMPLE_RATE_47250 = 47250,
                SAMPLE_RATE_48000 = 48000,
                SAMPLE_RATE_50000 = 50000,
                SAMPLE_RATE_50400 = 50400,
                SAMPLE_RATE_88200 = 88200,
                SAMPLE_RATE_96000 = 96000,
                SAMPLE_RATE_176400 = 176400,
                SAMPLE_RATE_192000 = 192000,
                SAMPLE_RATE_352800 = 352800,
                SAMPLE_RATE_2822400 = 2822400,
                SAMPLE_RATE_5644800 = 5644800,

                SAMPLE_RATE_NB
            };

        public:
            AudioSample() : AudioSample(Format::SAMPLE_FMT_NONE) {}
            AudioSample(Format fmt){
                SetFormat(fmt);
            }
            ~AudioSample() {}

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
                    case (int8_t)Format::SAMPLE_FMT_U8: _name = "u8", _sample_size = 1; break;
                    case (int8_t)Format::SAMPLE_FMT_S16: _name = "s16", _sample_size = 2; break;
                    case (int8_t)Format::SAMPLE_FMT_S32: _name = "s32", _sample_size = 4; break;
                    case (int8_t)Format::SAMPLE_FMT_FLT: _name = "flt", _sample_size = 4; break;
                    case (int8_t)Format::SAMPLE_FMT_DBL: _name = "dbl", _sample_size = 8; break;
                    case (int8_t)Format::SAMPLE_FMT_U8P: _name = "u8p", _sample_size = 1; break;
                    case (int8_t)Format::SAMPLE_FMT_S16P: _name = "s16p", _sample_size = 2; break;
                    case (int8_t)Format::SAMPLE_FMT_S32P: _name = "s32p", _sample_size = 4; break;
                    case (int8_t)Format::SAMPLE_FMT_FLTP: _name = "fltp", _sample_size = 4; break;
                    case (int8_t)Format::SAMPLE_FMT_DBLP: _name = "dblp", _sample_size = 8; break;
                    case (int8_t)Format::SAMPLE_FMT_NONE:
                    default:
                        _name = "none", _sample_size = 0; break;
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

            const char* GetName()
            {
                return _name.c_str();
            }

            // 샘플 레이트
            Rate            _rate;

            // 샘플 포맷
            Format          _format;
            
            // 샘플 사이즈
            int32_t         _sample_size;

            // 샘플 포맷명
            std::string     _name;
    };




    //////////////////////////////////////////////////////////////////////////////////////////////////
    // 오디오 레이아웃
    //////////////////////////////////////////////////////////////////////////////////////////////////

    class AudioChannel {
        public:
            enum class Layout : uint32_t 
            {
                AUDIO_FRONT_LEFT = 0x00000001,
                AUDIO_FRONT_RIGHT = 0x00000002,
                AUDIO_FRONT_CENTER =  0x00000004,
                AUDIO_LAYOUT_MONO = AUDIO_FRONT_CENTER,
                AUDIO_LAYOUT_STEREO = (AUDIO_FRONT_LEFT|AUDIO_FRONT_RIGHT)
                // TODO(SOULK) : 추가적인 레이아웃을 지원해야함
            };

        public:
            AudioChannel() : _layout(Layout::AUDIO_LAYOUT_STEREO), _count(2), _name("") {}
            ~AudioChannel() {}

            AudioChannel &operator =(const AudioChannel &T) noexcept
            {
                _layout = T._layout;
                _count = T._count;
                _name = T._name;

                return *this;
            }

            void SetLayout(Layout layout)
            {
                _layout = layout;

                if(_layout == Layout::AUDIO_LAYOUT_STEREO) { _count = 2; _name="stereo"; return; }
                if(_layout == Layout::AUDIO_LAYOUT_MONO) { _count = 1; _name="mono"; return; }
                if(_layout == Layout::AUDIO_FRONT_LEFT) { _count = 1; _name="left"; return; }
                if(_layout == Layout::AUDIO_FRONT_RIGHT) { _count = 1; _name="right"; return; }
                if(_layout == Layout::AUDIO_FRONT_CENTER) { _count = 1; _name="center"; return; }
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
            const char* GetName() 
            {
                return _name.c_str();
            }

            Layout          _layout;
            int32_t         _count;
            std::string     _name;
    };

  

}