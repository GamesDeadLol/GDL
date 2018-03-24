
#pragma once

class CGarbageCollector
{
public:
	CGarbageCollector();
	~CGarbageCollector();

	void Tick();

	void AddByteArrayForCleanup(BYTE *data);

	TLockable<std::vector<BYTE *>> _byteArraysToDelete;
};
