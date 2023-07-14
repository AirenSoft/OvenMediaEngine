//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../mpegtspush/mpegtspush_application.h"

class SrtPushApplication : public MpegtsPushApplication
{
public:
	static std::shared_ptr<SrtPushApplication> Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	SrtPushApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	~SrtPushApplication() override;

public:
	virtual std::shared_ptr<ov::Error> StartPush(const std::shared_ptr<info::Push> &push) override;
};