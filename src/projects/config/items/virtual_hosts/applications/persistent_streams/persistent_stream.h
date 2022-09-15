//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan Kwon
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once


namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace prst
			{
				struct Stream : public Item
				{
				protected:
					ov::String _output_stream_name;
					ov::String _source_stream_match = "*";
					ov::String _fallback_stream_name = "";

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputStreamName, _output_stream_name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetSourceStreamMatch, _source_stream_match)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetFallbackStreamName, _fallback_stream_name)

				protected:
					void MakeList() override
					{
						Register("OutputStreamName", &_output_stream_name);
						Register<Optional>("SourceStreamMatch", &_source_stream_match);
						Register<Optional>("FallbackStreamName", &_fallback_stream_name);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
