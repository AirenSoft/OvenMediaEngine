//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "amf_document.h"

// definition
typedef enum
{
	AMF_MARKER_NUMBER		= 0x00,
	AMF_MARKER_BOOLEAN		= 0x01,
	AMF_MARKER_STRING		= 0x02,
	AMF_MARKER_OBJECT		= 0x03,
	AMF_MARKER_MOVIECLIP	= 0x04,
	AMF_MARKER_NULL			= 0x05,
	AMF_MARKER_UNDEFINED	= 0x06,
	AMF_MARKER_REFERENCE	= 0x07,
	AMF_MARKER_ECMA_ARRAY	= 0x08,
	AMF_MARKER_OBJECT_END	= 0x09,
	AMF_MARKER_STRICT_ARRAY	= 0x0a,
	AMF_MARKER_DATE			= 0x0b,
	AMF_MARKER_LONG_STRING	= 0x0c,
	AMF_MARKER_UNSUPPORTED	= 0x0d,
	AMF_MARKER_RECORDSET	= 0x0e,
	AMF_MARKER_XML_DOCUMENT	= 0x0f,
	AMF_MARKER_TYPED_OBJECT	= 0x10,
} AmfTypeMarker;



//////////////////////////////////////////////////////////////////////////
//                              AmfUtil                                //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfUtil - WriteInt8
//====================================================================================================
int AmfUtil::WriteInt8(void * data, uint8_t Number)
{
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 기록
	pt_out[0] = Number;

	return 1;
}

//====================================================================================================
// AmfUtil - WriteInt16
//====================================================================================================
int AmfUtil::WriteInt16(void * data, uint16_t Number)
{
	auto	*pt_out = (uint8_t *)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 기록
	pt_out[0] = (uint8_t)(Number >> 8);
	pt_out[1] =  (uint8_t)(Number & 0xff);

	return 2;
}

//====================================================================================================
// AmfUtil - WriteInt24
//====================================================================================================
int AmfUtil::WriteInt24(void * data, uint32_t Number)
{
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 기록
	pt_out[0] = (uint8_t)(Number >> 16);
	pt_out[1] = (uint8_t)( Number >> 8);
	pt_out[2] = (uint8_t)(Number & 0xff);

	return 3;
}

//====================================================================================================
// AmfUtil - WriteInt32
//====================================================================================================
int AmfUtil::WriteInt32(void * data, uint32_t Number)
{
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 기록
	pt_out[0] = (uint8_t)(Number >> 24);
	pt_out[1] = (uint8_t)(Number >> 16);
	pt_out[2] = (uint8_t)(Number >> 8);
	pt_out[3] = (uint8_t)(Number & 0xff);

	return 4;
}

//====================================================================================================
// AmfUtil - ReadInt8
//====================================================================================================
uint8_t AmfUtil::ReadInt8(void * data)
{
	auto	*pt_in = (uint8_t*)data;
	uint8_t	number = 0;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 읽기
	number |= pt_in[0];

	return number;
}

//====================================================================================================
// AmfUtil - ReadInt16
//====================================================================================================
uint16_t AmfUtil::ReadInt16(void * data)
{
	auto	*pt_in = (uint8_t*)data;
	uint16_t	number = 0;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 읽기
	number |= pt_in[0] << 8;
	number |= pt_in[1];

	return number;
}

//====================================================================================================
// AmfUtil - ReadInt24
//====================================================================================================
uint32_t AmfUtil::ReadInt24(void * data)
{
	auto	*pt_in = (uint8_t*)data;
	uint32_t	number = 0;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 읽기
	number |= pt_in[0] << 16;
	number |= pt_in[1] << 8;
	number |= pt_in[2];

	return number;
}

//====================================================================================================
// AmfUtil - ReadInt32
//====================================================================================================
uint32_t AmfUtil::ReadInt32(void * data)
{
	auto	*pt_in = (uint8_t*)data;
	uint32_t	number = 0;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 읽기
	number |= pt_in[0] << 24;
	number |= pt_in[1] << 16;
	number |= pt_in[2] << 8;
	number |= pt_in[3];

	return number;
}

