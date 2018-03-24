
#pragma once

class CWeenieFactory
{
public:
	CWeenieFactory();
	virtual ~CWeenieFactory();

	void Reset();
	void Initialize();

	CWeenieObject *CreateWeenieByClassID(DWORD wcid, const Position *pos = NULL, bool bSpawn = false);
	CWeenieObject *CreateWeenieByName(const char *name, const Position *pos = NULL, bool bSpawn = false);
	CWeenieObject *CloneWeenie(CWeenieObject *weenie);

	bool ApplyWeenieDefaults(CWeenieObject *weenie, DWORD wcid);

	DWORD GetWCIDByName(const char *name, int index = 0);

	CWeenieDefaults *GetWeenieDefaults(DWORD wcid);
	CWeenieDefaults *GetWeenieDefaults(const char *name, int index = 0);
	
	bool TryToResolveAppearanceData(CWeenieObject *weenie);

	CWeenieObject *CreateBaseWeenieByType(int weenieType, unsigned int wcid, const char *weenieName = "");

	DWORD GetScrollSpellForWCID(DWORD wcid);
	DWORD GetWCIDForScrollSpell(DWORD spell_id);

	void RefreshLocalStorage();
	
	std::list<DWORD> GetWCIDsWithMotionTable(DWORD mtable);

	inline DWORD GetFirstAvatarWCID() { return _firstAvatarWCID; }
	inline DWORD GetNumAvatars() { return _numAvatars; }

protected:
	CWeenieObject *CreateWeenie(CWeenieDefaults *defaults, const Position *pos = NULL, bool bSpawn = false);

	void ApplyWeenieDefaults(CWeenieObject *weenie, CWeenieDefaults *defaults);

	void LoadLocalStorage(bool refresh = false);
	void LoadLocalStorageIndexed();
	void LoadAvatarData();

	void MapScrollWCIDs();

	std::unordered_map<DWORD, CWeenieDefaults *> _weenieDefaults;
	std::multimap<std::string, CWeenieDefaults *> _weenieDefaultsByName;
	std::unordered_map<DWORD, DWORD> _scrollWeenies; // keyed by spell ID
	DWORD _firstAvatarWCID = 0;
	DWORD _numAvatars = 0;
};

