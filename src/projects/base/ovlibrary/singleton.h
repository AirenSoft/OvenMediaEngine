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
		virtual ~Singleton() = default;

		static T *GetInstance()
		{
			static T instance;
			instance.InitSingletonInstance();
			return &instance;
		}

	protected:
		Singleton() = default;

	private:
		virtual void InitSingletonInstance()
		{
			// For derived class
		}
	};
}