//====================================================================================================
// AmfUtil - WriteInt8
//====================================================================================================
int AmfUtil::EncodeNumber(void * data, double Number)
{
	auto	*pt_in = (uint8_t*)&Number;
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// marker 기록
	pt_out += WriteInt8(pt_out, AMF_MARKER_NUMBER);

	// 데이터 기록
	pt_out[0] = pt_in[7];
	pt_out[1] = pt_in[6];
	pt_out[2] = pt_in[5];
	pt_out[3] = pt_in[4];
	pt_out[4] = pt_in[3];
	pt_out[5] = pt_in[2];
	pt_out[6] = pt_in[1];
	pt_out[7] = pt_in[0];

	return (1+8);
}

//====================================================================================================
// AmfUtil - WriteInt8
//====================================================================================================
int AmfUtil::EncodeBoolean(void * data, bool Boolean)
{
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// marker 기록
	pt_out += WriteInt8(pt_out, AMF_MARKER_BOOLEAN);

	// 데이터 기록
	pt_out[0] = (Boolean ? 1 : 0);

	return (1+1);
}

//====================================================================================================
// AmfUtil - EncodeString
//====================================================================================================
int AmfUtil::EncodeString(void * data, char *  pString)
{
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data || !pString ) { return 0; }

	// marker 기록
	pt_out += WriteInt8(pt_out, AMF_MARKER_STRING);

	// 데이터 기록
	pt_out += WriteInt16(pt_out, (uint16_t)strlen(pString));
	strncpy((char*)pt_out, pString, strlen(pString));

	return (1+2+(int)strlen(pString));
}

//====================================================================================================
// AmfUtil - DecodeNumber
//====================================================================================================
int AmfUtil::DecodeNumber(void * data, double *pNumber)
{
	auto	*pt_in = (uint8_t*)data;
	auto	*pt_out = (uint8_t*)pNumber;

	// 파라미터 체크
	if( !data || !pNumber ) { return 0; }

	// marker 체크
	if( ReadInt8(pt_in) != AMF_MARKER_NUMBER ) { return 0; }
	pt_in++;

	// 데이터 읽기
	pt_out[0] = pt_in[7];
	pt_out[1] = pt_in[6];
	pt_out[2] = pt_in[5];
	pt_out[3] = pt_in[4];
	pt_out[4] = pt_in[3];
	pt_out[5] = pt_in[2];
	pt_out[6] = pt_in[1];
	pt_out[7] = pt_in[0];

	return (1+8);
}

//====================================================================================================
// AmfUtil - DecodeBoolean
//====================================================================================================
int AmfUtil::DecodeBoolean(void * data, bool *pBoolean)
{
	auto	*pt_in = (uint8_t*)data;

	// 파라미터 체크
	if( !data || !pBoolean ) { return 0; }

	// marker 체크
	if( ReadInt8(pt_in) != AMF_MARKER_BOOLEAN ) { return 0; }
	pt_in++;

	// 데이터 읽기
	*pBoolean = pt_in[0];

	return (1+1);
}

//====================================================================================================
// AmfUtil - DecodeString
//====================================================================================================
int AmfUtil::DecodeString(void * data, char *  pString)
{
	auto	*pt_in = (uint8_t*)data;
	int		str_len;

	// 파라미터 체크
	if( !data || !pString ) { return 0; }

	// marker 체크
	if( ReadInt8(pt_in) != AMF_MARKER_STRING ) { return 0; }
	pt_in++;

	// 데이터 읽기
	str_len = (int)ReadInt16(pt_in);
	pt_in += 2;
	//
	strncpy(pString, (char*)pt_in, str_len);
	pString[str_len] = '\0';

	return (1+2+str_len);
}




