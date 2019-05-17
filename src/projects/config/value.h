//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "value_base.h"

namespace cfg
{
	class Item;

	//region Value

	// int, float, string 등 처리용
	template<typename Ttype, typename Tdummy = void>
	class Value : public ValueBase
	{
	public:
		Value(ValueType type, Ttype *target) : ValueBase()
		{
			_type = type;
			_target = target;
			_value_size = sizeof(Ttype);
		}

		Ttype *GetTarget()
		{
			return static_cast<Ttype *>(_target);
		}

	protected:
		using ValueBase::_target;
	};

	// Item 처리용
	class ValueForElementBase : public ValueBase
	{
	public:
		virtual bool Copy(ValueForElementBase *copy_to) const = 0;

		Item *GetTarget()
		{
			return static_cast<Item *>(_target);
		}
	};

	template<typename Ttype>
	class ValueForElement : public ValueForElementBase
	{
	public:
		explicit ValueForElement(Ttype *item) : ValueForElementBase()
		{
			_type = ValueType::Element;
			_target = item;
			_value_size = sizeof(Ttype);
		}

		bool Copy(ValueForElementBase *copy_to) const override
		{
			auto source = static_cast<Ttype *>(_target);
			auto target = static_cast<Ttype *>(copy_to->GetTarget());

			if((source == nullptr) || (target == nullptr))
			{
				OV_ASSERT2(false);
				return false;
			}

			*target = *source;

			return true;
		}
	};

	// vector 처리용
	class ValueForListBase : public ValueBase
	{
	public:
		virtual Item *Create() = 0;
		virtual void Clear() = 0;
		virtual std::vector<Item *> GetList() = 0;
		virtual bool Copy(ValueForListBase *copy_to) const = 0;

		void *GetTarget()
		{
			return _target;
		}
	};

	template<typename Ttype>
	class ValueForList : public ValueForListBase
	{
	public:
		explicit ValueForList(std::vector<Ttype> *target) : ValueForListBase()
		{
			_type = ValueType::List;
			_target = target;
			_value_size = sizeof(target);
		}

		std::vector<Item *> GetList() override
		{
			std::vector<Item *> list;

			for(auto &item : *(static_cast<std::vector<Ttype> *>(_target)))
			{
				list.push_back(&item);
			}

			return list;
		}

		void Clear() override
		{
			auto target = static_cast<std::vector<Ttype> *>(_target);
			target->clear();
		}

		Item *Create() override
		{
			auto target = static_cast<std::vector<Ttype> *>(_target);
			target->emplace_back();

			return &(target->back());
		}

		bool Copy(ValueForListBase *copy_to) const override
		{
			auto source = static_cast<std::vector<Ttype> *>(_target);
			auto target = static_cast<std::vector<Ttype> *>(copy_to->GetTarget());

			*target = *source;

			return true;
		}
	};

	//endregion
}