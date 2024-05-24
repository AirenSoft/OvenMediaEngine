//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct Options : public Item
				{
				protected:
					bool _webrtc_auto_abr = true;
					
					// If this option is true, ts publisher will use this playlist
					bool _enable_ts_packaging = false;

					// -1(default) : absolute - /app/stream/chunklist.m3u8
					// 0 : relative file - chunklist.m3u8
					// 1 : relative stream - ../stream/chunklist.m3u8
					// 2 : relative app - ../../app/stream/chunklist.m3u8
					int _hls_chunklist_path_depth = -1;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(IsWebRtcAutoAbr, _webrtc_auto_abr);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetHlsChunklistPathDepth, _hls_chunklist_path_depth);
					CFG_DECLARE_CONST_REF_GETTER_OF(IsTsPackagingEnabled, _enable_ts_packaging);

				protected:
					void MakeList() override
					{
						Register<Optional>("WebRtcAutoAbr", &_webrtc_auto_abr);
						Register<Optional>("HLSChunklistPathDepth", &_hls_chunklist_path_depth);
						Register<Optional>("EnableTsPackaging", &_enable_ts_packaging);
					}
				};
			} // namespace oprf
		} // namespace app
	} // namespace vhost
}  // namespace cfg