//////////////////////////////////////////////////////////////////////////
//                            AmfProperty                              //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty()
{
	// 초기화
	_Initialize();
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(tAMF_DATA_TYPE Type)
{
	// 초기화
	_Initialize();

	// 설정
	_amf_data_type = Type;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(double Number)
{
	// 초기화
	_Initialize();

	// 설정
	_amf_data_type = AMF_NUMBER;
	_number = Number;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(bool Boolean)
{
	// 초기화
	_Initialize();

	// 설정
	_amf_data_type = AMF_BOOLEAN;
	_boolean = Boolean;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(const char *  pString) // 스트링은 내부에서 메모리 할당해서 복사
{
	// 초기화
	_Initialize();

	// 파라미터 체크
	if( !pString ) { return; }

	// 설정
	_string = new char[strlen(pString) + 1];
	strcpy(_string, pString);
	_amf_data_type = AMF_STRING;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(AmfArray *pArray) // array 는 파라미터 포인터를 그대로 저장
{
	// 초기화
	_Initialize();

	// 파라미터 체크
	if( !pArray ) { return; }

	// 설정
	_amf_data_type = AMF_ARRAY;
	_array = pArray;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(AmfObject *pObject) // object 는 파라미터 포인터를 그대로 저장
{
	// 초기화
	_Initialize();

	// 파라미터 체크
	if( !pObject ) { return; }

	// 설정
	_amf_data_type = AMF_OBJECT;
	_object = pObject;
}

//====================================================================================================
// AmfProperty - ~AmfProperty
//====================================================================================================
AmfProperty::~AmfProperty()
{
	// 메모리 해제
	if( _string )	{ delete _string;	_string = nullptr; }
	if( _array )	{ delete _array;	_array = nullptr; }
	if( _object )	{ delete _object;	_object = nullptr; }
}

//====================================================================================================
// AmfProperty - _Initialize
//====================================================================================================
void AmfProperty::_Initialize()
{
	// 초기화
	_amf_data_type		= AMF_NULL;
	_number	= 0.0;
	_boolean	= true;
	_string	= nullptr;
	_array	= nullptr;
	_object	= nullptr;
}

//====================================================================================================
// AmfProperty - Dump
//====================================================================================================
void AmfProperty::Dump(std::string &dump_string)
{
	char text[1024] = {0,};
	switch(GetType())
	{
	case AMF_NULL:
		dump_string += ("nullptr\n");
		break;
	case AMF_UNDEFINED:
		dump_string += ("Undefined\n");
		break;
	case AMF_NUMBER:
		sprintf(text, "%.1f\n", GetNumber());
		dump_string += text;
		break;
	case AMF_BOOLEAN:
		sprintf(text, "%d\n", GetBoolean());
		dump_string += text;
		break;
	case AMF_STRING:
		if( _string )
		{
			sprintf(text, "%s\n", strlen(GetString()) ? GetString() : "SIZE-ZERO STRING");
			dump_string += text;
		}
		break;
	case AMF_ARRAY:
		if( _array )
		{
			_array->Dump(dump_string);
		}
		break;
	case AMF_OBJECT:
		if( _object )
		{
			_object->Dump(dump_string);
		}
		break;
	}
}

//====================================================================================================
// AmfProperty - Encode
//====================================================================================================
int AmfProperty::Encode(void * data) // ret=0이면 실패
{
	auto	*pt_out = (uint8_t*)data;

	// 파라미터 체크
	if( !data ) { return 0; }

	// 타입에 따라 인코딩
	switch(_amf_data_type)
	{
	case AMF_NULL:
		pt_out += WriteInt8(pt_out, AMF_MARKER_NULL);
		break;
	case AMF_UNDEFINED:
		pt_out += WriteInt8(pt_out, AMF_MARKER_UNDEFINED);
		break;
	case AMF_NUMBER:
		pt_out += EncodeNumber(pt_out, _number);
		break;
	case AMF_BOOLEAN:
		pt_out += EncodeBoolean(pt_out, _boolean);
		break;
	case AMF_STRING:
		if( !_string ) { break; }
		pt_out += EncodeString(pt_out, _string);
		break;
	case AMF_ARRAY:
		if( !_array ) { break; }
		pt_out += _array->Encode(pt_out);
		break;
	case AMF_OBJECT:
		if( !_object ) { break; }
		pt_out += _object->Encode(pt_out);
		break;
	default:
		break;
	}

	return (int)(pt_out - (uint8_t*)data);
}

//====================================================================================================
// AmfProperty - Decode
//====================================================================================================
int AmfProperty::Decode(void * data, int DataLen) // ret=0이면 실패
{
	auto	*pt_in = (uint8_t*)data;
	int		size = 0;

	// 파라미터 체크
	if( !data ) { return 0; }
	if( DataLen < 1 ) { return 0; }

	// 타입 초기화
	_amf_data_type = AMF_NULL;

	// 타입에 따라 디코딩
	switch(ReadInt8(data))
	{
	case AMF_MARKER_NULL:
		size = 1;
		_amf_data_type = AMF_NULL;
		break;
	case AMF_MARKER_UNDEFINED:
		size = 1;
		_amf_data_type = AMF_UNDEFINED;
		break;
	case AMF_MARKER_NUMBER:
		if( DataLen < (1+8) ) { break; }
		size = DecodeNumber(pt_in, &_number);
		_amf_data_type = AMF_NUMBER;
		break;
	case AMF_MARKER_BOOLEAN:
		if( DataLen < (1+1) ) { return 0; }
		size = DecodeBoolean(pt_in, &_boolean);
		_amf_data_type = AMF_BOOLEAN;
		break;
	case AMF_MARKER_STRING:
		if( DataLen < (1+3) ) { return 0; }
		if( _string ) { delete _string; _string = nullptr; }
		_string = new char[ReadInt16(pt_in+1) + 1];
		size = DecodeString(pt_in, _string);
		if( !size ) { delete _string; _string = nullptr; }
		_amf_data_type = AMF_STRING;
		break;
	case AMF_MARKER_ECMA_ARRAY:
		if( _array ) { delete _array; _array = nullptr; }
		_array = new AmfArray;
		size = _array->Decode(pt_in, DataLen);
		if( !size ) { delete _array; _array = nullptr; }
		_amf_data_type = AMF_ARRAY;
		break;
	case AMF_MARKER_OBJECT:
		if( _object ) { delete _object; _object = nullptr; }
		_object = new AmfObject;
		size = _object->Decode(pt_in, DataLen);
		if( !size ) { delete _object; _object = nullptr; }
		_amf_data_type = AMF_OBJECT;
		break;
	default:
		break;
	}
	
	return size;
}



//////////////////////////////////////////////////////////////////////////
//                           AmfObjectArray                            //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfObjectArray - AmfObjectArray
//====================================================================================================
AmfObjectArray::AmfObjectArray(tAMF_DATA_TYPE Type)
{
	// 초기화
	_amf_data_type = Type;
	_amf_property_pairs.clear();
}

//====================================================================================================
// AmfObjectArray - ~AmfObjectArray
//====================================================================================================
AmfObjectArray::~AmfObjectArray()
{
	int	i;

	// 해제
	for(i=0; i<(int)_amf_property_pairs.size(); i++)
	{
		if( _amf_property_pairs[i] )
		{
			if( _amf_property_pairs[i]->_property ) { delete _amf_property_pairs[i]->_property; _amf_property_pairs[i]->_property = nullptr; }
			delete _amf_property_pairs[i]; _amf_property_pairs[i] = nullptr;
		}
	}
	_amf_property_pairs.clear();
}

//====================================================================================================
// AmfObjectArray - Dump
//====================================================================================================
void AmfObjectArray::Dump(std::string &dump_string)
{
	int		i;
	char text[1024] = {0,};

	for(i=0; i<(int)_amf_property_pairs.size(); i++)
	{
		sprintf(text, "%s : ", _amf_property_pairs[i]->_name);
		dump_string += text;

		if( _amf_property_pairs[i]->_property->GetType() == AMF_ARRAY || _amf_property_pairs[i]->_property->GetType() == AMF_OBJECT )
		{
			dump_string += ("\n");
		}
		_amf_property_pairs[i]->_property->Dump(dump_string);
	}
}

//====================================================================================================
// AmfObjectArray - Encode
//====================================================================================================
int AmfObjectArray::Encode(void * data)
{
	auto	*pt_out = (uint8_t*)data;
	uint8_t	start_marker, end_marker;
	int		i;

	// marker 설정
	start_marker	= (_amf_data_type == AMF_OBJECT ? AMF_MARKER_OBJECT : AMF_MARKER_ECMA_ARRAY);
	end_marker		= AMF_MARKER_OBJECT_END;

	// 기록할 아이템 개수가 0이면 스킵
	if( _amf_property_pairs.empty())
	{ 
		return 0; 
	}

	// 시작 marker 기록
	pt_out += WriteInt8(pt_out, start_marker);

	// array 이면 카운트값 기록
	if( _amf_data_type == AMF_ARRAY ) 
	{ 
		pt_out += WriteInt32(pt_out, 0); /* 0=infinite */
	}

	// object 아이템 기록
	for(i=0; i<(int)_amf_property_pairs.size(); i++)
	{
		tPROPERTY_PAIR	*pt_pair = nullptr;

		// 아이템 포인터 얻기
		pt_pair = _amf_property_pairs[i];
		if( !pt_pair ) { continue; }

		// property 가 invalid 인지 체크
		//if( pt_pair->_property->GetType() == AMF_NULL ) { continue; }

		// 문자열 기록
		pt_out += WriteInt16(pt_out, (uint16_t)strlen(pt_pair->_name));
		strncpy((char*)pt_out, pt_pair->_name, strlen(pt_pair->_name));
		pt_out += strlen(pt_pair->_name);

		// value 기록
		pt_out += pt_pair->_property->Encode(pt_out);
	}

	// 끝 marker 기록
	pt_out += WriteInt16(pt_out, 0);
	pt_out += WriteInt8(pt_out, end_marker);

	return (int)(pt_out - (uint8_t*)data);
}

//====================================================================================================
// AmfObjectArray - Decode
//====================================================================================================
int AmfObjectArray::Decode(void * data, int DataLen)
{
	auto 			*pt_in = (uint8_t*)data;
	uint8_t	start_marker, end_marker;

	// marker 설정
	start_marker	= (_amf_data_type == AMF_OBJECT ? AMF_MARKER_OBJECT : AMF_MARKER_ECMA_ARRAY);
	end_marker		= AMF_MARKER_OBJECT_END;

	// 시작 marker 확인
	if( ReadInt8(pt_in) != start_marker ) { return 0; }
	pt_in++;

	// array 일 경우 count 값 읽어들임
	if( _amf_data_type == AMF_ARRAY ) 
	{ 
		pt_in += sizeof(uint32_t); 
	}

	// 분석
	while(true)
	{
		tPROPERTY_PAIR	*pt_pair = nullptr;
		int				len;

		// 끝 marker 인가?
		if( ReadInt8(pt_in) == end_marker ) 
		{ 
			pt_in++; 
			break; 
		}

		// _name 길이 읽기
		len = (int)ReadInt16(pt_in); 
		pt_in += sizeof(uint16_t);
		if(len == 0) 
		{ 
			continue; 
		}

		// pair 생성
		pt_pair = new tPROPERTY_PAIR;
		pt_pair->_property = new AmfProperty;

		// _name 읽기
		strncpy(pt_pair->_name, (char*)pt_in, len); 
		pt_in += len;
		pt_pair->_name[len] = '\0';

		// value 읽기
		len = pt_pair->_property->Decode(pt_in, DataLen - (int)(pt_in - (uint8_t*)data));
		pt_in += len;
		
		// value 체크
		//if( !len || pt_pair->_property->GetType() == AMF_NULL )
		if( len == 0 )
		{
			delete pt_pair->_property;	
			pt_pair->_property = nullptr;

			delete pt_pair;					
			pt_pair = nullptr;
			return 0;
		}

		// list 에 추가
		_amf_property_pairs.push_back(pt_pair); 
		pt_pair = nullptr;
	}

	return (int)(pt_in - (uint8_t*)data);
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *  pName, tAMF_DATA_TYPE Type)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }
	if( Type != AMF_NULL && Type != AMF_UNDEFINED ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty(Type);

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *  pName, double Number)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty(Number);

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *  pName, bool Boolean)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty(Boolean);

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *  pName, const char *  pString)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty(pString);

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *  pName, AmfArray *pArray)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty(pArray);

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *  pName, AmfObject *pObject)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty(pObject);

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - AddNullProperty
//====================================================================================================
bool AmfObjectArray::AddNullProperty(const char *  pName)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// 파라미터 체크
	if( !pName ) { return false; }

	// pair 생성
	pt_pair = new tPROPERTY_PAIR;
	strcpy(pt_pair->_name, pName);
	pt_pair->_property = new AmfProperty();

	// 저장
	_amf_property_pairs.push_back(pt_pair); 
	pt_pair = nullptr;

	return true;
}

