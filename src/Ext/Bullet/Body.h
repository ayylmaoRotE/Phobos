#pragma once
#include <BulletClass.h>

#include <Ext/BulletType/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <Utilities/Container.h>
#include <Utilities/TemplateDef.h>
#include <New/Entity/LaserTrailClass.h>
#include "Trajectories/PhobosTrajectory.h"

class BulletExt
{
public:
	using base_type = BulletClass;

	static constexpr DWORD Canary = 0x2A2A2A2A;
	static constexpr size_t ExtPointerOffset = 0x18;

	class ExtData final : public Extension<BulletClass>
	{
	public:
		unsigned int RNGSeed { 0u };
		BulletTypeExt::ExtData* TypeExtData;
		HouseClass* FirerHouse;
		int CurrentStrength;
		TechnoTypeExt::ExtData* InterceptorTechnoType;
		InterceptedStatus InterceptedStatus;
		bool DetonateOnInterception;
		std::vector<std::unique_ptr<LaserTrailClass>> LaserTrails;
		bool SnappedToTarget; // Used for custom trajectory projectile target snap checks
		int DamageNumberOffset;
		int ParabombFallRate;

		TrajectoryPointer Trajectory;

		ExtData(BulletClass* OwnerObject) : Extension<BulletClass>(OwnerObject)
			, TypeExtData { nullptr }
			, FirerHouse { nullptr }
			, CurrentStrength { 0 }
			, InterceptorTechnoType { nullptr }
			, InterceptedStatus { InterceptedStatus::None }
			, DetonateOnInterception { true }
			, LaserTrails {}
			, Trajectory { nullptr }
			, SnappedToTarget { false }
			, DamageNumberOffset { INT32_MIN }
			, ParabombFallRate { 0 }
		{ }

		virtual ~ExtData() = default;

		virtual void InvalidatePointer(void* ptr, bool bRemoved) override { }

		virtual void LoadFromStream(PhobosStreamReader& Stm) override;
		virtual void SaveToStream(PhobosStreamWriter& Stm) override;

		void InterceptBullet(TechnoClass* pSource, BulletClass* pInterceptor);
		void ApplyRadiationToCell(CellStruct cell, int spread, int radLevel);
		void InitializeLaserTrails();

		__forceinline unsigned int RNG_Next()
		{
			// xorshift32 — deterministic & fast
			unsigned int x = this->RNGSeed;
			x ^= x << 13; x ^= x >> 17; x ^= x << 5;
			if (!x) { x = 0xA5366B4D; }
			this->RNGSeed = x;
			return x;
		}

		__forceinline int RNG_Ranged(int lo, int hi)
		{
			if (hi <= lo) { return lo; }
			const unsigned int span = (unsigned int)(hi - lo + 1);
			return lo + (int)(RNG_Next() % span);	
		}

		static BulletExt* Get(BulletClass* p) { /* your ext map */ }
		uint32_t SyncSeed = 0xDEADBEEF;
		inline uint32_t Next()
		{
			// xorshift32 – fast, deterministic, no globals
			uint32_t x = SyncSeed;
			x ^= x << 13; x ^= x >> 17; x ^= x << 5;
			SyncSeed = x ? x : 0xA5366B4D; // avoid 0
			return SyncSeed;
		}
		inline int NextRange(int maxExclusive)
		{
			return maxExclusive > 0 ? (int)(Next() % (uint32_t)maxExclusive) : 0;
		}

	private:
		template <typename T>
		void Serialize(T& Stm);
	};

	class ExtContainer final : public Container<BulletExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;

	static void ApplyArcingFix(BulletClass* pThis, const CoordStruct& sourceCoords, const CoordStruct& targetCoords, BulletVelocity& velocity);

	static void SimulatedFiringUnlimbo(BulletClass* pBullet, HouseClass* pHouse, WeaponTypeClass* pWeapon, const CoordStruct& sourceCoords, bool randomVelocity);
	static void SimulatedFiringEffects(BulletClass* pBullet, HouseClass* pHouse, ObjectClass* pAttach, bool firingEffect, bool visualEffect);
	static inline void SimulatedFiringAnim(BulletClass* pBullet, HouseClass* pHouse, ObjectClass* pAttach);
	static inline void SimulatedFiringReport(BulletClass* pBullet);
	static inline void SimulatedFiringLaser(BulletClass* pBullet, HouseClass* pHouse);
	static inline void SimulatedFiringElectricBolt(BulletClass* pBullet);
	static inline void SimulatedFiringRadBeam(BulletClass* pBullet, HouseClass* pHouse);
	static inline void SimulatedFiringParticleSystem(BulletClass* pBullet, HouseClass* pHouse);

};
