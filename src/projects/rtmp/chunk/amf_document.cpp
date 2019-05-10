//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "amf_document.h"




//////////////////////////////////////////////////////////////////////////
//                              AmfUtil                                //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfUtil - WriteInt8
//====================================================================================================
int AmfUtil::WriteInt8(void *data, uint8_t number)
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // 기록
    pt_out[0] = number;

    return 1;
}

//====================================================================================================
// AmfUtil - WriteInt16
//====================================================================================================
int AmfUtil::WriteInt16(void *data, uint16_t number)
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // 기록
    pt_out[0] = (uint8_t) (number >> 8);
    pt_out[1] = (uint8_t) (number & 0xff);

    return 2;
}

//====================================================================================================
// AmfUtil - WriteInt24
//====================================================================================================
int AmfUtil::WriteInt24(void *data, uint32_t number)
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // 기록
    pt_out[0] = (uint8_t) (number >> 16);
    pt_out[1] = (uint8_t) (number >> 8);
    pt_out[2] = (uint8_t) (number & 0xff);

    return 3;
}

//====================================================================================================
// AmfUtil - WriteInt32
//====================================================================================================
int AmfUtil::WriteInt32(void *data, uint32_t number)
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // 기록
    pt_out[0] = (uint8_t) (number >> 24);
    pt_out[1] = (uint8_t) (number >> 16);
    pt_out[2] = (uint8_t) (number >> 8);
    pt_out[3] = (uint8_t) (number & 0xff);

    return 4;
}

//====================================================================================================
// AmfUtil - ReadInt8
//====================================================================================================
uint8_t AmfUtil::ReadInt8(void *data)
{
    auto *pt_in = (uint8_t *) data;
    uint8_t number = 0;

    // 파라미터 체크
    if (!data) { return 0; }

    // 읽기
    number |= pt_in[0];

    return number;
}

//====================================================================================================
// AmfUtil - ReadInt16
//====================================================================================================
uint16_t AmfUtil::ReadInt16(void *data)
{
    auto *pt_in = (uint8_t *) data;
    uint16_t number = 0;

    // 파라미터 체크
    if (!data) { return 0; }

    // 읽기
    number |= pt_in[0] << 8;
    number |= pt_in[1];

    return number;
}

//====================================================================================================
// AmfUtil - ReadInt24
//====================================================================================================
uint32_t AmfUtil::ReadInt24(void *data)
{
    auto *pt_in = (uint8_t *) data;
    uint32_t number = 0;

    // 파라미터 체크
    if (!data) { return 0; }

    // 읽기
    number |= pt_in[0] << 16;
    number |= pt_in[1] << 8;
    number |= pt_in[2];

    return number;
}

//====================================================================================================
// AmfUtil - ReadInt32
//====================================================================================================
uint32_t AmfUtil::ReadInt32(void *data)
{
    auto *pt_in = (uint8_t *) data;
    uint32_t number = 0;

    // 파라미터 체크
    if (!data) { return 0; }

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
int AmfUtil::EncodeNumber(void *data, double number)
{
    auto *pt_in = (uint8_t *) &number;
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // marker 기록
    pt_out += WriteInt8(pt_out, (int) AmfTypeMarker::Number);

    // 데이터 기록
    pt_out[0] = pt_in[7];
    pt_out[1] = pt_in[6];
    pt_out[2] = pt_in[5];
    pt_out[3] = pt_in[4];
    pt_out[4] = pt_in[3];
    pt_out[5] = pt_in[2];
    pt_out[6] = pt_in[1];
    pt_out[7] = pt_in[0];

    return (1 + 8);
}

//====================================================================================================
// AmfUtil - WriteInt8
//====================================================================================================
int AmfUtil::EncodeBoolean(void *data, bool boolean)
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // marker 기록
    pt_out += WriteInt8(pt_out, (int) AmfTypeMarker::Boolean);

    // 데이터 기록
    pt_out[0] = (boolean ? 1 : 0);

    return (1 + 1);
}

//====================================================================================================
// AmfUtil - EncodeString
//====================================================================================================
int AmfUtil::EncodeString(void *data, char *string)
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data || !string) { return 0; }

    // marker 기록
    pt_out += WriteInt8(pt_out, (int) AmfTypeMarker::String);

    // 데이터 기록
    pt_out += WriteInt16(pt_out, (uint16_t) strlen(string));
    strncpy((char *) pt_out, string, strlen(string));

    return (1 + 2 + (int) strlen(string));
}