//====================================================================================================
// AmfObjectArray - _GetPair
//====================================================================================================
AmfObject::tPROPERTY_PAIR* AmfObjectArray::_GetPair(int Index)
{
	// 범위 체크
	if( Index < 0 ) { return nullptr; }
	if( Index >= (int)_amf_property_pairs.size() ) { return nullptr; }

	return _amf_property_pairs[Index];
}

//====================================================================================================
// AmfObjectArray - FindName
//====================================================================================================
int AmfObjectArray::FindName(const char *  pName) // ret<0이면 실패
{
	int		i;

	// 파라미터 체크
	if( !pName ) { return -1; }

	// 검색
	for(i=0; i<(int)_amf_property_pairs.size(); i++)
	{
		if( !strcmp(_amf_property_pairs[i]->_name, pName) )
		{
			return i;
		}
	}

	return -1;
}

//====================================================================================================
// AmfObjectArray - GetName
//====================================================================================================
char *  AmfObjectArray::GetName(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return nullptr; }

	return pt_pair->_name;
}

//====================================================================================================
// AmfObjectArray - GetType
//====================================================================================================
tAMF_DATA_TYPE AmfObjectArray::GetType(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return AMF_NULL; }

	return pt_pair->_property->GetType();
}

//====================================================================================================
// AmfObjectArray - GetNumber
//====================================================================================================
double AmfObjectArray::GetNumber(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return 0; }

	return pt_pair->_property->GetNumber();
}

