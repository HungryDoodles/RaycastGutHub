// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
//#include "RayGameStateBase.h"

DECLARE_STATS_GROUP(TEXT("LagCompensation"), STATGROUP_LagCompensation, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Compensate"), STAT_LagCompensate, STATGROUP_LagCompensation, RAYCAST_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Decompensate"), STAT_LagDecompensate, STATGROUP_LagCompensation, RAYCAST_API);

/** Provides lag compensation interface-like class with autmatic registration
 * 
 */
class RAYCAST_API FLagCompensateable
{
	static TArray<FLagCompensateable*> compensateables;

	// Is this compensateable has been compensated?
	bool bIsCompensated = false;
public:

	FLagCompensateable() 
	{
		//check(!compensateables.Contains(this));
		compensateables.Add(this);
	}
	virtual ~FLagCompensateable() 
	{
		const int32 pos = compensateables.Find(this);
		//check(pos != INDEX_NONE);
		if (pos >= 0)
			compensateables[pos] = nullptr;
	}

	bool IsCompensated() { return bIsCompensated; }

	// Rewind given seconds into past
	virtual void CompensateSeconds(float amount) = 0;
	// Rewind of rewind lol
	virtual void RevertCompensation() = 0;
	// Can this object be compensated? 
	virtual bool AllowCompensation() = 0;
	// Get world this object is working from, used to filter compensateables from different UWorlds
	virtual UWorld * GetCompensateableWorld() = 0;

	// Rewind time for every registered compensateable
	static void Compensate(float amount, UWorld * world);
	// Revert rewinding of the time for every registered compensateable
	static void Decompensate(UWorld * world);
};

/*class FScopedLagCompensation 
{
	ARayGameStateBase * gameState;
public:
	FScopedLagCompensation(ARayGameStateBase * rayGameState, float seconds)
	{
		gameState = rayGameState;
		gameState->CompensateLag(seconds);
	}
	~FScopedLagCompensation() 
	{
		gameState->DecompensateLag();
	}
};*/

template <typename T>
class RAYCAST_API TCompensationDataMemory
{
	TArray<TTuple<float, T>> savedData;
public:
	float maxMemoryTimeSeconds = 1.f;

	void CleanUp() 
	{
		if (savedData.Num() == 0)
			return;

		float elimTime = savedData[0].Get<0>() - maxMemoryTimeSeconds;
		
		savedData.RemoveAll([elimTime](const TTuple<float, T>& data)->bool { return data.Get<0>() < elimTime; });
	}

	template <typename T>
	void Save(T data, float timePoint) 
	{
		if (savedData.Num() == 0)
			savedData.Add(TTuple<float, T>( timePoint, data ));
		else if (savedData[0].Get<0>() < timePoint)
			savedData.Insert(TTuple<float, T>(timePoint, data), 0);

		CleanUp();
	}
	
	template <typename T>
	T Get(float second) 
	{
		if (savedData.Num() == 0)
			return T();
		else if (savedData.Num() == 1)
			return savedData[0].Get<1>();

		if (savedData[0].Get<0>() <= second)
			return savedData[0].Get<1>();
		else if (savedData.Last().Get<0>() > second)
			return savedData.Last().Get<1>();

		TTuple<float, T> * foundData = savedData.FindByPredicate([second](const TTuple<float, T>& data)->bool { return data.Get<0>() < second; });
		
		if (foundData == nullptr) // Outdated second (too high ping?)
			return savedData.Last().Get<1>();

		// (Try) Interpolate
		float alpha = (second - (*foundData).Get<0>()) / ((*(foundData - 1)).Get<0>() - (*foundData).Get<0>());

		return FMath::Lerp( (*(foundData)).Get<1>(), (*(foundData - 1)).Get<1>(), alpha);
	}
};