//====================================================================================================
// AmfUtil - DecodeNumber
//====================================================================================================
int AmfUtil::DecodeNumber(void *data, double *number)
{
    auto *pt_in = (uint8_t *) data;
    auto *pt_out = (uint8_t *) number;

    // 파라미터 체크
    if (!data || !number) { return 0; }

    // marker 체크
    if (ReadInt8(pt_in) != (int) AmfTypeMarker::Number) { return 0; }
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

    return (1 + 8);
}

//====================================================================================================
// AmfUtil - DecodeBoolean
//====================================================================================================
int AmfUtil::DecodeBoolean(void *data, bool *boolean)
{
    auto *pt_in = (uint8_t *) data;

    // 파라미터 체크
    if (!data || !boolean) { return 0; }

    // marker 체크
    if (ReadInt8(pt_in) != (int) AmfTypeMarker::Boolean) { return 0; }
    pt_in++;

    // 데이터 읽기
    *boolean = pt_in[0];

    return (1 + 1);
}

//====================================================================================================
// AmfUtil - DecodeString
//====================================================================================================
int AmfUtil::DecodeString(void *data, char *string)
{
    auto *pt_in = (uint8_t *) data;
    int str_len;

    // 파라미터 체크
    if (!data || !string) { return 0; }

    // marker 체크
    if (ReadInt8(pt_in) != (int) AmfTypeMarker::String) { return 0; }
    pt_in++;

    // 데이터 읽기
    str_len = (int) ReadInt16(pt_in);
    pt_in += 2;
    //
    strncpy(string, (char *) pt_in, str_len);
    string[str_len] = '\0';

    return (1 + 2 + str_len);
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
AmfProperty::AmfProperty(AmfDataType type)
{
    // 초기화
    _Initialize();

    // 설정
    _amf_data_type = type;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(double number)
{
    // 초기화
    _Initialize();

    // 설정
    _amf_data_type = AmfDataType::Number;
    _number = number;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(bool boolean)
{
    // 초기화
    _Initialize();

    // 설정
    _amf_data_type = AmfDataType::Boolean;
    _boolean = boolean;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(const char *string) // 스트링은 내부에서 메모리 할당해서 복사
{
    // 초기화
    _Initialize();

    // 파라미터 체크
    if (!string) { return; }

    // 설정
    _string = new char[strlen(string) + 1];
    strcpy(_string, string);
    _amf_data_type = AmfDataType::String;
}

//====================================================================================================
// AmfProperty - AmfProperty
//====================================================================================================
AmfProperty::AmfProperty(AmfArray *pArray) // array 는 파라미터 포인터를 그대로 저장
{
    // 초기화
    _Initialize();

    // 파라미터 체크
    if (!pArray) { return; }

    // 설정
    _amf_data_type = AmfDataType::Array;
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
    if (!pObject) { return; }

    // 설정
    _amf_data_type = AmfDataType::Object;
    _object = pObject;
}

//====================================================================================================
// AmfProperty - ~AmfProperty
//====================================================================================================
AmfProperty::~AmfProperty()
{
    // 메모리 해제
    if (_string)
    {
        delete [] _string;
        _string = nullptr;
    }

    if (_array)
    {
        delete _array;
        _array = nullptr;
    }

    if (_object)
    {
        delete _object;
        _object = nullptr;
    }
}

//====================================================================================================
// AmfProperty - _Initialize
//====================================================================================================
void AmfProperty::_Initialize()
{
    // 초기화
    _amf_data_type = AmfDataType::Null;
    _number = 0.0;
    _boolean = true;
    _string = nullptr;
    _array = nullptr;
    _object = nullptr;
}

//====================================================================================================
// AmfProperty - Dump
//====================================================================================================
void AmfProperty::Dump(std::string &dump_string)
{
    char text[1024] = {0,};
    switch (GetType())
    {
        case AmfDataType::Null:
            dump_string += ("nullptr\n");
            break;
        case AmfDataType::Undefined:
            dump_string += ("Undefined\n");
            break;
        case AmfDataType::Number:
            sprintf(text, "%.1f\n", GetNumber());
            dump_string += text;
            break;
        case AmfDataType::Boolean:
            sprintf(text, "%d\n", GetBoolean());
            dump_string += text;
            break;
        case AmfDataType::String:
            if (_string)
            {
                sprintf(text, "%s\n", strlen(GetString()) ? GetString() : "SIZE-ZERO STRING");
                dump_string += text;
            }
            break;
        case AmfDataType::Array:
            if (_array)
            {
                _array->Dump(dump_string);
            }
            break;
        case AmfDataType::Object:
            if (_object)
            {
                _object->Dump(dump_string);
            }
            break;
    }
}

//====================================================================================================
// AmfProperty - Encode
//====================================================================================================
int AmfProperty::Encode(void *data) // ret=0이면 실패
{
    auto *pt_out = (uint8_t *) data;

    // 파라미터 체크
    if (!data) { return 0; }

    // 타입에 따라 인코딩
    switch (_amf_data_type)
    {
        case AmfDataType::Null:
            pt_out += WriteInt8(pt_out, (int) AmfTypeMarker::Null);
            break;
        case AmfDataType::Undefined:
            pt_out += WriteInt8(pt_out, (int) AmfTypeMarker::Undefined);
            break;
        case AmfDataType::Number:
            pt_out += EncodeNumber(pt_out, _number);
            break;
        case AmfDataType::Boolean:
            pt_out += EncodeBoolean(pt_out, _boolean);
            break;
        case AmfDataType::String:
            if (!_string) { break; }
            pt_out += EncodeString(pt_out, _string);
            break;
        case AmfDataType::Array:
            if (!_array) { break; }
            pt_out += _array->Encode(pt_out);
            break;
        case AmfDataType::Object:
            if (!_object) { break; }
            pt_out += _object->Encode(pt_out);
            break;
        default:
            break;
    }

    return (int) (pt_out - (uint8_t *) data);
}

//====================================================================================================
// AmfProperty - Decode
//====================================================================================================
int AmfProperty::Decode(void *data, int data_length) // ret=0이면 실패
{
    auto *pt_in = (uint8_t *) data;
    int size = 0;

    // 파라미터 체크
    if (!data) { return 0; }
    if (data_length < 1) { return 0; }

    // 타입 초기화
    _amf_data_type = AmfDataType::Null;

    // 타입에 따라 디코딩
    switch ((AmfTypeMarker) ReadInt8(data))
    {
        case AmfTypeMarker::Null:
            size = 1;
            _amf_data_type = AmfDataType::Null;
            break;
        case AmfTypeMarker::Undefined:
            size = 1;
            _amf_data_type = AmfDataType::Undefined;
            break;
        case AmfTypeMarker::Number:
            if (data_length < (1 + 8)) { break; }
            size = DecodeNumber(pt_in, &_number);
            _amf_data_type = AmfDataType::Number;
            break;
        case AmfTypeMarker::Boolean:
            if (data_length < (1 + 1)) { return 0; }
            size = DecodeBoolean(pt_in, &_boolean);
            _amf_data_type = AmfDataType::Boolean;
            break;
        case AmfTypeMarker::String:
            if (data_length < (1 + 3)) { return 0; }
            if (_string)
            {
                delete [] _string;
                _string = nullptr;
            }
            _string = new char[ReadInt16(pt_in + 1) + 1];
            size = DecodeString(pt_in, _string);
            if (!size)
            {
                delete [] _string;
                _string = nullptr;
            }
            _amf_data_type = AmfDataType::String;
            break;
        case AmfTypeMarker::EcmaArray:
            if (_array)
            {
                delete _array;
                _array = nullptr;
            }
            _array = new AmfArray;
            size = _array->Decode(pt_in, data_length);
            if (!size)
            {
                delete _array;
                _array = nullptr;
            }
            _amf_data_type = AmfDataType::Array;
            break;
        case AmfTypeMarker::Object:
            if (_object)
            {
                delete _object;
                _object = nullptr;
            }
            _object = new AmfObject;
            size = _object->Decode(pt_in, data_length);
            if (!size)
            {
                delete _object;
                _object = nullptr;
            }
            _amf_data_type = AmfDataType::Object;
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
AmfObjectArray::AmfObjectArray(AmfDataType Type)
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
    int i;

    // 해제
    for (i = 0; i < (int) _amf_property_pairs.size(); i++)
    {
        if (_amf_property_pairs[i])
        {
            if (_amf_property_pairs[i]->_property)
            {
                delete _amf_property_pairs[i]->_property;
                _amf_property_pairs[i]->_property = nullptr;
            }
            delete _amf_property_pairs[i];
            _amf_property_pairs[i] = nullptr;
        }
    }
    _amf_property_pairs.clear();
}

//====================================================================================================
// AmfObjectArray - Dump
//====================================================================================================
void AmfObjectArray::Dump(std::string &dump_string)
{
    int i;
    char text[1024] = {0,};

    for (i = 0; i < (int) _amf_property_pairs.size(); i++)
    {
        sprintf(text, "%s : ", _amf_property_pairs[i]->_name);
        dump_string += text;

        if (_amf_property_pairs[i]->_property->GetType() == AmfDataType::Array ||
            _amf_property_pairs[i]->_property->GetType() == AmfDataType::Object)
        {
            dump_string += ("\n");
        }
        _amf_property_pairs[i]->_property->Dump(dump_string);
    }
}

//====================================================================================================
// AmfObjectArray - Encode
//====================================================================================================
int AmfObjectArray::Encode(void *data)
{
    auto *pt_out = (uint8_t *) data;
    uint8_t start_marker;
    uint8_t end_marker;
    int i;

    // marker 설정
    start_marker = (uint8_t) (_amf_data_type == AmfDataType::Object ? AmfTypeMarker::Object : AmfTypeMarker::EcmaArray);
    end_marker = (uint8_t) AmfTypeMarker::ObjectEnd;

    // 기록할 아이템 개수가 0이면 스킵
    if (_amf_property_pairs.empty())
    {
        return 0;
    }

    // 시작 marker 기록
    pt_out += WriteInt8(pt_out, start_marker);

    // array 이면 카운트값 기록
    if (_amf_data_type == AmfDataType::Array)
    {
        pt_out += WriteInt32(pt_out, 0); /* 0=infinite */
    }

    // object 아이템 기록
    for (i = 0; i < (int) _amf_property_pairs.size(); i++)
    {
        tPROPERTY_PAIR *property_pair = nullptr;

        // 아이템 포인터 얻기
        property_pair = _amf_property_pairs[i];
        if (!property_pair) { continue; }

        // property 가 invalid 인지 체크
        //if( property_pair->_property->GetType() == AmfDataType::Null ) { continue; }

        // 문자열 기록
        pt_out += WriteInt16(pt_out, (uint16_t) strlen(property_pair->_name));
        memcpy((void *)pt_out, (const void *)property_pair->_name, strlen(property_pair->_name));
        pt_out += strlen(property_pair->_name);

        // value 기록
        pt_out += property_pair->_property->Encode(pt_out);
    }

    // 끝 marker 기록
    pt_out += WriteInt16(pt_out, 0);
    pt_out += WriteInt8(pt_out, end_marker);

    return (int) (pt_out - (uint8_t *) data);
}

//====================================================================================================
// AmfObjectArray - Decode
//====================================================================================================
int AmfObjectArray::Decode(void *data, int data_length)
{
    auto *pt_in = (uint8_t *) data;
    uint8_t start_marker;
    uint8_t end_marker;

    // marker 설정
    start_marker = (uint8_t) (_amf_data_type == AmfDataType::Object ? AmfTypeMarker::Object : AmfTypeMarker::EcmaArray);
    end_marker = (uint8_t) AmfTypeMarker::ObjectEnd;

    // 시작 marker 확인
    if (ReadInt8(pt_in) != start_marker) { return 0; }
    pt_in++;

    // array 일 경우 count 값 읽어들임
    if (_amf_data_type == AmfDataType::Array)
    {
        pt_in += sizeof(uint32_t);
    }

    // 분석
    while (true)
    {
        tPROPERTY_PAIR *property_pair = nullptr;
        int len;

        // 끝 marker 인가?
        if (ReadInt8(pt_in) == end_marker)
        {
            pt_in++;
            break;
        }

        // _name 길이 읽기
        len = (int) ReadInt16(pt_in);
        pt_in += sizeof(uint16_t);
        if (len == 0)
        {
            continue;
        }

        // pair 생성
        property_pair = new tPROPERTY_PAIR;
        property_pair->_property = new AmfProperty;

        // _name 읽기
        strncpy(property_pair->_name, (char *) pt_in, len);
        pt_in += len;
        property_pair->_name[len] = '\0';

        // value 읽기
        len = property_pair->_property->Decode(pt_in, data_length - (int) (pt_in - (uint8_t *) data));
        pt_in += len;

        // value 체크
        //if( !len || property_pair->_property->GetType() == AmfDataType::Null )
        if (len == 0) {
            delete property_pair->_property;
            property_pair->_property = nullptr;

            delete property_pair;
            property_pair = nullptr;
            return 0;
        }

        // list 에 추가
        _amf_property_pairs.push_back(property_pair);
        property_pair = nullptr;
    }

    return (int) (pt_in - (uint8_t *) data);
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *name, AmfDataType type)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }
    if (type != AmfDataType::Null && type != AmfDataType::Undefined) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty(type);

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *name, double number)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty(number);

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *name, bool boolean)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty(boolean);

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *name, const char *string)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty(string);

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *name, AmfArray *array)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty(array);

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - AddProperty
//====================================================================================================
bool AmfObjectArray::AddProperty(const char *name, AmfObject *object)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty(object);

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - AddNullProperty
//====================================================================================================
bool AmfObjectArray::AddNullProperty(const char *name)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // 파라미터 체크
    if (!name) { return false; }

    // pair 생성
    property_pair = new tPROPERTY_PAIR;
    strcpy(property_pair->_name, name);
    property_pair->_property = new AmfProperty();

    // 저장
    _amf_property_pairs.push_back(property_pair);
    property_pair = nullptr;

    return true;
}

