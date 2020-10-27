//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http_server/http_server.h>

#include <memory>

namespace api
{
	template <typename Tclass>
	class Controller
	{
	public:
		// For all Handler registrations, prefix before pattern
		void AppendPrefix(const ov::String &prefix)
		{
			_prefix.Append(prefix);
		}

		void SetInterceptor(const std::shared_ptr<HttpDefaultInterceptor> &interceptor)
		{
			_interceptor = interceptor;
		}

		template <typename Tcontroller>
		std::shared_ptr<Controller<Tcontroller>> CreateSubController()
		{
			auto instance = std::make_shared<Tcontroller>();

			instance->AppendPrefix(_prefix);
			instance->SetInterceptor(_interceptor);

			instance->PrepareHandlers();

			return instance;
		}

		virtual void PrepareHandlers() = 0;

	protected:
		using ApiHandler = HttpNextHandler (Tclass::*)(const std::shared_ptr<HttpClient> &client);

		// Register handlers
		void PostHandler(const ov::String &pattern, const ApiHandler &handler)
		{
			auto function = std::bind(handler, dynamic_cast<Tclass *>(this), std::placeholders::_1);
			_interceptor->RegisterPost(ov::String::FormatString("%s%s", _prefix.CStr(), pattern.CStr()), function);
		}

		void GetHandler(const ov::String &pattern, const ApiHandler &handler)
		{
			auto function = std::bind(handler, dynamic_cast<Tclass *>(this), std::placeholders::_1);
			_interceptor->RegisterGet(ov::String::FormatString("%s%s", _prefix.CStr(), pattern.CStr()), function);
		}

		void PutHandler(const ov::String &pattern, const ApiHandler &handler)
		{
			auto function = std::bind(handler, dynamic_cast<Tclass *>(this), std::placeholders::_1);
			_interceptor->RegisterPut(ov::String::FormatString("%s%s", _prefix.CStr(), pattern.CStr()), function);
		}

		void DeleteHandler(const ov::String &pattern, const ApiHandler &handler)
		{
			auto function = std::bind(handler, dynamic_cast<Tclass *>(this), std::placeholders::_1);
			_interceptor->RegisterDelete(ov::String::FormatString("%s%s", _prefix.CStr(), pattern.CStr()), function);
		}

		ov::String _prefix;
		std::shared_ptr<HttpDefaultInterceptor> _interceptor;
	};
}  // namespace api