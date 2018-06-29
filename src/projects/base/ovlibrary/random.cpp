//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "random.h"
#include <random>
#include <algorithm>

//TODO(getroot): 성능상 문제가 생기면 변수를 멤버 변수로 만들어서 사용한다.
uint32_t ov::Random::GenerateInteger()
{
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<uint32_t> dist(1, UINT32_MAX);
	return dist(mt);
}

ov::String ov::Random::GenerateString(uint32_t size)
{
	std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::random_device rd;
	std::mt19937 generator(rd());
	std::shuffle(str.begin(), str.end(), generator);
	ov::String random_str = str.substr(0, size).c_str();

	return random_str;
}