//====================================================================================================
// AmfObjectArray - _GetPair
//====================================================================================================
AmfObject::tPROPERTY_PAIR *AmfObjectArray::_GetPair(int index)
{
    // 범위 체크
    if (index < 0) { return nullptr; }
    if (index >= (int) _amf_property_pairs.size()) { return nullptr; }

    return _amf_property_pairs[index];
}

//====================================================================================================
// AmfObjectArray - FindName
//====================================================================================================
int AmfObjectArray::FindName(const char *name) // ret<0이면 실패
{
    int i;

    // 파라미터 체크
    if (!name) { return -1; }

    // 검색
    for (i = 0; i < (int) _amf_property_pairs.size(); i++)
    {
        if (!strcmp(_amf_property_pairs[i]->_name, name))
        {
            return i;
        }
    }

    return -1;
}

//====================================================================================================
// AmfObjectArray - GetName
//====================================================================================================
char *AmfObjectArray::GetName(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return nullptr; }

    return property_pair->_name;
}

//====================================================================================================
// AmfObjectArray - GetType
//====================================================================================================
AmfDataType AmfObjectArray::GetType(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return AmfDataType::Null; }

    return property_pair->_property->GetType();
}

//====================================================================================================
// AmfObjectArray - GetNumber
//====================================================================================================
double AmfObjectArray::GetNumber(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return 0; }

    return property_pair->_property->GetNumber();
}

