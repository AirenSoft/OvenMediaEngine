//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../http_response.h"
#include "../../protocol/http2/http2_frame.h"
#include "../../hpack/encoder.h"

namespace http
{
	namespace svr
	{
		namespace h2
		{
			class Http2Response : public HttpResponse
			{
			public:
				// Constructor
				Http2Response(const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<hpack::Encoder> &hpack_encoder)
					: HttpResponse(client_socket)
				{
					_hpack_encoder = hpack_encoder;
				}

				bool Send(const std::shared_ptr<prot::h2::Http2Frame> &frame)
				{
					return HttpResponse::Send(frame->ToData());
				}

			private:
				uint32_t SendHeader() override
				{
					return 0;
				}

				uint32_t SendPayload() override
				{
					return 0;
				}

				std::shared_ptr<hpack::Encoder> _hpack_encoder;
			};
		}
	}
}