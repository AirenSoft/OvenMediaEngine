//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

// auto &GetInt() { return int_value; }
#define CFG_DECLARE_REF_GETTER_OF(getter_name, variable_name) \
	auto &getter_name(bool *is_parsed = nullptr)              \
	{                                                         \
		if (is_parsed != nullptr)                             \
		{                                                     \
			*is_parsed = IsParsed(&variable_name);            \
		}                                                     \
                                                              \
		return variable_name;                                 \
	}

// const auto &GetInt() const { return int_value; }
#define CFG_DECLARE_CONST_REF_GETTER_OF(getter_name, variable_name) \
	const auto &getter_name(bool *is_parsed = nullptr) const        \
	{                                                               \
		if (is_parsed != nullptr)                                   \
		{                                                           \
			*is_parsed = IsParsed(&variable_name);                  \
		}                                                           \
                                                                    \
		return variable_name;                                       \
	}

// virtual decltype(variable_name) GetInt() { return int_value; }
#define CFG_DECLARE_VIRTUAL_REF_GETTER_OF(getter_name, variable_name)       \
	virtual decltype(variable_name) &getter_name(bool *is_parsed = nullptr) \
	{                                                                       \
		if (is_parsed != nullptr)                                           \
		{                                                                   \
			*is_parsed = IsParsed(&variable_name);                          \
		}                                                                   \
                                                                            \
		return variable_name;                                               \
	}

// virtual const decltype(variable_name) GetInt() const { return int_value; }
#define CFG_DECLARE_VIRTUAL_CONST_REF_GETTER_OF(getter_name, variable_name)             \
	virtual const decltype(variable_name) &getter_name(bool *is_parsed = nullptr) const \
	{                                                                                   \
		if (is_parsed != nullptr)                                                       \
		{                                                                               \
			*is_parsed = IsParsed(&variable_name);                                      \
		}                                                                               \
                                                                                        \
		return variable_name;                                                           \
	}

// auto GetInt() override { return int_value; }
#define CFG_DECLARE_OVERRIDED_GETTER_OF(getter_name, variable_name) \
	auto getter_name(bool *is_parsed = nullptr) override            \
	{                                                               \
		if (is_parsed != nullptr)                                   \
		{                                                           \
			*is_parsed = IsParsed(&variable_name);                  \
		}                                                           \
                                                                    \
		return variable_name;                                       \
	}

// auto GetInt() const override { return int_value; }
#define CFG_DECLARE_OVERRIDED_CONST_GETTER_OF(getter_name, variable_name) \
	auto getter_name(bool *is_parsed = nullptr) const override            \
	{                                                                     \
		if (is_parsed != nullptr)                                         \
		{                                                                 \
			*is_parsed = IsParsed(&variable_name);                        \
		}                                                                 \
                                                                          \
		return variable_name;                                             \
	}

#define SET_IF_NOT_PARSED(GETTER, SETTER, VARIABLE) \
	{                                               \
		bool is_parsed;                             \
		variant.GETTER(&is_parsed);                 \
		if (!is_parsed)                             \
		{                                           \
			variant.SETTER(VARIABLE);               \
		}                                           \
	}
