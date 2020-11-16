//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#define INIT_MODULE(variable, name, create)              \
	logti("Trying to create a " name " module");         \
                                                         \
	auto variable = create;                              \
                                                         \
	if (variable == nullptr)                             \
	{                                                    \
		logte("Failed to initialize " name " module");    \
		return false;                                        \
	}                                                    \
                                                         \
	if (orchestrator->RegisterModule(variable) == false) \
	{                                                    \
		logte("Failed to register" name " module");      \
		return false;                                        \
	}

#define RELEASE_MODULE(variable, name)                         \
	logti("Trying to delete a " name " module");               \
                                                               \
	if (variable != nullptr)                                   \
	{                                                          \
		if (orchestrator->UnregisterModule(variable) != false) \
		{                                                      \
			variable->Stop();                                  \
		}                                                      \
		else                                                   \
		{                                                      \
			logte("Failed to unregister" name " module");      \
		}                                                      \
	}

#define INIT_EXTERNAL_MODULE(name, func)                                          \
	{                                                                             \
		logtd("Trying to initialize " name "...");                                \
		auto error = func();                                                      \
                                                                                  \
		if (error != nullptr)                                                     \
		{                                                                         \
			logte("Could not initialize " name ": %s", error->ToString().CStr()); \
			return false;                                                             \
		}                                                                         \
	}

#define TERMINATE_EXTERNAL_MODULE(name, func)                                    \
	{                                                                            \
		logtd("Trying to terminate " name "...");                                \
		auto error = func();                                                     \
                                                                                 \
		if (error != nullptr)                                                    \
		{                                                                        \
			logte("Could not terminate " name ": %s", error->ToString().CStr()); \
			return 1;                                                            \
		}                                                                        \
	}
