//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include <base/info/media_track.h>
#include <base/ovlibrary/ovlibrary.h>

#include "h264/h264_converter.h"
#include "aac/aac_converter.h"

class CodecMediaType
{
public:
	// RFC 6381
	static ov::String GetCodecsParameter(const std::shared_ptr<const MediaTrack> &track)
	{
		ov::String codec_string;

		switch (track->GetCodecId())
		{
			case cmn::MediaCodecId::None:
				// Not supported
				break;

			case cmn::MediaCodecId::H264: {
				auto profile_string = H264Converter::GetProfileString(track->GetCodecExtradata());
				if (profile_string.IsEmpty())
				{
					profile_string = H264_CONVERTER_DEFAULT_PROFILE;
				}

				codec_string.AppendFormat("avc1.%s", profile_string.CStr());

				break;
			}

			case cmn::MediaCodecId::H265: {
				// https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#ISO_Base_Media_File_Format_syntax
				//
				// cccc.PP.LL.DD.CC[.cp[.tc[.mc[.FF]]]]
				//
				// cccc = FOURCC
				// PP = Profile
				// LL = Level
				// DD = Bit Depth
				// CC = Chroma Subsampling
				// cp = Color Primaries
				// tc = Transfer Characteristics
				// mc = Matrix Coefficients
				// FF = Indicates whether to restrict tlack level and color range of each color
				codec_string = "hev1.1.6.L93.B0";
				break;
			}

			case cmn::MediaCodecId::Vp8: {
				// Not supported (vp08.00.41.08)
				break;
			}

			case cmn::MediaCodecId::Vp9: {
				// Not supported (vp09.02.10.10.01.09.16.09.01)
				break;
			}

			case cmn::MediaCodecId::Flv: {
				// Not supported
				break;
			}

			case cmn::MediaCodecId::Aac: {
				auto profile_string = AacConverter::GetProfileString(track->GetAacConfig());

				if (profile_string.IsEmpty())
				{
					profile_string = AAC_CONVERTER_DEFAULT_PROFILE;
				}

				// "mp4a.oo[.A]"
				//   oo = OTI = [Object Type Indication]
				//   A = [Object Type Number]
				//
				// OTI == 40 (Audio ISO/IEC 14496-3)
				//   http://mp4ra.org/#/object_types
				//
				// OTN == profile_number
				//   https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#MPEG-4_audio
				codec_string.AppendFormat("mp4a.40.%s", profile_string.CStr());

				break;
			}

			case cmn::MediaCodecId::Mp3:
				// OTI == 40 (Audio ISO/IEC 14496-3)
				//   http://mp4ra.org/#/object_types
				//
				// OTN == 34 (MPEG-1 Layer-3 (MP3))
				//   https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#MPEG-4_audio
				codec_string.AppendFormat("mp4a.40.34");
				break;

			case cmn::MediaCodecId::Opus:
				// http://mp4ra.org/#/object_types
				// OTI == ad (Opus audio)
				codec_string.AppendFormat("mp4a.ad");
				break;

			case cmn::MediaCodecId::Jpeg: {
				// Not supported
				break;
			}

			case cmn::MediaCodecId::Png: {
				// Not supported
				break;
			}
		}

		return codec_string;
	}
};