//====================================================================================================
// AmfObjectArray - GetBoolean
//====================================================================================================
bool AmfObjectArray::GetBoolean(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return false; }

	return pt_pair->_property->GetBoolean();
}

//====================================================================================================
// AmfObjectArray - GetString
//====================================================================================================
char *  AmfObjectArray::GetString(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return nullptr; }

	return pt_pair->_property->GetString();
}

//====================================================================================================
// AmfObjectArray - GetArray
//====================================================================================================
AmfArray* AmfObjectArray::GetArray(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return nullptr; }

	return pt_pair->_property->GetArray();
}

//====================================================================================================
// AmfObjectArray - GetObject
//====================================================================================================
AmfObject* AmfObjectArray::GetObject(int Index)
{
	tPROPERTY_PAIR	*pt_pair = nullptr;

	// pair 얻기
	pt_pair = _GetPair(Index);
	if( !pt_pair ) { return nullptr; }

	return pt_pair->_property->GetObject();
}



//////////////////////////////////////////////////////////////////////////
//                               AmfArray                              //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfArray - AmfObjectArray
//====================================================================================================
AmfArray::AmfArray( ) : AmfObjectArray(AMF_ARRAY)
{

}

//====================================================================================================
// AmfArray - Dump
//====================================================================================================
void AmfArray::Dump(std::string & dump_string)
{
    dump_string += ("\n=> Array Start <=\n");
	AmfObjectArray::Dump(dump_string);
    dump_string += ("=> Array  End  <=\n");
}



