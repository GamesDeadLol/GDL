
#pragma once

#include "WeenieObject.h"

class CMissileWeenie : public CWeenieObject
{
public:
	CMissileWeenie();
	virtual ~CMissileWeenie() override;

	virtual class CMissileWeenie *AsMissile() { return this; }

	virtual void ApplyQualityOverrides() override;

protected:
};

