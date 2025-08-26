#pragma once

#include <GeneralStructures.h>
#include <HouseClass.h>

#include <New/Type/LaserTrailTypeClass.h>


class LaserTrailClass
{
public:
	LaserTrailTypeClass* Type;
	bool Visible;
	bool Cloaked;
	CoordStruct FLH;
	bool IsOnTurret;
	ColorStruct CurrentColor;
	Nullable<CoordStruct> LastLocation;
	bool Intrinsic;

	LaserTrailClass(LaserTrailTypeClass* pTrailType, HouseClass* pHouse = nullptr,
		CoordStruct flh = { 0, 0, 0 }, bool isOnTurret = false) :
		Type { pTrailType }
		, Visible { true }
		, Cloaked { false }
		, FLH { flh }
		, IsOnTurret { isOnTurret }
		, CurrentColor { pTrailType->Color }
		, LastLocation {}
		, Intrinsic { true }
	{
		if (this->Type->IsHouseColor && pHouse)
			this->CurrentColor = pHouse->LaserColor;
	}

	LaserTrailClass() :
		Type {},
		Visible {},
		Cloaked {},
		FLH {},
		IsOnTurret {},
		CurrentColor {},
		LastLocation {},
		Intrinsic {}
	{ }

	bool Update(CoordStruct location);

	bool Load(PhobosStreamReader& stm, bool registerForChange);
	bool Save(PhobosStreamWriter& stm) const;

private:
	template <typename T>
	bool Serialize(T& stm);

	// Fast helpers (header-only to enable inlining on old compilers)
	static inline long long DistSq3D(const CoordStruct& a, const CoordStruct& b)
	{
		const int dx = a.X - b.X;
		const int dy = a.Y - b.Y;
		const int dz = a.Z - b.Z;
		return 1LL * dx * dx + 1LL * dy * dy + 1LL * dz * dz;
	}

	static inline int clampInt(int v, int lo, int hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}
};