//====================================================================================================
// AmfObjectArray - GetBoolean
//====================================================================================================
bool AmfObjectArray::GetBoolean(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return false; }

    return property_pair->_property->GetBoolean();
}

//====================================================================================================
// AmfObjectArray - GetString
//====================================================================================================
char *AmfObjectArray::GetString(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return nullptr; }

    return property_pair->_property->GetString();
}

//====================================================================================================
// AmfObjectArray - GetArray
//====================================================================================================
AmfArray *AmfObjectArray::GetArray(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return nullptr; }

    return property_pair->_property->GetArray();
}

//====================================================================================================
// AmfObjectArray - GetObject
//====================================================================================================
AmfObject *AmfObjectArray::GetObject(int index)
{
    tPROPERTY_PAIR *property_pair = nullptr;

    // pair 얻기
    property_pair = _GetPair(index);
    if (!property_pair) { return nullptr; }

    return property_pair->_property->GetObject();
}



//////////////////////////////////////////////////////////////////////////
//                               AmfArray                              //
//////////////////////////////////////////////////////////////////////////

//====================================================================================================
// AmfArray - AmfObjectArray
//====================================================================================================
AmfArray::AmfArray() : AmfObjectArray(AmfDataType::Array)
{

}

//====================================================================================================
// AmfArray - Dump
//====================================================================================================
void AmfArray::Dump(std::string &dump_string)
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
AmfObject::AmfObject() : AmfObjectArray(AmfDataType::Object)
{

}