//////////////////////////////////////////////////////////////////////////
//                              AmfObject                              //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfObject - AmfObject
//====================================================================================================
AmfObject::AmfObject( ) : AmfObjectArray(AMF_OBJECT)
{

}

//====================================================================================================
// AmfObject - Dump
//====================================================================================================
void AmfObject::Dump(std::string & dump_string)
{
    dump_string += ("=> Object Start <=\n");
	AmfObjectArray::Dump(dump_string);
    dump_string += ("=> Object  End  <=\n");
}



//////////////////////////////////////////////////////////////////////////
//                              AmfDocument                            //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfDocument - AmfDocument
//====================================================================================================
AmfDocument::AmfDocument( )
{
	// 초기화
	_amf_propertys.clear();
}

//====================================================================================================
// AmfDocument - ~AmfDocument
//====================================================================================================
AmfDocument::~AmfDocument( )
{
	int	i;

	// 해제
	for(i=0; i<(int)_amf_propertys.size(); i++)
	{
		if( _amf_propertys[i] ) { delete _amf_propertys[i]; _amf_propertys[i] = nullptr; }
	}
	_amf_propertys.clear();
}

//====================================================================================================
// AmfDocument - Dump
//====================================================================================================
void AmfDocument::Dump(std::string & dump_string)
{
	int	i;

	// 해제
    dump_string += ("\n======= DUMP START =======\n");
	for(i=0; i<(int)_amf_propertys.size(); i++)
	{
		_amf_propertys[i]->Dump(dump_string);
	}
    dump_string += ("\n======= DUMP END =======\n");
}

