//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stun_datastructure.h"

#include <algorithm>

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

#include "modules/ice/stun/attributes/stun_attribute.h"

class StunMessage
{
public:
	enum LastErrorCode : uint8_t
	{
		NOT_USED = 0,
		SUCCESS,
		INVALID_DATA,
		NOT_ENOUGH_DATA
	};

	StunMessage();
	virtual ~StunMessage();

	bool Parse(ov::ByteStream &stream);
	bool ParseHeader(ov::ByteStream &stream);

	LastErrorCode GetLastErrorCode() const;
	bool IsValid() const;
	bool CheckIntegrity(const ov::String &password) const;
	bool GetUfrags(ov::String *local_ufrag, ov::String *remote_ufrag) const;

	static constexpr int DefaultHeaderLength()
	{
		// All STUN messages MUST start with a 20-byte header followed by zero or more Attributes

		// Type (2B) + Message Length (2B) + Magic Cookie (4B) + Transaction ID (12B) = 20B
		return sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + (sizeof(uint8_t) * 12);
	}

	// stun response 정의 및 credential hash 생성은 stun.cc:958 부근 참고

	ov::String ToString() const;

	// 기본 헤더들의 Getter/Setter 모음
	uint16_t GetType() const;
	// SetType() = SetClass() + SetMethod()
	void SetType(uint16_t type);
	StunClass GetClass() const;
	const char *GetClassString() const;
	void SetClass(StunClass clazz);
	StunMethod GetMethod() const;
	const char *GetMethodString() const;
	void SetMethod(StunMethod method);

	// message length는 자동계산 되므로, setter가 없어도 됨
	uint16_t GetMessageLength() const;
	// magic cookie는 고정되어 있으므로, setter가 없어도 됨
	uint32_t GetMagicCookie() const;

	// 길이: OV_STUN_TRANSACTION_ID_LENGTH
	const uint8_t *GetTransactionId() const;
	void SetTransactionId(const uint8_t transaction_id[OV_STUN_TRANSACTION_ID_LENGTH]);

	// type에 해당하는 attribute를 얻어옴
	// native 코드를 보니, 동일 type이 여러 개 있는 경우는 없는듯
	template<class T>
	const T *GetAttribute(StunAttributeType type) const
	{
		for(const auto &attribute : _attributes)
		{
			if(attribute->GetType() == type)
			{
				return static_cast<const T *>(attribute.get());
			}
		}

		return nullptr;
	}

	bool AddAttribute(std::unique_ptr<StunAttribute> attribute);

	std::shared_ptr<ov::Data> Serialize(const ov::String &integrity_key);

protected:
	
	bool ParseAttributes(ov::ByteStream &stream);
	std::unique_ptr<StunAttribute> ParseFingerprintAttribute(ov::ByteStream &stream);

	// 데이터 생성할 때 사용
	bool WriteHeader(ov::ByteStream &stream);
	bool WriteMessageLength(ov::ByteStream &stream);
	bool WriteAttributes(ov::ByteStream &stream);
	bool WriteMessageIntegrityAttribute(ov::ByteStream &stream, const ov::String &integrity_key);
	bool WriteFingerprintAttribute(ov::ByteStream &stream);

	// stun 정보
	// FINGERPRINT 유효성 판단 (stun.cc:277)
	// spec: RFC 5389, section 7.3, 15.5
	bool CalculateFingerprint(ov::ByteStream &stream, ssize_t length, uint32_t *fingerprint) const;
	bool ValidateFingerprint(ov::ByteStream &stream);

	bool _parsed;

	// STUN 기본 헤더
	uint16_t _type;
	uint16_t _message_length;
	uint32_t _magic_cookie;
	uint8_t _transaction_id[OV_STUN_TRANSACTION_ID_LENGTH];

	std::vector<std::unique_ptr<StunAttribute>> _attributes;

	// Integrity attribute와 Fingerprint attribute는 모든 데이터가 입력된 후 Serialze()할 때 자동 생성 되는 것이므로 별도로 취급함
	std::unique_ptr<StunAttribute> _integrity_attribute;
	std::unique_ptr<StunAttribute> _fingerprint_attribute;

	// Last error codes
	LastErrorCode	_last_error_code = NOT_USED;
};
