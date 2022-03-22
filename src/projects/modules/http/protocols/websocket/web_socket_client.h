#include <utility>

//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

#include "./web_socket_datastructure.h"
#include <variant>

namespace http
{
	namespace svr
	{
		namespace ws
		{
			class Client
			{
			public:
				Client(const std::shared_ptr<HttpTransaction> &transaction);
				virtual ~Client();

				ssize_t Send(const std::shared_ptr<const ov::Data> &data, FrameOpcode opcode);
				ssize_t Send(const std::shared_ptr<const ov::Data> &data);
				ssize_t Send(const ov::String &string);
				ssize_t Send(const Json::Value &value);

				const std::shared_ptr<HttpTransaction> &GetClient()
				{
					return _transaction;
				}

				ov::String ToString() const;

				void AddData(ov::String key, std::variant<bool, uint64_t, ov::String> value);
				std::tuple<bool, std::variant<bool, uint64_t, ov::String>> GetData(ov::String key);

				void Close();

			protected:
				std::shared_ptr<HttpTransaction> _transaction;

			private:
				// key : data<int or string>
				std::map<ov::String, std::variant<bool, uint64_t, ov::String>> _data_map;
			};
		}  // namespace ws
	}	   // namespace svr
}  // namespace http
