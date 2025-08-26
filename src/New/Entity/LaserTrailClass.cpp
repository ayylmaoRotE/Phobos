#include "LaserTrailClass.h"

#include <Utilities/TemplateDef.h>
#include <Ext/EBolt/Body.h>

// Draws LaserTrail if the conditions are suitable.
// Returns true if drawn, false otherwise.
bool LaserTrailClass::Update(CoordStruct location)
{
	bool result = false;

	// First update since construction/reset
	if (!this->LastLocation.isset())
	{
		this->LastLocation = location;
		return result;
	}

	// Snapshot locals to avoid multiple nullable.Get() and pointer indirections
	const auto last = this->LastLocation.Get();
	const auto pType = this->Type;

	// Distance gate (3D): avoid sqrt by comparing squared values.
	const long long dx = static_cast<long long>(location.X) - last.X;
	const long long dy = static_cast<long long>(location.Y) - last.Y;
	const long long dz = static_cast<long long>(location.Z) - last.Z;
	const long long dist2 = dx * dx + dy * dy + dz * dz;

	//  use cached squared threshold (precomputed at type load)
	const long long seg2 = static_cast<long long>(pType->SegmentLengthSq);

	if (dist2 > seg2) // TODO reimplement IgnoreVertical properly? (kept as-is)
	{
		// We spawn a new segment only if visible and not cloaked.
		const bool xyMovedEnough =
			pType->IgnoreVertical
			? (abs(static_cast<int>(dx)) > 16 || abs(static_cast<int>(dy)) > 16)
			: true;

		if (this->Visible && !this->Cloaked && xyMovedEnough)
		{
			if (pType->DrawType == LaserTrailDrawType::Laser)
			{
				const auto pLaser = GameCreate<LaserDrawClass>(
					last, location,
					this->CurrentColor, ColorStruct{ 0, 0, 0 }, ColorStruct { 0, 0, 0 },
					pType->FadeDuration.Get(64));

				pLaser->Thickness = pType->Thickness;
				pLaser->IsHouseColor = true;
				pLaser->IsSupported = pType->IsIntense;
			}
			else if (pType->DrawType == LaserTrailDrawType::EBolt)
			{
				const auto pBolt = GameCreate<EBolt>();
				const auto pBoltExt = EBoltExt::ExtMap.Find(pBolt);
				const auto& boltDisable = pType->Bolt_Disable;
				const auto& boltColor = pType->Bolt_Color;

				const int alternateIdx = pType->IsAlternateColor ? 5 : 10;
				const int defaultAlternate = EBoltExt::GetDefaultColor_Int(FileSystem::PALETTE_PAL, alternateIdx);
				const int defaultWhite = EBoltExt::GetDefaultColor_Int(FileSystem::PALETTE_PAL, 15);

				for (int idx = 0; idx < 3; ++idx)
				{
					if (boltDisable[idx])
					{
						pBoltExt->Disable[idx] = true;
					}
					else if (boltColor[idx].isset())
					{
						pBoltExt->Color[idx] = boltColor[idx].Get();
					}
					else
					{
						pBoltExt->Color[idx] = Drawing::Int_To_RGB(idx < 2 ? defaultAlternate : defaultWhite);
					}
				}

				pBoltExt->Arcs = pType->Bolt_Arcs;

				int fade = pType->FadeDuration.Get(17);
				fade = clampInt(fade, 1, 31);
				pBolt->Lifetime = 1 << (fade - 1);
				pBolt->AlternateColor = pType->IsAlternateColor;

				pBolt->Fire(this->LastLocation, location, 0);
			}
			else if (pType->DrawType == LaserTrailDrawType::RadBeam)
			{
				const auto pRadBeam = RadBeam::Allocate(RadBeamType::RadBeam);
				pRadBeam->SetCoordsSource(this->LastLocation);
				pRadBeam->SetCoordsTarget(location);
				pRadBeam->Period = pType->FadeDuration.Get(15);
				pRadBeam->Amplitude = pType->Beam_Amplitude;

				const ColorStruct beamColor = pType->Beam_Color.Get(RulesClass::Instance->RadColor);
				pRadBeam->SetColor(beamColor);
			}

			result = true;
		}

		this->LastLocation = location;
	}

	return result;
}

#pragma region Save/Load

template <typename T>
bool LaserTrailClass::Serialize(T& stm)
{
	return stm
		.Process(this->Type)
		.Process(this->Visible)
		.Process(this->Cloaked)
		.Process(this->FLH)
		.Process(this->IsOnTurret)
		.Process(this->CurrentColor)
		.Process(this->LastLocation)
		.Process(this->Intrinsic)
		.Success();
};

bool LaserTrailClass::Load(PhobosStreamReader& stm, bool RegisterForChange)
{
	return Serialize(stm);
}

bool LaserTrailClass::Save(PhobosStreamWriter& stm) const
{
	return const_cast<LaserTrailClass*>(this)->Serialize(stm);
}

#pragma endregion
