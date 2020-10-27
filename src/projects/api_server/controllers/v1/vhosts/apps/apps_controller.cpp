//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "apps_controller.h"

#include <functional>

#include "../../../../api_private.h"

namespace api
{
	namespace v1
	{
		class Temp;

		template <typename T>
		class TB
		{
		protected:
			using TF = std::function<void()>;

			// 인자 타입: int (api::v1::Temp::*)(int))
			// typedef int(T::* TFF)(T *const, int);
			// typedef int (T::* TFF)(int value);
			using TFF = void (T::*)();

		public:
			virtual void Prepare() = 0;

		protected:
			// template <typename Thandler>
			// (int (api::v1::Temp::*)(api::v1::Temp * const, int)) 0x555555dfa02e <api::v1::Temp::Method(int)>
			// void Register(Thandler handler)
			// ‘api::v1::Temp::Register(int (api::v1::Temp::*)(int))’
			void Register(TFF handler)
			{
				// TF ff = std::bind(handler, dynamic_cast<T *>(this), std::placeholders::_1);
				TF ff = std::bind(handler, dynamic_cast<T *>(this));

				Register(ff);
			}

			void Register(const TF &func)
			{
				_functions.push_back(func);
			}

			std::vector<TF> _functions;
		};

		class Temp : public TB<Temp>
		{
		public:
			void Prepare() override
			{
				Register(&Temp::MM);

				// Register(&Temp::Method);
				// Register(&Temp::Method2);

				for (auto &function : _functions)
				{
					// printf(">>>>>>> %d\n", function(10));
					function();
				}

				fflush(stdout);
				exit(0);
			}

			void MM()
			{
			}

			int Method(int a)
			{
				return a;
			}

			int Method2(int a)
			{
				return a * 10;
			}
		};

		void AppsController::PrepareHandlers()
		{
			// std::function<HttpNextHandler(const std::shared_ptr<HttpClient> &client)>
			PostHandler("/[^/]+", &AppsController::OnVhostsApps);
		};

		HttpNextHandler AppsController::OnVhostsApps(const std::shared_ptr<HttpClient> &client)
		{
			logti(">Hello");
			return HttpNextHandler::DoNotCall;
		}
	}  // namespace v1
}  // namespace api
