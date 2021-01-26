//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <type_traits>

namespace cfg
{
	// #region ========== Annotations ==========
	struct Optional;
	struct ResolvePath;
	// #endregion

	// #region ========== Annotation Utilities ==========
	template <typename... Args>
	struct CheckAnnotations;

	template <typename Texpected, typename Tfirst_type, typename... Ttypes>
	struct CheckAnnotations<Texpected, Tfirst_type, Ttypes...>
	{
		static constexpr bool value = std::is_same<Texpected, Tfirst_type>::value || CheckAnnotations<Texpected, Ttypes...>::value;
	};

	template <typename Texpected>
	struct CheckAnnotations<Texpected>
	{
		static constexpr bool value = false;
	};
	// #endregion
}  // namespace cfg
