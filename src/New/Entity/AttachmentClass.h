#pragma once

#include <algorithm>
#include <deque>

#include <GeneralStructures.h>

#include <New/Type/AttachmentTypeClass.h>
#include <Ext/TechnoType/Body.h>

class TechnoClass;

class AttachmentClass
{
public:
	static std::vector<AttachmentClass*> Array;

	TechnoTypeExt::ExtData::AttachmentDataEntry* Data;
	TechnoClass* Parent;
	TechnoClass* Child;
	CDTimerClass RespawnTimer;
	Mission LastParentMission;
	AbstractClass* LastParentTarget;

	// Formation system data
	struct TrailPoint {
		CoordStruct Position;
		DirStruct Facing;
		int Timestamp;

		TrailPoint() : Position(CoordStruct::Empty), Facing(DirStruct(0)), Timestamp(0) {}
		TrailPoint(CoordStruct pos, DirStruct face, int time) : Position(pos), Facing(face), Timestamp(time) {}
	};

	std::deque<TrailPoint> ParentMovementHistory;
	CDTimerClass PositionUpdateTimer;
	CoordStruct LastParentPosition;
	CoordStruct CurrentFormationPosition;
	DirStruct CurrentChildFacing;
	CoordStruct TargetTrailPosition;
	DirStruct TargetTrailFacing;
	double OrbitAngle;
	int FormationIndex;
	int LastUpdateFrame;
	bool IsMovingToTrailPosition;


	AttachmentClass(TechnoTypeExt::ExtData::AttachmentDataEntry* data,
		TechnoClass* pParent, TechnoClass* pChild = nullptr) :
		Data { data },
		Parent { pParent },
		Child { pChild },
		RespawnTimer { },
		LastParentMission { Mission::None },
		LastParentTarget { nullptr },
		ParentMovementHistory { },
		PositionUpdateTimer { },
		LastParentPosition { CoordStruct::Empty },
		CurrentFormationPosition { CoordStruct::Empty },
		CurrentChildFacing { DirStruct(0) },
		TargetTrailPosition { CoordStruct::Empty },
		TargetTrailFacing { DirStruct(0) },
		OrbitAngle { 0.0 },
		FormationIndex { 0 },
		LastUpdateFrame { 0 },
		IsMovingToTrailPosition { false }
	{
		Array.push_back(this);
		// Initialize formation data immediately if parent is available
		if (this->Parent) {
			this->InitializeFormationData();
		}
	}

	AttachmentClass() :
		Data { nullptr },
		Parent { nullptr },
		Child { nullptr },
		RespawnTimer { },
		LastParentMission { Mission::None },
		LastParentTarget { nullptr },
		ParentMovementHistory { },
		PositionUpdateTimer { },
		LastParentPosition { CoordStruct::Empty },
		CurrentFormationPosition { CoordStruct::Empty },
		CurrentChildFacing { DirStruct(0) },
		TargetTrailPosition { CoordStruct::Empty },
		TargetTrailFacing { DirStruct(0) },
		OrbitAngle { 0.0 },
		FormationIndex { 0 },
		LastUpdateFrame { 0 },
		IsMovingToTrailPosition { false }
	{
		Array.push_back(this);
	}

	~AttachmentClass();

	AttachmentTypeClass* GetType();
	TechnoTypeClass* GetChildType();
	CoordStruct GetChildLocation();

	// Formation system methods
	CoordStruct CalculateFormationPosition();
	CoordStruct GetTrailPosition();
	CoordStruct GetEscortPosition();
	CoordStruct GetOrbitPosition();
	CoordStruct GetLineFormationPosition();
	CoordStruct GetWedgeFormationPosition();
	CoordStruct GetDiamondFormationPosition();
	bool IsPositionBlocked(CoordStruct position);
	CoordStruct FindAlternativePosition(CoordStruct preferred);
	void UpdateMovementHistory();
	void InitializeFormationData();

	// Enhanced trail system methods
	void UpdateTrailMovement();
	CoordStruct InterpolateTrailPosition(const TrailPoint& from, const TrailPoint& to, double progress);
	DirStruct InterpolateTrailFacing(const DirStruct& from, const DirStruct& to, double progress);
	DirStruct CalculateMovementFacing(CoordStruct from, CoordStruct to);
	double CalculateTrailProgress();
	int GetCurrentGameFrame();

	// Train-inspired direct following methods
	CoordStruct GetDirectFollowPosition();
	AttachmentClass* GetLeadingAttachment();
	CoordStruct CalculateFollowPosition(CoordStruct leadPos, DirStruct leadFacing);

	void Initialize();
	void CreateChild();
	void AI();
	void Destroy(TechnoClass* pSource);
	void ChildDestroyed();

	void Unlimbo();
	void Limbo();

	bool AttachChild(TechnoClass* pChild);
	bool DetachChild();

	void InvalidatePointer(void* ptr);

	bool Load(PhobosStreamReader& stm, bool registerForChange);
	bool Save(PhobosStreamWriter& stm) const;

private:
	template <typename T>
	bool Serialize(T& stm);
};
