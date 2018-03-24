
#pragma once

template<typename TStatType, typename TDataType>
class TYPEMod : public PackObj
{
public:
	void Pack(BinaryWriter *pWriter)
	{
		UNFINISHED();
	}

	template<typename TStatType, typename TDataType>
	bool UnPackInternal(BinaryReader *pReader)
	{
		_unk = pReader->Read<int>();
		_someEnum = pReader->Read<int>();
		_stat = (TStatType)pReader->Read<int>();
		_value = pReader->Read<TDataType>();
		return true;
	}

	virtual bool UnPack(BinaryReader *pReader) override
	{
		return UnPackInternal<TStatType, TDataType>(pReader);
	}

	int _unk;
	int _someEnum;
	TStatType _stat;
	TDataType _value;
};

template<typename TStatType, typename TDataType>
class TYPERequirement : public PackObj
{
public:
	virtual void Pack(BinaryWriter *pWriter) override
	{ 
		UNFINISHED();
	}

	template<typename TStatType, typename TDataType>
	bool UnPackInternal(BinaryReader *pReader)
	{
		_stat = (TStatType)pReader->Read<int>();
		_value = pReader->Read<TDataType>();
		_someEnum = pReader->Read<int>();
		_someString = pReader->ReadString();
		return true;
	}

	virtual bool UnPack(BinaryReader *pReader) override
	{
		return UnPackInternal<TStatType, TDataType>(pReader);
	}

	TStatType _stat;
	TDataType _value;
	int _someEnum;
	std::string _someString;
};

class CraftRequirements
{
public:
	bool UnPack(BinaryReader *pReader)
	{
		_intRequirement.UnPack(pReader);
		_didRequirement.UnPack(pReader);
		_iidRequirement.UnPack(pReader);
		_floatRequirement.UnPack(pReader);
		_stringRequirement.UnPack(pReader);
		_boolRequirement.UnPack(pReader);
		return true;
	}

	PackableList<TYPERequirement<STypeInt, int>> _intRequirement;
	PackableList<TYPERequirement<STypeDID, DWORD>> _didRequirement;
	PackableList<TYPERequirement<STypeIID, DWORD>> _iidRequirement;
	PackableList<TYPERequirement<STypeFloat, double>> _floatRequirement;
	PackableList<TYPERequirement<STypeString, std::string>> _stringRequirement;
	PackableList<TYPERequirement<STypeBool, BOOL>> _boolRequirement;
};

class CraftMods
{
public:
	bool UnPack(BinaryReader *pReader)
	{
		_intMod.UnPack(pReader);
		_didMod.UnPack(pReader);
		_iidMod.UnPack(pReader);
		_floatMod.UnPack(pReader);
		_stringMod.UnPack(pReader);
		_boolMod.UnPack(pReader);

		int a = pReader->Read<int>();
		int b = pReader->Read<int>();
		int c = pReader->Read<int>();
		int d = pReader->Read<int>();
		int e = pReader->Read<int>();
		int f = pReader->Read<int>();

		int g = pReader->Read<BOOL>();
		DWORD h = pReader->Read<DWORD>(); // dataID

		int i = pReader->Read<int>();
		DWORD j = pReader->Read<DWORD>(); // instanceID

		/*
		printf("%u %u %u %u %u %u %d %d %d %d %d %d %d %u %d %u\n",
			(DWORD)_intMod.size(), (DWORD)_didMod.size(), (DWORD)_iidMod.size(),
			(DWORD)_floatMod.size(), (DWORD)_stringMod.size(), (DWORD)_boolMod.size(),
			a, b, c, d, e, f, g, h, i, j);
			*/

		return true;
	}

	PackableList<TYPEMod<STypeInt, int>> _intMod;
	PackableList<TYPEMod<STypeDID, DWORD>> _didMod;
	PackableList<TYPEMod<STypeIID, DWORD>> _iidMod;
	PackableList<TYPEMod<STypeFloat, double>> _floatMod;
	PackableList<TYPEMod<STypeString, std::string>> _stringMod;
	PackableList<TYPEMod<STypeBool, BOOL>> _boolMod;
};

class CCraftOperation : public PackObj
{
public:
	DECLARE_PACKABLE()

	DWORD _unk = 0;
	STypeSkill _skill = STypeSkill::UNDEF_SKILL;
	int _difficulty = 0;
	DWORD _unk2 = 0;
	DWORD _successWcid = 0;
	DWORD _successAmount = 0;
	std::string _successMessage;
	DWORD _failWcid = 0;
	DWORD _failAmount = 0;
	std::string _failMessage;

	double _comp1_1;
	int _comp1_2;
	std::string _comp1_str;

	double _comp2_1;
	int _comp2_2;
	std::string _comp2_str;

	double _comp3_1;
	int _comp3_2;
	std::string _comp3_str;

	double _comp4_1;
	int _comp4_2;
	std::string _comp4_str;

	CraftRequirements _requirements[3];
	CraftMods _mods[8];

	DWORD _dataID = 0;
};

class CCraftTable : public PackObj
{
public:
	CCraftTable();
	virtual ~CCraftTable() override;

	DECLARE_PACKABLE()

	PackableHashTable<DWORD, CCraftOperation> _operations;
	PackableHashTable<DWORD64, DWORD, DWORD64> _precursorMap;
};
