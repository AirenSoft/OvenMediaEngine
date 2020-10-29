//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "./error.h"

// These classes are a simple PCRE2 wrapper
//
// Note - I wanted to use ov::String, but ov::String did not have the same function as std::string_view, so I used std::string* classes.

// Usage:
//
//    auto regex = ov::Regex("(?<token1>[^/]+)/.+");
//    auto error = regex.Compile();
//
//    if (error == nullptr)
//    {
//        auto matches = regex.Matches("Dim/iden");
//
//        auto group_list = matches.GetGroupList();
//
//        printf("=== Group List ===\n");
//        int index = 0;
//        for (auto &group : group_list)
//        {
//            printf("%d: %.*s\n", index, static_cast<int>(group.length()), group.data());
//            index++;
//        }
//
//        printf("=== Named Group List ===\n");
//        auto named_list = matches.GetNamedGroupList();
//
//        for (auto &item : named_list)
//        {
//            auto &key = item.first;
//            auto &value = item.second;
//
//            printf("%s = %.*s\n", key.c_str(), static_cast<int>(value.length()), value.data());
//        }
//    }
//    else
//    {
//        printf("%s\n", error->ToString().CStr());
//    }

namespace ov
{
	// Match result must be a
	class MatchResult
	{
	public:
		MatchResult();

		// MatchResult with error
		MatchResult(const std::shared_ptr<ov::Error> &error);

		// MatchResult with results
		MatchResult(std::shared_ptr<std::string> subject,
					std::vector<std::string_view> group_list,
					std::map<std::string, std::string_view> named_group);

		const std::shared_ptr<ov::Error> &GetError() const;

		const std::string &GetSubject() const;

		size_t GetGroupCount() const;
		std::string_view GetGroupAt(size_t index) const;
		const std::vector<std::string_view> &GetGroupList() const;

		size_t GetNamedGroupCount() const;
		std::string_view GetNamedGroup(const char *name) const;
		const std::map<std::string, std::string_view> &GetNamedGroupList() const;

	protected:
		std::shared_ptr<Error> _error;

		// Stored as std::shared_ptr for std::string_view to work well even if MatchResult is copied
		std::shared_ptr<std::string> _subject;

		std::vector<std::string_view> _group_list;
		std::map<std::string, std::string_view> _named_group;
	};

	class Regex
	{
	public:
		enum class Option
		{
			None,

			// PCRE2_CASELESS
			CaseInsensitive,
			// PCRE2_MULTILINE
			Multiline,
			// PCRE2_DOTALL
			DotAll,
			// PCRE2_LITERAL
			Literal
		};

		// TODO(dimiden): Implement a feature to support multiple options
		Regex(const char *pattern, Option options);
		Regex(const char *pattern);
		Regex(const Regex &regex);
		Regex(Regex &&regex);
		~Regex();

		// TODO(dimiden): Copy/Move Ctor is needed for _code using pcre2_code_copy

		const std::string &GetPattern() const;

		std::shared_ptr<Error> Compile();

		bool Test(const char *subject);
		MatchResult Matches(const char *subject);

		void Release();

	protected:
		std::map<std::string, std::string_view> CreateNamedGroup(const char *base_address, const size_t *output_vectors);

		std::string _pattern;
		Option _options;

		// To hide pcre2_code
		void *_code = nullptr;
	};
}  // namespace ov