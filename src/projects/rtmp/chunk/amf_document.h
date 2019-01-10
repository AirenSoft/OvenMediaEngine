//==============================================================================
//
//  RtmpProvider
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <string.h>
#include <vector>
#include <string>

enum class AmfDataType : int32_t {
    Null = 0,
    Undefined,
    Number,
    Boolean,
    String,
    Array,
    Object,
};

enum class AmfTypeMarker : int32_t {
    Number = 0,
    Boolean,
    String,
    Object,
    MovieClip,
    Null,
    Undefined,
    Reference,
    EcmaArray,
    ObjectEnd,
    StrictArray,
    Date,
    LongString,
    Unsupported,
    Recordset,
    Xml,
    TypedObject,
};

class AmfObjectArray;

class AmfObject;

class AmfArray;

class AmfProperty;

class AmfDocument;

//====================================================================================================
// AmfUtil
//====================================================================================================
class AmfUtil {
public:
    AmfUtil() = default;

    virtual ~AmfUtil() = default;

public:
    // marker 제외 숫자 읽기/쓰기 (Big Endian)
    static int WriteInt8(void *data, uint8_t number);

    static int WriteInt16(void *data, uint16_t number);

    static int WriteInt24(void *data, uint32_t number);

    static int WriteInt32(void *data, uint32_t number);

    static uint8_t ReadInt8(void *data);

    static uint16_t ReadInt16(void *data);

    static uint32_t ReadInt24(void *data);

    static uint32_t ReadInt32(void *data);

public:
    // marker 포함 ENC/DEC
    static int EncodeNumber(void *data, double number);

    static int EncodeBoolean(void *data, bool boolean);

    static int EncodeString(void *data, char *string);

    static int DecodeNumber(void *data, double *number);

    static int DecodeBoolean(void *data, bool *boolean);

    static int DecodeString(void *data, char *string);
};

//====================================================================================================
// AmfProperty
//====================================================================================================
class AmfProperty : public AmfUtil {
public:
    AmfProperty();

    explicit AmfProperty(AmfDataType type);

    explicit AmfProperty(double number);

    explicit AmfProperty(bool boolean);

    explicit AmfProperty(const char *string);    // 스트링은 내부에서 메모리 할당해서 복사
    explicit AmfProperty(AmfArray *array);            // array 는 파라미터 포인터를 그대로 저장
    explicit AmfProperty(AmfObject *object);        // object 는 파라미터 포인터를 그대로 저장
    ~AmfProperty() override;

public:
    void Dump(std::string &dump_string);

public:
    int Encode(void *data); // ret=0이면 실패, type -> packet
    int Decode(void *data, int data_length); // ret=0이면 실패, packet -> type

public:
    AmfDataType GetType() { return _amf_data_type; }

    double GetNumber() { return _number; }

    bool GetBoolean() { return _boolean; }

    char *GetString() { return _string; }

    AmfArray *GetArray() { return _array; }

    AmfObject *GetObject() { return _object; }

private:
    void _Initialize();

private:
    AmfDataType _amf_data_type;
    double _number;
    bool _boolean;
    char *_string;
    AmfArray *_array;
    AmfObject *_object;
};

//====================================================================================================
// AmfObjectArray
//====================================================================================================
class AmfObjectArray : AmfUtil {
public:
    explicit AmfObjectArray(AmfDataType type);

    ~AmfObjectArray() override;

public:
    typedef struct sPROPERTY_PAIR {
        char _name[128];
        AmfProperty *_property;
    } tPROPERTY_PAIR; // new, delete

public:
    virtual void Dump(std::string &dump_string);

public:
    int Encode(void *data); // ret=0이면 실패, type -> packet
    int Decode(void *data, int data_length); // ret=0이면 실패, packet -> type

public:
    bool AddProperty(const char *name, AmfDataType type);

    bool AddProperty(const char *name, double number);

    bool AddProperty(const char *name, bool boolean);

    bool AddProperty(const char *name, const char *string);

    bool AddProperty(const char *name, AmfArray *array);

    bool AddProperty(const char *name, AmfObject *object);

    bool AddNullProperty(const char *name);

public:
    int FindName(const char *name); // ret<0이면 실패
    char *GetName(int index);

    AmfDataType GetType(int index);

    double GetNumber(int index);

    bool GetBoolean(int index);

    char *GetString(int index);

    AmfArray *GetArray(int index);

    AmfObject *GetObject(int index);

private:
    tPROPERTY_PAIR *_GetPair(int index);

private:
    AmfDataType _amf_data_type;
    std::vector<tPROPERTY_PAIR *> _amf_property_pairs;
};


//====================================================================================================
// AmfArray
//====================================================================================================
class AmfArray : public AmfObjectArray {
public:
    AmfArray();

    ~AmfArray() override = default;

public:
    void Dump(std::string &dump_string) final;
};

//====================================================================================================
// AmfObject
//====================================================================================================
class AmfObject : public AmfObjectArray {
public:
    AmfObject();

    ~AmfObject() override = default;

public:
    void Dump(std::string &dump_string) final;
};

//====================================================================================================
// AmfDocument
//====================================================================================================
class AmfDocument : AmfUtil {
public:
    AmfDocument();

    ~AmfDocument() override;

public:
    void Dump(std::string &dump_string);

public:
    int Encode(void *data);                    // ret=0이면 실패
    int Decode(void *data, int data_length);        // ret=0이면 실패

public:
    bool AddProperty(AmfDataType type);

    bool AddProperty(double number);

    bool AddProperty(bool boolean);

    bool AddProperty(const char *string);    // 스트링은 내부에서 메모리 할당해서 복사
    bool AddProperty(AmfArray *array);            // array는 파라미터 포인터를 그대로 저장
    bool AddProperty(AmfObject *object);        // object는 파라미터 포인터를 그대로 저장

public:
    int GetPropertyCount();

    int GetPropertyIndex(char *name);        // 실패하면 -1
    AmfProperty *GetProperty(int index);

private:
    std::vector<AmfProperty *> _amf_propertys;
};
 

