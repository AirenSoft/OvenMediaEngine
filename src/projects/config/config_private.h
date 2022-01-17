//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "utilities/config_utility.h"

#define OV_LOG_TAG                      "Config"

#define logap(format, ...) logtp("[%p] " format, this, ##__VA_ARGS__)
#define logad(format, ...) logtd("[%p] " format, this, ##__VA_ARGS__)
#define logas(format, ...) logts("[%p] " format, this, ##__VA_ARGS__)

#define logai(format, ...) logti("[%p] " format, this, ##__VA_ARGS__)
#define logaw(format, ...) logtw("[%p] " format, this, ##__VA_ARGS__)
#define logae(format, ...) logte("[%p] " format, this, ##__VA_ARGS__)
#define logac(format, ...) logtc("[%p] " format, this, ##__VA_ARGS__)