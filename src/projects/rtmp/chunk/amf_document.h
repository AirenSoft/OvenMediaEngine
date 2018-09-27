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

typedef enum
{
	AMF_NULL = 0,
	AMF_UNDEFINED,
	AMF_NUMBER,
	AMF_BOOLEAN,
	AMF_STRING,
	AMF_ARRAY,
	AMF_OBJECT,
} tAMF_DATA_TYPE;

class AmfObjectArray;
class AmfObject;
class AmfArray;
class AmfProperty;
class AmfDocument;

//====================================================================================================
// AmfUtil
//====================================================================================================
class AmfUtil
{
public:
	AmfUtil( ) = default ;
	virtual ~AmfUtil( ) = default;

public:
	// marker 제외 숫자 읽기/쓰기 (Big Endian)
	static	int			WriteInt8(void * data, uint8_t Number);
	static	int			WriteInt16(void * data, uint16_t Number);
	static	int			WriteInt24(void * data, uint32_t Number);
	static	int			WriteInt32(void * data, uint32_t Number);

	static	uint8_t		ReadInt8(void * data);
	static	uint16_t	ReadInt16(void * data);
	static	uint32_t	ReadInt24(void * data);
	static	uint32_t	ReadInt32(void * data);

public:
	// marker 포함 ENC/DEC
	static	int			EncodeNumber(void * data, double Number);
	static	int			EncodeBoolean(void * data, bool Boolean);
	static	int			EncodeString(void * data, char *  pString);

	static	int			DecodeNumber(void * data, double *pNumber);
	static	int			DecodeBoolean(void * data, bool *pBoolean);
	static	int			DecodeString(void * data, char *  pString);
};

//====================================================================================================
// AmfProperty
//====================================================================================================
class AmfProperty : public AmfUtil
{
public:
	AmfProperty( );
	explicit		AmfProperty(tAMF_DATA_TYPE Type);
	explicit		AmfProperty(double Number);
	explicit		AmfProperty(bool Boolean);
	explicit		AmfProperty(const char *  pString); 	// 스트링은 내부에서 메모리 할당해서 복사
	explicit		AmfProperty(AmfArray *pArray); 			// array 는 파라미터 포인터를 그대로 저장
	explicit		AmfProperty(AmfObject *pObject); 		// object 는 파라미터 포인터를 그대로 저장
	 ~AmfProperty( ) override;

public:
	void			Dump(std::string &dump_string);

public:
	int				Encode(void * data); // ret=0이면 실패, type -> packet
	int				Decode(void * data, int DataLen); // ret=0이면 실패, packet -> type

public:
	tAMF_DATA_TYPE	GetType( )		{ return _amf_data_type; }
	double			GetNumber( )	{ return _number; }
	bool			GetBoolean( )	{ return _boolean; }
	char * 			GetString( )	{ return _string; }
	AmfArray*		GetArray( )		{ return _array; }
	AmfObject*		GetObject( )	{ return _object; }

private:
	void			_Initialize( );

private:
	tAMF_DATA_TYPE	_amf_data_type;
	double			_number;
	bool			_boolean;
	char * 			_string;
	AmfArray*		_array;
	AmfObject*		_object;
};

//====================================================================================================
// AmfObjectArray
//====================================================================================================
class AmfObjectArray : AmfUtil
{
public:
    explicit AmfObjectArray(tAMF_DATA_TYPE Type);
    ~AmfObjectArray( ) override;

public:
	typedef struct sPROPERTY_PAIR
	{
		char			_name[128];
		AmfProperty	*	_property;
	} tPROPERTY_PAIR; // new, delete

public:
	virtual void    Dump(std::string & dump_string);

public:
	int				Encode(void * data); // ret=0이면 실패, type -> packet
	int				Decode(void * data, int DataLen); // ret=0이면 실패, packet -> type

public:
	bool			AddProperty(const char *  pName, tAMF_DATA_TYPE Type);
	bool			AddProperty(const char *  pName, double Number);
	bool			AddProperty(const char *  pName, bool Boolean);
	bool			AddProperty(const char *  pName, const char *  pString);
	bool			AddProperty(const char *  pName, AmfArray *pArray);
	bool			AddProperty(const char *  pName, AmfObject *pObject);
	bool			AddNullProperty(const char *  pName);

public:
	int				FindName(const char *  pName); // ret<0이면 실패
	char * 			GetName(int Index);

	tAMF_DATA_TYPE	GetType(int Index);
	double			GetNumber(int Index);
	bool			GetBoolean(int Index);
	char * 			GetString(int Index);
	AmfArray*		GetArray(int Index);
	AmfObject*		GetObject(int Index);

private:
	tPROPERTY_PAIR*	_GetPair(int Index);

private:
	tAMF_DATA_TYPE					_amf_data_type;
	std::vector<tPROPERTY_PAIR*>	_amf_property_pairs;
};


//====================================================================================================
// AmfArray
//====================================================================================================
class AmfArray : public AmfObjectArray
{
public:
   AmfArray( );
   ~AmfArray( ) override = default;

public:
	void Dump(std::string &dump_string) final;
};

//====================================================================================================
// AmfObject
//====================================================================================================
class AmfObject : public AmfObjectArray
{
public:
	AmfObject( );
	~AmfObject( ) override = default;

public:
	void Dump(std::string &dump_string) final;
};

//====================================================================================================
// AmfDocument
//====================================================================================================
class AmfDocument : AmfUtil
{
public:
    AmfDocument( );
    ~AmfDocument( ) override;

public:
	void			Dump(std::string &dump_string);

public:
	int				Encode(void * data); 					// ret=0이면 실패
	int				Decode(void * data, int DataLen); 		// ret=0이면 실패

public:
	bool			AddProperty(tAMF_DATA_TYPE Type);
	bool			AddProperty(double Number);
	bool			AddProperty(bool Boolean);
	bool			AddProperty(const char *  pString); 	// 스트링은 내부에서 메모리 할당해서 복사
	bool			AddProperty(AmfArray *pArray); 			// array는 파라미터 포인터를 그대로 저장
	bool			AddProperty(AmfObject *pObject); 		// object는 파라미터 포인터를 그대로 저장

public:
	int				GetPropertyCount( );
	int				GetPropertyIndex(char *  pName); 		// 실패하면 -1
	AmfProperty*	GetProperty(int Index);

private:
	std::vector<AmfProperty*>	_amf_propertys;
};
 

