//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

// const auto &GetInt() const { return int_value; }
#define CFG_DECLARE_REF_GETTER_OF(getter_name, variable_name) \
	const auto &getter_name(bool *is_parsed = nullptr) const  \
	{                                                         \
		if (is_parsed != nullptr)                             \
		{                                                     \
			*is_parsed = IsParsed(&variable_name);            \
		}                                                     \
                                                              \
		return variable_name;                                 \
	}

// virtual const decltype(int_value) GetInt() const { return int_value; }
#define CFG_DECLARE_VIRTUAL_REF_GETTER_OF(getter_name, variable_name)                   \
	virtual const decltype(variable_name) &getter_name(bool *is_parsed = nullptr) const \
	{                                                                                   \
		if (is_parsed != nullptr)                                                       \
		{                                                                               \
			*is_parsed = IsParsed(&variable_name);                                      \
		}                                                                               \
                                                                                        \
		return variable_name;                                                           \
	}

// decltype(int_value) GetInt() const override { return int_value; }
#define CFG_DECLARE_OVERRIDED_GETTER_OF(getter_name, variable_name)               \
	decltype(variable_name) getter_name(bool *is_parsed = nullptr) const override \
	{                                                                             \
		if (is_parsed != nullptr)                                                 \
		{                                                                         \
			*is_parsed = IsParsed(&variable_name);                                \
		}                                                                         \
                                                                                  \
		return variable_name;                                                     \
	}