//====================================================================================================
// AmfDocument - Encode
//====================================================================================================
int AmfDocument::Encode(void * data) // ret=0이면 실패
{
	auto	*pt_out = (uint8_t*)data;
	int		i;

	// 인코딩
	for(i=0; i<(int)_amf_propertys.size(); i++)
	{
		pt_out += _amf_propertys[i]->Encode(pt_out);
	}

	return (int)(pt_out - (uint8_t*)data);
}

//====================================================================================================
// AmfDocument - Decode
//====================================================================================================
int AmfDocument::Decode(void * data, int DataLen) // ret=0이면 실패
{
	auto			*pt_in = (uint8_t*)data;
	int				total_len = 0;
	int				ret_len;

	// 디코딩 시작
	while( total_len < DataLen )
	{
		AmfProperty	*pt_item = nullptr;

		// 새 property 할당
		pt_item = new AmfProperty;

		// 디코딩
		ret_len = pt_item->Decode(pt_in, DataLen-total_len);
		if( !ret_len ) 
		{ 
			delete pt_item; 
			pt_item = nullptr; 
			break; 
		}

		// 크기 재설정
		pt_in += ret_len;
		total_len += ret_len;

		// 등록
		_amf_propertys.push_back(pt_item); 
		pt_item = nullptr;
	}

	return (int)(pt_in - (uint8_t*)data);
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(tAMF_DATA_TYPE Type)
{
	// 파라미터 체크
	if( Type != AMF_NULL && Type != AMF_UNDEFINED ) { return false; }

	// 추가
	_amf_propertys.push_back(new AmfProperty(Type));

	return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(double Number)
{
	// 추가
	_amf_propertys.push_back(new AmfProperty(Number));

	return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(bool Boolean)
{
	// 추가
	_amf_propertys.push_back(new AmfProperty(Boolean));

	return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(const char *  pString)
{
	// 파라미터 체크
	if( !pString ) { return false; }

	// 추가
	_amf_propertys.push_back(new AmfProperty(pString));

	return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(AmfArray *pArray)
{
	// 파라미터 체크
	if( !pArray ) { return false; }

	// 추가
	_amf_propertys.push_back(new AmfProperty(pArray));

	return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(AmfObject *pObject)
{
	// 파라미터 체크
	if( !pObject ) { return false; }

	// 추가
	_amf_propertys.push_back(new AmfProperty(pObject));

	return true;
}

//====================================================================================================
// AmfDocument - GetPropertyCount
//====================================================================================================
int AmfDocument::GetPropertyCount( )
{
	return (int)_amf_propertys.size();
}

//====================================================================================================
// AmfDocument - GetPropertyIndex
//====================================================================================================
int AmfDocument::GetPropertyIndex(char *  pName)
{
	int		i;

	// 파라미터 체크
	if( !pName )
	{
		return -1;
	}

	if( _amf_propertys.empty())
	{
		return -1;
	}

	// 찾기
	for(i=0; i<(int)_amf_propertys.size(); i++)
	{
		if( _amf_propertys[i]->GetType() != AMF_STRING ) { continue; }
		if( strcmp(_amf_propertys[i]->GetString(), pName) ) { continue; }

		return i;
	}

	return -1;
}

//====================================================================================================
// AmfDocument - GetProperty
//====================================================================================================
AmfProperty* AmfDocument::GetProperty(int Index)
{
	// 파라미터 체크
	if( Index < 0 ) { return nullptr; }
	if( Index >= (int)_amf_propertys.size() ) { return nullptr; }

	return _amf_propertys[Index];
}