//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/converter.h>
#include "../http_datastructure.h"

namespace http
{
	namespace svr
	{
		class HttpExchange;

		class HttpResponse : public ov::EnableSharedFromThis<HttpResponse>
		{
		public:
			HttpResponse(const std::shared_ptr<ov::ClientSocket> &client_socket);
			~HttpResponse() override = default;

			std::shared_ptr<ov::ClientSocket> GetRemote();
			std::shared_ptr<const ov::ClientSocket> GetRemote() const;

			void SetTlsData(const std::shared_ptr<ov::TlsServerData> &tls_data);
			std::shared_ptr<ov::TlsServerData> GetTlsData();

			StatusCode GetStatusCode() const;
			// Get Reason
			ov::String GetReason();

			// reason = default
			void SetStatusCode(StatusCode status_code);
			// custom reason
			void SetStatusCode(StatusCode status_code, const ov::String &reason);

			// Append a new item to the existing header
			bool AddHeader(const ov::String &key, const ov::String &value);
			// Overwrites the existing value to <value>
			bool SetHeader(const ov::String &key, const ov::String &value);
			const std::vector<ov::String> &GetHeader(const ov::String &key) const;
			bool RemoveHeader(const ov::String &key);

			// Enqueue the data into the queue (This data will be sent when SendResponse() is called)
			// Can be used for response with content-length
			bool AppendData(const std::shared_ptr<const ov::Data> &data);
			bool AppendString(const ov::String &string);
			bool AppendFile(const ov::String &filename);

			// Send the data immediately
			// Can be used for response without content-length
			template <typename T>
			bool Send(const T *data)
			{
				return Send(data, sizeof(T));
			}
			virtual bool Send(const void *data, size_t length);
			virtual bool Send(const std::shared_ptr<const ov::Data> &data);

			uint32_t Response();

			bool Close();

		protected:
			bool IsHeaderSent() const;
			// Get Response Data Size
			size_t GetResponseDataSize() const;
			// Get Response Data List
			const std::vector<std::shared_ptr<const ov::Data>> &GetResponseDataList() const;
			// Get Response Header
			const std::unordered_map<ov::String, std::vector<ov::String>> &GetResponseHeaderList() const;
			void ResetResponseData();
		private:
			virtual uint32_t SendHeader();
			virtual uint32_t SendResponse();

			std::shared_ptr<ov::ClientSocket> _client_socket;
			std::shared_ptr<ov::TlsServerData> _tls_data;

			StatusCode _status_code = StatusCode::OK;
			ov::String _reason = StringFromStatusCode(StatusCode::OK);

			bool _is_header_sent = false;
			
			// FIXME(dimiden): It is supposed to be synchronized whenever a packet is sent, but performance needs to be improved
			std::recursive_mutex _response_mutex;
			std::unordered_map<ov::String, std::vector<ov::String>> _response_header;
			std::vector<std::shared_ptr<const ov::Data>> _response_data_list;
			size_t _response_data_size = 0;

			std::vector<ov::String> _default_value{};
		};
	}  // namespace svr
}  // namespace http
