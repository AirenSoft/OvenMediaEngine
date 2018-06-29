//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace ov
{
	template<class T>
	class Singleton
	{
	public:
		explicit Singleton()
		{
		}

		virtual ~Singleton()
		{
		}

		static T *Instance()
		{
			static T instance;

			return &instance;
		}
	};

}