//====================================================================================================
// AmfObject - Dump
//====================================================================================================
void AmfObject::Dump(std::string &dump_string)
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
AmfDocument::AmfDocument()
{
    // 초기화
    _amf_propertys.clear();
}

//====================================================================================================
// AmfDocument - ~AmfDocument
//====================================================================================================
AmfDocument::~AmfDocument()
{
    int i;

    // 해제
    for (i = 0; i < (int) _amf_propertys.size(); i++)
    {
        if (_amf_propertys[i])
        {
            delete _amf_propertys[i];
            _amf_propertys[i] = nullptr;
        }
    }
    _amf_propertys.clear();
}

//====================================================================================================
// AmfDocument - Dump
//====================================================================================================
void AmfDocument::Dump(std::string &dump_string)
{
    int i;

    // 해제
    dump_string += ("\n======= DUMP START =======\n");
    for (i = 0; i < (int) _amf_propertys.size(); i++)
    {
        _amf_propertys[i]->Dump(dump_string);
    }
    dump_string += ("\n======= DUMP END =======\n");
}

//====================================================================================================
// AmfDocument - Encode
//====================================================================================================
int AmfDocument::Encode(void *data) // ret=0이면 실패
{
    auto *pt_out = (uint8_t *) data;
    int i;

    // 인코딩
    for (i = 0; i < (int) _amf_propertys.size(); i++)
    {
        pt_out += _amf_propertys[i]->Encode(pt_out);
    }

    return (int) (pt_out - (uint8_t *) data);
}

