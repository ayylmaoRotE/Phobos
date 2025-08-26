#include "Body.h"
#include <ParticleClass.h>

// Small, header-local clamp. Branchy is fine on this CPU/branch predictor and avoids pulling <algorithm>.
static __forceinline int clamp_int(int v, int lo, int hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

DEFINE_HOOK(0x62BE30, ParticleClass_Gas_AI_DriftSpeed, 0x5)
{
	enum { ContinueAI = 0x62BE60 };

	GET(ParticleClass*, pParticle, EBP);

	// Ext lookup once; type is stable for the particle
	const auto pExt = ParticleTypeExt::ExtMap.Find(pParticle->Type);
	const int max = pExt->Gas_MaxDriftSpeed;
	const int min = -max;

	// Pull to locals (one deref), clamp, then write back once.
	int vx = pParticle->Velocity.X;
	int vy = pParticle->Velocity.Y;

	if (vx > max || vx < min)
		vx = clamp_int(vx, min, max);
	if (vy > max || vy < min)
		vy = clamp_int(vy, min, max);

	// Store if changed (prevents false writes)
	pParticle->Velocity.X = vx;
	pParticle->Velocity.Y = vy;

	return ContinueAI;
}
