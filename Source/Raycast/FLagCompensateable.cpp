// Fill out your copyright notice in the Description page of Project Settings.

#include "FLagCompensateable.h"

DEFINE_STAT(STAT_LagCompensate);
DEFINE_STAT(STAT_LagDecompensate);

TArray<FLagCompensateable*> FLagCompensateable::compensateables;

void FLagCompensateable::Compensate(float amount, UWorld * world)
{
	bool bNeedsCleanup = false;

	SCOPE_CYCLE_COUNTER(STAT_LagCompensate);

	for (int i = 0; i < compensateables.Num(); ++i)
	{
		if (FLagCompensateable * compensateable = compensateables[i])
		{
			if (compensateable->AllowCompensation() &&
				(compensateable->GetCompensateableWorld() == world || world == nullptr) &&
				!compensateable->bIsCompensated)
			{
				compensateable->CompensateSeconds(amount);
				compensateable->bIsCompensated = true;
			}
		}
		else
		{
			bNeedsCleanup = true;
		}
	}

	if (bNeedsCleanup)
	{
		compensateables.RemoveAll([](FLagCompensateable * compensateable)->bool { return compensateable == nullptr; });
	}
}

void FLagCompensateable::Decompensate(UWorld * world)
{
	bool bNeedsCleanup = false;

	SCOPE_CYCLE_COUNTER(STAT_LagDecompensate);

	for (int i = 0; i < compensateables.Num(); ++i)
	{
		if (FLagCompensateable * compensateable = compensateables[i])
		{
			if (compensateable->AllowCompensation() &&
				(compensateable->GetCompensateableWorld() == world || world == nullptr) &&
				compensateable->bIsCompensated)
			{
				compensateable->RevertCompensation();
				compensateable->bIsCompensated = false;
			}
		}
		else
		{
			bNeedsCleanup = true;
		}
	}

	if (bNeedsCleanup)
	{
		compensateables.RemoveAll([](FLagCompensateable * compensateable)->bool { return compensateable == nullptr; });
	}
}