//====================================================================================================
// AmfDocument - Decode
//====================================================================================================
int AmfDocument::Decode(void *data, int data_length) // ret=0이면 실패
{
    auto *pt_in = (uint8_t *) data;
    int total_len = 0;
    int ret_len;

    // 디코딩 시작
    while (total_len < data_length)
    {
        AmfProperty *pt_item = nullptr;

        // 새 property 할당
        pt_item = new AmfProperty;

        // 디코딩
        ret_len = pt_item->Decode(pt_in, data_length - total_len);
        if (!ret_len)
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

    return (int) (pt_in - (uint8_t *) data);
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(AmfDataType type)
{
    // 파라미터 체크
    if (type != AmfDataType::Null && type != AmfDataType::Undefined) { return false; }

    // 추가
    _amf_propertys.push_back(new AmfProperty(type));

    return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(double number)
{
    // 추가
    _amf_propertys.push_back(new AmfProperty(number));

    return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(bool boolean)
{
    // 추가
    _amf_propertys.push_back(new AmfProperty(boolean));

    return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(const char *string)
{
    // 파라미터 체크
    if (!string) { return false; }

    // 추가
    _amf_propertys.push_back(new AmfProperty(string));

    return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(AmfArray *array)
{
    // 파라미터 체크
    if (!array) { return false; }

    // 추가
    _amf_propertys.push_back(new AmfProperty(array));

    return true;
}

//====================================================================================================
// AmfDocument - AddProperty
//====================================================================================================
bool AmfDocument::AddProperty(AmfObject *object)
{
    // 파라미터 체크
    if (!object) { return false; }

    // 추가
    _amf_propertys.push_back(new AmfProperty(object));

    return true;
}

//====================================================================================================
// AmfDocument - GetPropertyCount
//====================================================================================================
int AmfDocument::GetPropertyCount()
{
    return (int) _amf_propertys.size();
}

//====================================================================================================
// AmfDocument - GetPropertyIndex
//====================================================================================================
int AmfDocument::GetPropertyIndex(char *name)
{
    int i;

    // 파라미터 체크
    if (!name)
    {
        return -1;
    }

    if (_amf_propertys.empty())
    {
        return -1;
    }

    // 찾기
    for (i = 0; i < (int) _amf_propertys.size(); i++)
    {
        if (_amf_propertys[i]->GetType() != AmfDataType::String) { continue; }
        if (strcmp(_amf_propertys[i]->GetString(), name)) { continue; }

        return i;
    }

    return -1;
}

//====================================================================================================
// AmfDocument - GetProperty
//====================================================================================================
AmfProperty *AmfDocument::GetProperty(int index)
{
    // 파라미터 체크
    if (index < 0) { return nullptr; }
    if (index >= (int) _amf_propertys.size()) { return nullptr; }

    return _amf_propertys[index];
}