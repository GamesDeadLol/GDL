
#include "StdAfx.h"
#include "GarbageCollector.h"

CGarbageCollector::CGarbageCollector()
{
	_byteArraysToDelete.reserve(4000);
}

CGarbageCollector::~CGarbageCollector()
{
	for (BYTE *byteArray : _byteArraysToDelete)
	{
		delete[] byteArray;
	}
}

void CGarbageCollector::Tick()
{
	_byteArraysToDelete.Lock();
	for (BYTE *byteArray : _byteArraysToDelete)
	{
		delete [] byteArray;
	}
	_byteArraysToDelete.clear();
	_byteArraysToDelete.Unlock();
}

void CGarbageCollector::AddByteArrayForCleanup(BYTE *data)
{
	_byteArraysToDelete.Lock();
	_byteArraysToDelete.push_back(data);
	_byteArraysToDelete.Unlock();
}
