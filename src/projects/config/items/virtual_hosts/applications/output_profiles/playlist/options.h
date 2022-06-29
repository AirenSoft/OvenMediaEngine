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

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(IsWebRtcAutoAbr, _webrtc_auto_abr);

				protected:
					void MakeList() override
					{
						Register<Optional>("WebRtcAutoAbr", &_webrtc_auto_abr);
					}
				};
			} // namespace oprf
		} // namespace app
	} // namespace vhost
}  // namespace cfg