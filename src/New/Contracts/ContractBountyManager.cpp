#include "ContractBountyManager.h"
#include <Utilities/TemplateDef.h>
#include <Unsorted.h>
#include <windows.h>     // COLORREF, RGB
#include <vector>
#include <string>
#include <cwchar>
#include <cwctype>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <cstdarg>
#include <unordered_map>
#include <cstring>       // strlen
#include <Utilities/Patch.h>
#include "MessageListClass.h"   // or the correct relative path
#include <ScenarioClass.h>    // ScenarioClass::Instance
#include <Randomizer.h>       // ScenarioClass::Random
#include <StringTable.h>
#include <SuperWeaponTypeClass.h>
#include <ColorScheme.h>          // ColorScheme::FindIndex used in DrawUI
#include <Utilities/Debug.h>      // Debug::Log calls
#include <climits> 
#include <MapClass.h>
#include <ObjectClass.h>
#include <CellClass.h>
#include <Misc/FlyingStrings.h>
#include <CCINIClass.h>
#include <Utilities/EVAGuard.h>

using namespace Contracts;

// (Unused helper you added; harmless to keep)
static MessageListClass* BannerML = nullptr;
static int BannerML_X = 0, BannerML_Y = 0, BannerML_W = 0, BannerML_H = 0;

static void ensureBannerMessageList(int x, int y, int width)
{
	const int wantH = 14; // or uiHeaderHeight if you prefer
	if (!BannerML || BannerML_X != x || BannerML_Y != y || BannerML_W != width || BannerML_H != wantH)
	{
		if (BannerML) { delete BannerML; BannerML = nullptr; }
		BannerML = new MessageListClass();
		BannerML->Init(
			x, y,
			/*MaxMsg*/1,
			/*MaxChars*/160,
			/*Height*/wantH,
			/*EditX*/0, /*EditY*/0,
			/*Overflow*/0, /*Start*/0, /*End*/0,
			/*Width*/width
		);
		BannerML_X = x; BannerML_Y = y; BannerML_W = width; BannerML_H = wantH;
	}
}

static inline int  HouseIdx(const HouseClass* h)
{
	return h ? h->ArrayIndex : -1;
}
static inline HouseClass* HouseByIdx(int idx)
{
	return (idx >= 0 && idx < HouseClass::Array.Count) ? HouseClass::Array.Items[idx] : nullptr;
}
static inline int  SWTypeIdx(const SuperWeaponTypeClass* t)
{
	return t ? t->ArrayIndex : -1;
}
static inline SuperWeaponTypeClass* SWTypeByIdx(int idx)
{
	return (idx >= 0 && idx < SuperWeaponTypeClass::Array.Count) ? SuperWeaponTypeClass::Array.Items[idx] : nullptr;
}

// --------------------- NEW: global enable/disable switch ---------------------
static bool gContractsEnabled = true;
static inline void wtrim_inplace(std::wstring& s);
static inline void trim_inplace(std::string& s)
{
	size_t b = 0, e = s.size();
	while (b < e && (unsigned char)s[b] <= ' ') ++b;
	while (e > b && (unsigned char)s[e - 1] <= ' ') --e;
	if (b || e != s.size()) s.assign(s.data() + b, e - b);
}

static bool parseBoolLoose(const std::string& raw, bool defv)
{
	std::string s = raw;
	trim_inplace(s);
	for (auto& c : s) c = (char)std::tolower((unsigned char)c);
	if (s.empty()) return defv;
	if (s == "1" || s == "true" || s == "yes" || s == "on")  return true;
	if (s == "0" || s == "false" || s == "no" || s == "off") return false;
	return defv; // unrecognized → default
}
// ----------------------------------------------------------------------------

static HouseClass* PickTeamRep(const Competitor& comp)
{
	HouseClass* best = nullptr;
	int bestSlot = INT_MAX, bestIdx = INT_MAX;
	for (auto* h : comp.members)
	{
		if (!h) continue;
		const int slot = h->GetSpawnPosition();  // -1 if not a lobby player
		const int idx = h->ArrayIndex;
		if (slot >= 0)
		{
			if (slot < bestSlot || (slot == bestSlot && idx < bestIdx))
			{
				best = h; bestSlot = slot; bestIdx = idx;
			}
		}
		else if (!best)
		{
			best = h; bestIdx = idx;
		}
	}
	return best;
}

static int SchemeIdx0_FromHouse(const HouseClass* h)
{
	if (!h) return -1;

	const int n = ColorScheme::Array.Count;

	// (A) Many WW paths store 1-based ArrayIndex in HouseClass::ColorSchemeIndex
	int v = h->ColorSchemeIndex;
	if (v >= 1 && v <= n && ColorScheme::Array.Items[v - 1])
	{
		return v - 1;
	}

	// (B) Some forks store a 0..8 player-color slot here → map via LUT (1-based) → 0-based
	if (v >= 0 && v < 9)
	{
		const int idx1 = ColorScheme::PlayerColorToColorSchemeLUT[v];
		const int idx0 = idx1 - 1;
		if (idx0 >= 0 && idx0 < n && ColorScheme::Array.Items[idx0])
		{
			return idx0;
		}
	}

	// (C) Fallback: use the actual lobby spawn slot (0..8) when available
	const int slot = h->GetSpawnPosition(); // -1 if not a normal lobby player
	if (slot >= 0 && slot < 9)
	{
		const int idx1 = ColorScheme::PlayerColorToColorSchemeLUT[slot];
		const int idx0 = idx1 - 1;
		if (idx0 >= 0 && idx0 < n && ColorScheme::Array.Items[idx0])
		{
			return idx0;
		}
	}

	return -1;
}

static int SchemeIdx0_Default()
{
	int idx = ColorScheme::FindIndex("LightGrey", /*ShadeCount*/53);
	return (idx >= 0) ? idx : 0;
}

// Rows message list – draws the colored per-team lines via scheme indices.
static MessageListClass* RowsML = nullptr;
static int RowsML_X = 0, RowsML_Y = 0, RowsML_W = 0, RowsML_H = 0;
static size_t RowsML_MaxMsg = 0;

static void ensureRowsMessageList(int x, int y, int width, size_t lines)
{
	const int lineH = 12;
	const int wantW = width;
	const int wantH = static_cast<int>(std::max<size_t>(1, lines) * lineH);
	const size_t wantMax = std::max<size_t>(1, lines);

	const bool needNew =
		!RowsML ||
		RowsML_X != x || RowsML_Y != y ||
		RowsML_W != wantW || RowsML_H != wantH ||
		RowsML_MaxMsg != wantMax;

	if (needNew)
	{
		if (RowsML) { delete RowsML; RowsML = nullptr; }
		RowsML = new MessageListClass();
		RowsML->Init(x, y,
			/*MaxMsg   */ static_cast<int>(wantMax),
			/*MaxChars */ 160,
			/*Height   */ wantH,
			/*EditX    */ 0, /*EditY*/ 0,
			/*Overflow */ 0, /*Start*/ 0, /*End*/ 0,
			/*Width    */ wantW);

		RowsML_X = x; RowsML_Y = y; RowsML_W = wantW; RowsML_H = wantH;
		RowsML_MaxMsg = wantMax;
	}
}

// compact safe printers (avoid depending on unknown ID accessors)
static inline int SWIdx(const SuperWeaponTypeClass* t)
{
	return t ? t->ArrayIndex : -1;
}

static inline CoordStruct CellToCoord(CellStruct c)
{
	// Z=0 is fine for reward spawns / SW targets; adjust if you have floor height handy
	return { c.X * 256, c.Y * 256, 0 };
}

void Manager::EnsureSeeds()
{
	if (Contracts.empty() || Rewards.empty()) return;
	NormalizeDefinitions();
}

void Manager::BeginIntermissionNow()
{
	if (intermissionEndFrame < 0)
	{
		bannerFramesLeft = intermissionFrames;                 // show banner for full gap
		intermissionEndFrame = Unsorted::CurrentFrame + intermissionFrames;
	}
}

void Manager::CaptureMatchSeedIfDue(int64_t anchorFrame)
{
	if (MatchSeedCaptured) return;

	const int64_t now = Unsorted::CurrentFrame;
	if (now < anchorFrame) return;

	if (auto* sc = ScenarioClass::Instance)
	{
		MatchSeed = sc->Random.Random();   // lockstep draw IF every peer calls at/after same sim frame
	}
	else
	{
		MatchSeed = 0; // deterministic fallback
	}
	MatchSeedCaptured = true;
}

static inline void BuildEligibleIndices(const std::vector<ContractDef>& contracts,
										uint32_t epoch,
										std::vector<int>& out)
{
	out.clear();
	out.reserve(contracts.size());
	for (int i = 0; i < (int)contracts.size(); ++i)
	{
		if (contracts[i].availableAfter <= (int)epoch)
		{
			out.push_back(i);
		}
	}

	// Safety (misconfig): if nothing is eligible, fallback to all
	if (out.empty())
	{
		out.resize(contracts.size());
		for (int i = 0; i < (int)contracts.size(); ++i) out[i] = i;
	}
}

static inline std::string readN(CCINIClass* ini, const char* sec, const char* key, const char* defv, size_t cap)
{
	std::vector<char> buf(cap + 1);
	if (!defv) defv = "";
	ini->ReadString(sec, key, defv, buf.data(), static_cast<int>(buf.size()));
	buf[cap] = '\0'; // belt-and-suspenders
	return std::string(buf.data());
}

// Reads base key and optional continuations: Key, Key.1, Key.2, ...
// Concatenates them with commas so your CSV parser works unchanged.
static inline std::string readLongList(CCINIClass* ini, const char* sec, const char* baseKey)
{
	// First chunk (8 KB cap)
	std::string out = readN(ini, sec, baseKey, "", 8192);

	// Continuations: .1, .2, ... (up to a sane limit)
	char k[128];
	for (int i = 1; i <= 64; ++i)
	{
		_snprintf_s(k, _TRUNCATE, "%s.%d", baseKey, i);
		std::string part = readN(ini, sec, k, "", 8192);
		if (part.empty())
			break;
		if (!out.empty() && out.back() != ',' && out.back() != ';')
			out.push_back(',');
		out += part;
	}
	return out;
}

// ---------- narrow<->wide helpers (ASCII-safe, fast, deterministic) ----------
static inline std::wstring ToWideASCII(const char* s);
static inline std::string  WideToNarrowASCII(const std::wstring& w);
static HouseClass* PickDeterministicOwner(const Competitor& winner)
{
	HouseClass* best = nullptr;
	int bestIdx = INT_MAX;
	for (auto* h : winner.members)
	{
		if (!h) continue;
		const int idx = h->ArrayIndex; // stable across peers
		if (idx >= 0 && idx < bestIdx) { bestIdx = idx; best = h; }
	}
	return best;
}

static void BroadcastRewardPopup(HouseClass* owner, const std::wstring& msg)
{
	if (!owner || msg.empty()) return;
	const CoordStruct world = CellToCoord(owner->Base.Center);
	const ColorStruct col { 255,255,255 };
	const Point2D offset { 0,-80 };
	FlyingStrings::Add(msg.c_str(), world, col, offset);
}

static int ExtractTrailingNumber(const std::string& s)
{
	int i = static_cast<int>(s.size()) - 1;
	while (i >= 0 && isdigit(static_cast<unsigned char>(s[i]))) --i;
	if (i + 1 >= static_cast<int>(s.size())) return INT_MAX; // no digits
	return atoi(s.c_str() + i + 1);
}

static inline bool starts_with_icase(const std::wstring& s, const wchar_t* prefix)
{
	const size_t n = wcslen(prefix);
	if (s.size() < n) return false;
	for (size_t i = 0; i < n; ++i)
	{
		// cast to wint_t per spec
		wchar_t a = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(s[i])));
		wchar_t b = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(prefix[i])));
		if (a != b) return false;
	}
	return true;
}

int Manager::DeterministicRanged(int min, int max, uint32_t salt, uint32_t index)
{
	const uint32_t span = static_cast<uint32_t>(max - min + 1);
	uint32_t v = Hash32(salt ^ (index * 0x9E3779B9u));
	return min + static_cast<int>(v % span);
}
int Manager::DeterministicWeightedPick(int totalWeight, uint32_t salt, uint32_t index) const
{
	return DeterministicRanged(1, totalWeight, salt, index);
}

static inline int PlayerNumberFromHouse(const HouseClass* h)
{
	if (!h) return -1;
	const int slot = h->GetSpawnPosition();
	if (slot >= 0)   return slot + 1;        // “Player 1..9” from lobby slot
	const int idx = h->ArrayIndex;
	return (idx >= 0) ? (idx + 1) : -1;      // fallback
}

static inline std::wstring LoadCSFSafe(const std::wstring& labelW)
{
	if (labelW.empty()) return L"";

	// CSF lookup expects narrow label
	std::string labelA = WideToNarrowASCII(labelW);
	if (labelA.empty()) return L"";

	if (const wchar_t* s = StringTable::LoadString(labelA.c_str()))
	{
		// empty or special single-glyph "¾" → treat as missing
		if (s[0] == L'\0') return L"";
		if (s[0] == 0x00BE && s[1] == L'\0') return L"";

		// Normalize to wstring to inspect/trim
		std::wstring v(s);
		wtrim_inplace(v);

		// Many RA2/YR builds return "MISSING: <text>" (or case variants).
		// If present, prefer the part after the colon; if nothing meaningful, treat as missing.
		auto starts_missing = [&](const std::wstring& x) -> bool
			{
				return starts_with_icase(x, L"MISSING")
					|| starts_with_icase(x, L"<MISSING")
					|| starts_with_icase(x, L"Missing")
					|| starts_with_icase(x, L"<Missing");
			};

		if (starts_missing(v))
		{
			// try to keep the readable tail after ':' if any
			size_t colon = v.find(L':');
			if (colon != std::wstring::npos && colon + 1 < v.size())
			{
				std::wstring tail = v.substr(colon + 1);
				wtrim_inplace(tail);
				if (!tail.empty())
				{
					return tail;            // strip the "MISSING:" marker, keep human text
				}
			}
			return L"";                     // force fallback elsewhere
		}

		return v;                           // good CSF string
	}
	return L"";
}

// --- normalization helpers ---
uint32_t Manager::Hash32(uint32_t x)
{
	x += 0x9E3779B9u;
	x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
	x = (x ^ (x >> 13)) * 0xC2B2AE35u;
	return x ^ (x >> 16);
}

uint32_t Manager::HashStr32(uint32_t h, const char* s)
{
	for (; *s; ++s)
	{
		h = Hash32(h ^ static_cast<uint8_t>(*s));
	}
	return h;
}

static inline std::string canon_kind(std::string s)
{
	trim_inplace(s);
	std::string o; o.reserve(s.size());
	for (unsigned char c : s)
	{
		if (c == ' ' || c == '\t' || c == '_' || c == '-') continue;
		o.push_back((char)std::tolower(c));
	}
	return o;
}
static inline void wtrim_inplace(std::wstring& s)
{
	size_t b = 0, e = s.size();
	while (b < e && iswspace(s[b])) ++b;
	while (e > b && iswspace(s[e - 1])) --e;
	if (b || e != s.size()) s.assign(s.begin() + b, s.begin() + e);
}

static inline std::wstring ToWideASCII(const char* s)
{
	if (!s) return L"";
	const size_t n = std::strlen(s);
	std::wstring w; w.resize(n);
	for (size_t i = 0; i < n; ++i)
	{
		w[i] = static_cast<unsigned char>(s[i]);
	}
	return w;
}

static inline std::string WideToNarrowASCII(const std::wstring& w)
{
	std::string s; s.reserve(w.size());
	for (wchar_t ch : w)
	{
		s.push_back((ch >= 0 && ch <= 0xFF) ? static_cast<char>(ch) : '?');
	}
	return s;
}

// Prefer localized UI name via StringTable if available, otherwise fall back to
// the type's ASCII Name converted to wide.
static inline std::wstring GetTypeDisplayName(TechnoTypeClass* t)
{
	if (!t) return L"";
	const wchar_t* w = nullptr;

	// Most YR SDKs expose UIName as a CSF label (narrow), e.g. "Name:GDI05".
	// If it exists, resolve it through the StringTable.
	if (t->UIName && t->UIName[0])
	{
		// t->UIName is wchar_t* label – resolve via safe CSF loader
		std::wstring ui = LoadCSFSafe(std::wstring(t->UIName));
		if (!ui.empty())
		{
			return ui; // only returns if not “¾”/empty
		}
	}

	// Fallback: ASCII name/ID (char[49] or const char*)
	if (t->Name)
	{
		return ToWideASCII(t->Name);
	}
	return L"";
}

static inline std::wstring GetSWDisplayName(SuperWeaponTypeClass* t)
{
	if (!t) return L"";
	if (t->UIName && t->UIName[0])
	{
		std::wstring ui = LoadCSFSafe(std::wstring(t->UIName));
		if (!ui.empty()) return ui;
	}
	if (t->ID) return ToWideASCII(t->ID);
	return L"Superweapon";
}

// Reward.Text %-codes:
//   %%  -> %
//   %m  -> money amount (int)
//   %c  -> unit count (int)
//   %u  -> unit display name
//   %S  -> superweapon display name
static std::wstring ExpandRewardText(const RewardDef& rd)
{
	std::wstring unitName = rd.unitType ? GetTypeDisplayName(rd.unitType) : L"";
	std::wstring swName = rd.swType ? GetSWDisplayName(rd.swType) : L"";

	const std::wstring& t = rd.rewardText;
	if (t.empty())
	{
		// sensible defaults if no template was provided
		switch (rd.kind)
		{
		case RewardKind::Money:       return L"Money: " + std::to_wstring(std::max(0, rd.moneyAmount));
		case RewardKind::Unit:        return std::to_wstring(std::max(1, rd.unitCount)) + L"× " + unitName;
		case RewardKind::SuperWeapon: return L"Superweapon: " + swName;
		}
	}

	std::wstring out; out.reserve(t.size() + 16);
	for (size_t i = 0; i < t.size(); ++i)
	{
		wchar_t c = t[i];
		if (c != L'%') { out.push_back(c); continue; }
		if (i + 1 >= t.size()) { out.push_back(L'%'); break; }

		switch (t[i + 1])
		{
		case L'%': out.push_back(L'%'); ++i; break;
		case L'm': case L'M': out += std::to_wstring(std::max(0, rd.moneyAmount)); ++i; break;
		case L'c': case L'C': out += std::to_wstring(std::max(1, rd.unitCount));     ++i; break;
		case L'u': case L'U': out += unitName;                                       ++i; break;
		case L's': case L'S': out += swName;                                         ++i; break;
		default:  out.push_back(L'%'); /* keep unknown %x literally */ break;
		}
	}
	return out;
}

static inline bool readBool(CCINIClass* ini, const char* sec, const char* key, bool defv)
{
	return ini->ReadBool(sec, key, defv);
}
static inline int readInt(CCINIClass* ini, const char* sec, const char* key, int defv)
{
	return ini->ReadInteger(sec, key, defv);
}
static inline std::wstring readW(CCINIClass* ini, const char* sec, const char* key, const wchar_t* defw = nullptr)
{
	char defA[512] = "";
	if (defw && *defw)
	{
		// deterministic ASCII mapping for default
		std::string tmp = WideToNarrowASCII(std::wstring(defw));
		strcpy_s(defA, tmp.c_str());
	}

	char buf[512] = {};
	ini->ReadString(sec, key, defA, buf, static_cast<int>(sizeof buf));
	return ToWideASCII(buf);
}

static inline std::string read(CCINIClass* ini, const char* sec, const char* key, const char* defv = "")
{
	char buf[512] {};
	ini->ReadString(sec, key, defv, buf, 512);
	return buf;
}

static inline int NowFrame() { return (int)Unsorted::CurrentFrame; }

void Manager::TickTransientSWs()
{
	if (TransientSWs.empty()) return;
	const int now = NowFrame();
	for (size_t i = 0; i < TransientSWs.size(); )
	{
		auto& e = TransientSWs[i];
		if (!e.SW || now >= e.ExpireFrame)
		{
			if (e.SW) e.SW->Lose();          // finalize one-shot
			TransientSWs[i] = TransientSWs.back();
			TransientSWs.pop_back();
		}
		else
		{
			++i;
		}
	}
}

// ---------- Manager singleton ----------

Manager& Manager::Instance()
{
	static Manager inst;
	return inst;
}

// ---------- parsing ----------

std::vector<TechnoTypeClass*> Manager::parseTechnoList(CCINIClass*, const char* csv)
{
	std::vector<TechnoTypeClass*> out;
	if (!csv || !*csv)
		return out;

	const size_t n = std::strlen(csv);
	std::vector<char> buf(n + 1);
	std::memcpy(buf.data(), csv, n + 1);

	auto trim_inplace_c = [](char*& s)
		{
			while (*s && (unsigned char)*s <= ' ') ++s;
			char* e = s + std::strlen(s);
			while (e > s && (unsigned char)e[-1] <= ' ') *--e = '\0';
		};

	char* ctx = nullptr;
	for (char* tok = strtok_s(buf.data(), ",;\t", &ctx);
		 tok;
		 tok = strtok_s(nullptr, ",;\t", &ctx))
	{
		trim_inplace_c(tok);
		if (!*tok) continue;

		if (auto* t = TechnoTypeClass::Find(tok))
		{
			out.push_back(t);
		}
		else
		{
			Debug::Log("[Contracts] Unknown TechnoType in Types: '%s'", tok);
		}
	}

	return out;
}

static double ParseDoubleCLocale(const char* s)
{
	// parse only [-]?[0-9]*[.][0-9]* with '.' as decimal separator
	// fast and deterministic
	bool neg = false; if (*s == '-') { neg = true; ++s; }
	uint64_t intp = 0;
	while (*s >= '0' && *s <= '9') { intp = intp * 10 + (*s++ - '0'); }
	uint64_t frac = 0, scale = 1;
	if (*s == '.')
	{
		++s;
		while (*s >= '0' && *s <= '9') { frac = frac * 10 + (*s++ - '0'); scale *= 10; }
	}
	double v = static_cast<double>(intp) + static_cast<double>(frac) / static_cast<double>(scale);
	return neg ? -v : v;
}

void Manager::parseContracts(CCINIClass* ini)
{
	Contracts.clear();
	for (int i = 1; ; ++i)
	{
		char base[64];
		sprintf_s(base, "Contract%d", i);

		// Check if this contract exists
		std::string typeStr = read(ini, "ContractBounties", (std::string(base) + ".Type").c_str(), "");
		if (typeStr.empty())
		{
			// If no Contract{i}.Type is found, check legacy single-line definition or break
			if (i == 1)
			{
				std::string legacy = read(ini, "ContractBounties", base, "");
				if (legacy.empty()) break;  // no contracts defined at all
				// TODO: parse legacy format if needed (not used in current config)
			}
			else
			{
				break;  // end of contract list
			}
		}

		ContractDef cd {};
		cd.id = base;                              // NEW
		cd.orderIndex = ExtractTrailingNumber(cd.id); // NEW
		cd.soundID.clear();
		cd.soundIndex = -1;
		// Determine contract kind
		std::string kindKey = canon_kind(typeStr);
		if (kindKey == "killunits")                 cd.kind = ContractKind::KillUnits;
		else if (kindKey == "killbuildings")        cd.kind = ContractKind::KillBuildings;
		else if (kindKey == "firstbuild")           cd.kind = ContractKind::FirstBuild;
		else if (kindKey == "infiltrate")           cd.kind = ContractKind::Infiltrate;
		else if (kindKey == "earnmoney")            cd.kind = ContractKind::EarnMoney;
		else
		{
			// Unknown type string – skip this contract
			continue;
		}

		// Read numeric parameters
		cd.required = readInt(ini, "ContractBounties", (std::string(base) + ".Required").c_str(), 0);
		std::string typesCSV = readLongList(ini, "ContractBounties", (std::string(base) + ".Types").c_str());
		cd.types = parseTechnoList(ini, typesCSV.c_str());  // parse comma-separated type list into TechnoTypeClass pointers
		cd.timerSeconds = readInt(ini, "ContractBounties", (std::string(base) + ".Timer").c_str(), 600);
		cd.perTeam = readBool(ini, "ContractBounties", (std::string(base) + ".ShareTeamProgress").c_str(), false);
		std::string tm = read(ini, "ContractBounties", (std::string(base) + ".TeamRequirementMultiplier").c_str(), "");
		cd.teamMultiplier = tm.empty() ? 1.0 : ParseDoubleCLocale(tm.c_str());
		cd.availableAfter = readInt(ini, "ContractBounties", (std::string(base) + ".AvailableAfter").c_str(), 0);

		// Text (wide) – read, trim, resolve Name: (case-insensitive). If CSF is bad/missing, auto-generate.
		cd.textTemplate = readW(ini, "ContractBounties", (std::string(base) + ".Text").c_str(), L"");

		// Contract Sounds
		std::string key = std::string(base) + ".Sound";
		std::string s = read(ini, "ContractBounties", key.c_str(), "");
		if (!s.empty())
		{
			cd.soundID = s;
			cd.soundIndex = VocClass::FindIndex(cd.soundID.c_str()); // -1 if unknown
		}

		wtrim_inplace(cd.textTemplate);

		bool haveText = !cd.textTemplate.empty();

		if (haveText && starts_with_icase(cd.textTemplate, L"Name:"))
		{
			std::wstring labelW = cd.textTemplate.substr(5);
			wtrim_inplace(labelW);

			// SAFE resolver: treats empty and single 0x00BE (“¾”) as missing.
			std::wstring resolved = LoadCSFSafe(labelW);
			if (!resolved.empty())
			{
				cd.textTemplate = std::move(resolved);
			}
			else
			{
				haveText = false; // force fallback
			}
		}

		if (!haveText)
		{
			// Auto-generate description (deterministic, MP-safe)
			switch (cd.kind)
			{
			case ContractKind::KillUnits:
				if (cd.types.empty())        cd.textTemplate = L"Eliminate " + std::to_wstring(cd.required) + L" enemy units";
				else if (cd.types.size() == 1) cd.textTemplate = L"Eliminate " + std::to_wstring(cd.required) + L" " + GetTypeDisplayName(cd.types[0]);
				else                         cd.textTemplate = L"Eliminate " + std::to_wstring(cd.required) + L" enemy units of specified types";
				break;
			case ContractKind::KillBuildings:
				if (cd.types.empty())        cd.textTemplate = L"Destroy " + std::to_wstring(cd.required) + L" enemy structures";
				else if (cd.types.size() == 1) cd.textTemplate = L"Destroy " + std::to_wstring(cd.required) + L" " + GetTypeDisplayName(cd.types[0]);
				else                         cd.textTemplate = L"Destroy " + std::to_wstring(cd.required) + L" enemy structures of specified types";
				break;
			case ContractKind::FirstBuild:
				if (cd.types.empty())        cd.textTemplate = L"Be the first to build the required item";
				else if (cd.types.size() == 1) cd.textTemplate = L"Be the first to build a " + GetTypeDisplayName(cd.types[0]);
				else                         cd.textTemplate = L"Be the first to build one of the specified items";
				break;
			case ContractKind::Infiltrate:
				if (cd.types.empty())        cd.textTemplate = L"Infiltrate " + std::to_wstring(cd.required) + L" enemy buildings";
				else if (cd.types.size() == 1) cd.textTemplate = L"Infiltrate an enemy " + GetTypeDisplayName(cd.types[0]);
				else                         cd.textTemplate = L"Infiltrate one of the specified enemy buildings";
				break;
			case ContractKind::EarnMoney:
				cd.textTemplate = L"Earn " + std::to_wstring(cd.required) + L" credits";
				break;
			default:
				cd.textTemplate = L"Complete the objective";
				break;
			}
		}

		Contracts.push_back(cd);
	}
}

void Manager::ensureHeaderMessageList()
{
	if (HeaderML) return;
	HeaderML = new MessageListClass();
	// X,Y = your existing uiX/uiY. MaxMsg=1, MaxChars=160, Height=uiHeaderHeight
	HeaderML->Init(uiX, uiY, /*MaxMsg*/1, /*MaxChars*/160,
				   uiHeaderHeight, /*EditX*/0, /*EditY*/0,
				   /*EnableOverflow*/0, /*OverflowStart*/0, /*OverflowEnd*/0,
				   uiHeaderWidth);
}

void Manager::parseRewards(CCINIClass* ini)
{
	Rewards.clear();
	totalRewardWeight = 0;

	for (int i = 1;; ++i)
	{
		char base[64]; sprintf_s(base, "Reward%d", i);
		auto typeS = read(ini, "ContractBounties", (std::string(base) + ".Type").c_str(), "");
		if (typeS.empty()) break;

		RewardDef rd {};
		rd.id = base;                              // NEW
		rd.orderIndex = ExtractTrailingNumber(rd.id); // NEW
		if (_stricmp(typeS.c_str(), "Money") == 0)
		{
			rd.kind = RewardKind::Money;
			rd.moneyAmount = readInt(ini, "ContractBounties", (std::string(base) + ".Amount").c_str(), 0);
		}
		else if (_stricmp(typeS.c_str(), "Unit") == 0)
		{
			rd.kind = RewardKind::Unit;

			std::string ut = read(ini, "ContractBounties", (std::string(base) + ".UnitType").c_str(), "");
			if (ut.empty())
			{
				// backward-compat: if .Type held the unit name by mistake
				auto maybe = read(ini, "ContractBounties", (std::string(base) + ".Type").c_str(), "");
				if (!maybe.empty()
					&& _stricmp(maybe.c_str(), "Unit") != 0
					&& _stricmp(maybe.c_str(), "Money") != 0
					&& _stricmp(maybe.c_str(), "SuperWeapon") != 0)
				{
					ut = std::move(maybe);
				}
			}

			rd.unitType = ut.empty() ? nullptr : TechnoTypeClass::Find(ut.c_str());
			rd.unitCount = readInt(ini, "ContractBounties", (std::string(base) + ".Count").c_str(), 1);
		}
		else if (_stricmp(typeS.c_str(), "SuperWeapon") == 0)
		{
			rd.kind = RewardKind::SuperWeapon;
			std::string st = read(ini, "ContractBounties", (std::string(base) + ".SuperWeapon").c_str(), "");
			if (st.empty())
			{
				auto maybe = read(ini, "ContractBounties", (std::string(base) + ".Type").c_str(), "");
				if (!maybe.empty()
					&& _stricmp(maybe.c_str(), "Unit") != 0
					&& _stricmp(maybe.c_str(), "Money") != 0
					&& _stricmp(maybe.c_str(), "SuperWeapon") != 0)
				{
					st = std::move(maybe);
				}
			}
			rd.swType = st.empty() ? nullptr : SuperWeaponTypeClass::Find(st.c_str());

			Debug::Log("[Contracts][parseRewards] %s: kind=SW, key='%s', swType=%p idx=%d",
				base, st.c_str(), rd.swType, SWIdx(rd.swType));
		}
		else
		{
			continue;
		}
		rd.rewardText = readW(ini, "ContractBounties", (std::string(base) + ".Text").c_str(), L"");
		wtrim_inplace(rd.rewardText);

		if (!rd.rewardText.empty() && starts_with_icase(rd.rewardText, L"Name:"))
		{
			std::wstring lbl = rd.rewardText.substr(5);
			wtrim_inplace(lbl);
			std::wstring resolved = LoadCSFSafe(lbl);
			rd.rewardText = resolved.empty() ? L"" : std::move(resolved); // fall back to auto text if CSF missing
		}

		rd.weight = readInt(ini, "ContractBounties", (std::string(base) + ".Weight").c_str(), 1);
		if (rd.weight < 0) rd.weight = 0;

		totalRewardWeight += rd.weight;
		std::string sk = std::string(base) + ".Sound";
		std::string s = read(ini, "ContractBounties", sk.c_str(), "");
		if (!s.empty())
		{
			rd.soundID = s;
			rd.soundIndex = VocClass::FindIndex(s.c_str()); // -1 if unknown → optional no-op
		}
		Rewards.push_back(rd);
	}

	// UI
	uiX = readInt(ini, "ContractBounties", "UI.X", 10);
	uiY = readInt(ini, "ContractBounties", "UI.Y", 10);
	uiShowTimer = readBool(ini, "ContractBounties", "UI.ShowTimer", true);

	// NEW
	uiHeaderHeight = std::max(10, readInt(ini, "ContractBounties", "UI.HeaderHeight", 18));
	uiHeaderWidth = std::max(100, readInt(ini, "ContractBounties", "UI.HeaderWidth", 640));
	uiHeaderColor = readInt(ini, "ContractBounties", "UI.HeaderColor", -1);
	uiHeaderStyle = readInt(ini, "ContractBounties", "UI.HeaderStyle", 0x4046);
}

void Manager::LoadFromRules(CCINIClass* ini)
{
	// ----------- NEW: read enable flag & early teardown when disabled -----------
	std::string rawEnabled = read(ini, "ContractBounties", "ContractsEnabled", "yes");
	gContractsEnabled = parseBoolLoose(rawEnabled, /*defv=*/true);
	Debug::Log("[Contracts] ContractsEnabled raw='%s' -> %d", rawEnabled.c_str(), gContractsEnabled ? 1 : 0);

	if (!gContractsEnabled)
	{
		if (HeaderML) { delete HeaderML; HeaderML = nullptr; }
		if (RowsML) { delete RowsML;   RowsML = nullptr; }
		RowsML_X = RowsML_Y = RowsML_W = RowsML_H = 0;
		RowsML_MaxMsg = 0;

		activeContractIndex = -1;
		bannerText.clear();
		bannerFramesLeft = 0;
		SeedsReady = false;

		Debug::Log("[Contracts] Disabled via rules. Skipping parse and runtime setup.");
		return;
	}
	// ---------------------------------------------------------------------------

	parseContracts(ini);
	parseRewards(ini);
	// --- ensure identical indices across clients ---
	NormalizeDefinitions();

	intermissionFrames = std::max(0, readInt(ini, "ContractBounties", "IntermissionFrames", 45)) * 15;

	// --- derive deterministic salts from the sorted ids ---
	if (HeaderML) { delete HeaderML; HeaderML = nullptr; }

	bannerText = Contracts.empty()
		? L"[Contracts] No contracts in [ContractBounties] (rulesmd.ini?)"
		: (L"[Contracts] Loaded: " + std::to_wstring(Contracts.size())
		   + L" contracts, " + std::to_wstring(Rewards.size()) + L" rewards");
	bannerFramesLeft = intermissionFrames; // 4 seconds
}

// Scenario (map) overrides. Called from Contracts::LoadScenarioOverrides(mapIni)
void Contracts::Manager::LoadFromScenario(CCINIClass* ini)
{
	if (!ini) return;

	// 0) What does the map have?
	const int keysInSection = ini->GetKeyCount("ContractBounties");
	Debug::Log("[Contracts] LoadFromScenario: INI=%p, [ContractBounties] keys=%d", ini, keysInSection);

	// 1) ContractsEnabled override (case-insensitive key)
	std::string rawEnabled = read(ini, "ContractBounties", "ContractsEnabled", "");
	if (rawEnabled.empty()) rawEnabled = read(ini, "ContractBounties", "contractsenabled", "");
	if (rawEnabled.empty()) rawEnabled = read(ini, "Basic", "ContractsEnabled", ""); // optional convenience

	if (!rawEnabled.empty())
	{
		const bool newEnabled = parseBoolLoose(rawEnabled, gContractsEnabled);
		if (newEnabled != gContractsEnabled)
		{
			gContractsEnabled = newEnabled;
			Debug::Log("[Contracts] Scenario ContractsEnabled -> %d", gContractsEnabled ? 1 : 0);
		}
	}

	if (!gContractsEnabled)
	{
		// Tear down everything if a map disables it.
		if (HeaderML) { delete HeaderML; HeaderML = nullptr; }
		if (RowsML) { delete RowsML;   RowsML = nullptr; }
		RowsML_X = RowsML_Y = RowsML_W = RowsML_H = 0;
		RowsML_MaxMsg = 0;
		activeContractIndex = -1;
		bannerText.clear();
		bannerFramesLeft = 0;
		SeedsReady = false;
		Debug::Log("[Contracts] Scenario disabled contracts. Skipping parse.");
		return;
	}

	{
		std::string raw = read(ini, "ContractBounties", "IntermissionFrames", "");
		if (!raw.empty())
		{
			int sec = 0;
			try { sec = std::stoi(raw); }
			catch (...) { sec = -1; }
			if (sec >= 0)
			{
				intermissionFrames = sec * 15;
				Debug::Log("[Contracts] Scenario IntermissionFrames -> %d (frames=%d)", sec, intermissionFrames);
			}
		}
	}

	// 2) If the map actually *defines* overrides (any key in section), replace rules with map data
	const bool hasAnyContract =
		!read(ini, "ContractBounties", "Contract1.Type", "").empty() ||
		!read(ini, "ContractBounties", "Contract1", "").empty();  // legacy single-line

	const bool hasAnyReward =
		!read(ini, "ContractBounties", "Reward1.Type", "").empty() ||
		!read(ini, "ContractBounties", "Reward1", "").empty() ||
		!read(ini, "ContractBounties", "Reward1.Text", "").empty();

	const bool hasAnyUI =
		!read(ini, "ContractBounties", "UI.X", "").empty() ||
		!read(ini, "ContractBounties", "UI.Y", "").empty() ||
		!read(ini, "ContractBounties", "UI.ShowTimer", "").empty() ||
		!read(ini, "ContractBounties", "UI.HeaderColor", "").empty() ||
		!read(ini, "ContractBounties", "UI.HeaderHeight", "").empty() ||
		!read(ini, "ContractBounties", "UI.HeaderWidth", "").empty();

	const bool mapHasOverrides = (hasAnyContract || hasAnyReward || hasAnyUI);

	if (mapHasOverrides)
	{
		Debug::Log("[Contracts] Scenario provides overrides — parsing map.");
		Contracts.clear();
		Rewards.clear();
		totalRewardWeight = 0;

		parseContracts(ini);
		parseRewards(ini);
		NormalizeDefinitions();

		if (HeaderML) { delete HeaderML; HeaderML = nullptr; }

		// force reseed/restart with new defs
		SeedsReady = false;
		NextContractID = 0;
		ActiveContractID = 0;

		bannerText = Contracts.empty()
			? L"[Contracts] Scenario overrides: no contracts."
			: (L"[Contracts] Scenario overrides loaded: "
			   + std::to_wstring(Contracts.size()) + L" contracts, "
			   + std::to_wstring(Rewards.size()) + L" rewards");
		bannerFramesLeft = intermissionFrames;

		Debug::Log("[Contracts] Scenario parse result: C=%zu, R=%zu", Contracts.size(), Rewards.size());
		return;
	}

	// 3B) No scenario overrides. If rulesmd was skipped earlier (disabled there),
	//     we must now load from rules so the system actually turns on.
	if (Contracts.empty() && Rewards.empty())
	{
		auto* rulesIni = CCINIClass::INI_Rules;   // <— grab global rules INI
		if (rulesIni)
		{
			Debug::Log("[Contracts] No map overrides. Loading from rulesmd due to scenario toggle.");
			Contracts.clear();
			Rewards.clear();
			totalRewardWeight = 0;

			parseContracts(rulesIni);
			parseRewards(rulesIni);
			NormalizeDefinitions();

			if (HeaderML) { delete HeaderML; HeaderML = nullptr; }

			SeedsReady = false;
			NextContractID = 0;
			ActiveContractID = 0;

			bannerText = Contracts.empty()
				? L"[Contracts] Enabled by scenario (no map overrides; rules has no contracts)."
				: L"[Contracts] Enabled by scenario (using rules definitions).";
			bannerFramesLeft = intermissionFrames;

			Debug::Log("[Contracts] Fallback rules parse result: C=%zu, R=%zu", Contracts.size(), Rewards.size());
		}
	}
}

// ---------- competitors / lifecycle ----------

static inline bool alliedMutual(HouseClass* a, HouseClass* b)
{
	return a && b && a->IsAlliedWith(b) && b->IsAlliedWith(a);
}

static bool IsCompetitiveHouse(HouseClass* h)
{
	if (!h)                      return false;
	if (h->Defeated)             return false;
	if (h->IsObserver())         return false;     // observers don't compete
	if (h->IsNeutral())          return false;     // HouseType->MultiplayPassive (Neutral/Special)

	// Belt-and-suspenders: filter by country ID if present.
	if (auto* id = h->get_ID())
	{
		if (_stricmp(id, "Neutral") == 0) return false;
		if (_stricmp(id, "Special") == 0) return false;
		if (_stricmp(id, "Civilian") == 0) return false; // optional, if you don’t want civ-side shown
		if (_stricmp(id, "Observer") == 0)  return false;
		if (_stricmp(id, "Spectator") == 0)  return false;
	}

	return true;
}

void Manager::rebuildCompetitors()
{
	Competitors.clear();

	// collect active non-defeated houses (skip observers if your SDK exposes it)
	std::vector<HouseClass*> houses;
	for (int i = 0; i < HouseClass::Array.Count; ++i)
	{
		auto* h = HouseClass::Array.Items[i];
		if (!IsCompetitiveHouse(h)) continue;
		houses.push_back(h);
	}
	if (houses.empty()) return;

	const auto& cd = Contracts[activeContractIndex];

	if (!cd.perTeam)
	{
		Competitors.reserve(houses.size());
		for (auto* h : houses)
		{
			Competitor c; c.members = { h };
			Competitors.push_back(std::move(c));
		}
	}
	else
	{
		// cluster by mutual alliance
		std::vector<char> used(houses.size(), 0);
		for (size_t i = 0; i < houses.size(); ++i) if (!used[i])
		{
			Competitor c; c.members.push_back(houses[i]); used[i] = 1;
			for (size_t j = i + 1; j < houses.size(); ++j) if (!used[j])
			{
				if (alliedMutual(houses[i], houses[j]))
				{
					c.members.push_back(houses[j]); used[j] = 1;
				}
			}
			Competitors.push_back(std::move(c));
		}
	}
}

int Manager::requiredFor(const ContractDef& c) const
{
	if (!c.perTeam) return c.required;
	if (c.teamMultiplier > 0.0)
	{
		return (int)std::ceil(c.required * c.teamMultiplier);
	}
	int maxSz = 1;
	for (const auto& comp : Competitors)
	{
		if ((int)comp.members.size() > maxSz) maxSz = (int)comp.members.size();
	}
	return c.required * maxSz;
}

void Manager::InitMatchSeed()
{
	if (!gContractsEnabled) return; // NEW guard

	// Base hash from definitions (stable across peers)
	uint32_t h = 0xC0B17EEFu;
	for (const auto& c : Contracts) { h = HashStr32(h, c.id.c_str()); }
	for (const auto& r : Rewards) { h = HashStr32(h, r.id.c_str()); }

	// Mix in the already-captured per-match randomness
	h ^= MatchSeed;

	GameSyncSeed = h;
	RollSaltContracts = Hash32(h ^ 0xC07A4C73u);
	RollSaltRewards = Hash32(h ^ 0xA5F00D23u);

	NextContractID = 0;
	ActiveContractID = 0;
	SeedsReady = true;
}

void Manager::StartOrSyncFromAnchor(int64_t anchorFrame)
{
	if (!gContractsEnabled) return;

	// 1) need defs and a captured MatchSeed
	EnsureSeeds();
	if (!MatchSeedCaptured) return;
	if (!SeedsReady) InitMatchSeed();
	if (Contracts.empty() || Rewards.empty()) return;

	const int n = static_cast<int>(Contracts.size());

	// 2) walk epochs from shared anchor up to current sim frame
	const int64_t now = Unsorted::CurrentFrame;
	int64_t start = anchorFrame;
	uint32_t epoch = 0;

	// track previous pick to avoid immediate repeats (Option A)
	int lastIdx = -1;

	constexpr uint32_t kMaxEpochScan = 100000; // safety

	while (epoch < kMaxEpochScan)
	{
		// NEW: pool filtered by AvailableAfter<=epoch
		std::vector<int> pool;
		BuildEligibleIndices(Contracts, epoch, pool);
		const int m = (int)pool.size();

		// base pick inside pool
		int k = DeterministicRanged(0, m - 1, RollSaltContracts, epoch);
		int idx = pool[k];

		// avoid immediate repeat vs previous epoch (Option A)
		if (m > 1 && lastIdx >= 0 && idx == lastIdx)
		{
			int k2 = DeterministicRanged(0, m - 1, RollSaltContracts ^ 0xBADC0DEu, epoch);
			int idx2 = pool[k2];
			if (idx2 == lastIdx)
			{
				int prevPos = -1;
				for (int i = 0; i < m; ++i) if (pool[i] == lastIdx) { prevPos = i; break; }
				if (prevPos >= 0)
				{
					k2 = (prevPos + 1 + (RollSaltContracts & 7)) % m;
					idx2 = pool[k2];
				}
			}
			idx = idx2;
		}

		const auto& cd = Contracts[idx];
		const int frames = std::max(1, cd.timerSeconds) * 15;
		const int64_t end = start + frames;

		if (now < end)
		{
			// ... (unchanged body that sets ActiveContractID/NextContractID,
			//      activeContractIndex, start/end frames, plays sound, rebuilds, etc.)
			// (your existing code here)
			return;
		}

		start = end;
		lastIdx = idx;        // remember to enforce "no immediate repeat"
		++epoch;
	}

	// extremely unlikely fallback
	initialAnchorFrame = now;
	startNewContract();
}

void Manager::NormalizeDefinitions()
{
	// Contracts: sort by number at end (Contract1, Contract2, ...), then by id
	std::sort(Contracts.begin(), Contracts.end(),
		[](const ContractDef& a, const ContractDef& b)
{
	if (a.orderIndex != b.orderIndex) return a.orderIndex < b.orderIndex;
	return a.id < b.id;
		});

	// Rewards: same idea (Reward1, Reward2, ...), then by id
	std::sort(Rewards.begin(), Rewards.end(),
		[](const RewardDef& a, const RewardDef& b)
{
	if (a.orderIndex != b.orderIndex) return a.orderIndex < b.orderIndex;
	return a.id < b.id;
		});
}

void Manager::StartContractAtFrame(int64_t anchorFrame)
{
	if (!gContractsEnabled) return; // NEW guard

	EnsureSeeds();
	if (!SeedsReady)
	{
		InitMatchSeed();  // <<< happens at the SAME sim frame on all peers
	}

	// Reset epoch sequence (deterministic across peers)
	NextContractID = 0;
	ActiveContractID = 0;

	// Force the first epoch to start at the identical sim frame on all peers
	initialAnchorFrame = anchorFrame;
	currentContractStartFrame = -1;

	// Cosmetic banner
	bannerText = L"[Contracts] Scenario start";
	bannerFramesLeft = intermissionFrames;

	if (Contracts.empty() || Rewards.empty())
	{
		bannerText = L"[Contracts] No contracts configured.";
		bannerFramesLeft = intermissionFrames;
		return;
	}

	startNewContract();
}

void Manager::StartContract()
{
	if (!gContractsEnabled) return; // NEW guard

	// SP/debug entry – seeds now if needed, from *this* callsite.
	EnsureSeeds();
	if (!SeedsReady)
	{
		InitMatchSeed();  // safe because SP or you call it deterministically
	}

	initialAnchorFrame = -1;           // no forced anchor, start "now"
	startNewContract();
}

void Manager::startNewContract()
{
	if (!gContractsEnabled) return;

	EnsureSeeds();
	if (!SeedsReady || Contracts.empty())
	{
		activeContractIndex = -1;
		return;
	}

	ActiveContractID = NextContractID++;

	// NEW: pick from eligible pool for this epoch
	std::vector<int> pool;
	BuildEligibleIndices(Contracts, ActiveContractID, pool);
	const int m = (int)pool.size();

	// base pick inside the pool
	int k = DeterministicRanged(0, m - 1, RollSaltContracts, ActiveContractID);
	int idx = pool[k];

	// avoid immediate repeat vs previous active index (Option A)
	if (m > 1 && idx == activeContractIndex)
	{
		int k2 = DeterministicRanged(0, m - 1, RollSaltContracts ^ 0xBADC0DEu, ActiveContractID);
		int idx2 = pool[k2];
		if (idx2 == activeContractIndex)
		{
			// step forward deterministically *within the pool*
			int prevPos = -1;
			for (int i = 0; i < m; ++i) if (pool[i] == activeContractIndex) { prevPos = i; break; }
			if (prevPos >= 0)
			{
				k2 = (prevPos + 1 + (RollSaltContracts & 7)) % m;
				idx2 = pool[k2];
			}
		}
		idx = idx2;
	}

	activeContractIndex = idx;

	rebuildCompetitors();
	for (auto& c : Competitors) c.progress = 0;
	moneyDuringContract.clear();

	const auto& cd = Contracts[activeContractIndex];
	activeIsPerTeam = cd.perTeam;
	activeRequired = std::max(1, requiredFor(cd));

	const int frames = std::max(1, cd.timerSeconds) * 15;

	const int64_t now = Unsorted::CurrentFrame;
	int64_t startFrame;

	if (initialAnchorFrame >= 0)
	{
		// First start after a sync/anchor: use the anchor exactly
		startFrame = initialAnchorFrame;
		initialAnchorFrame = -1;
	}
	else if (currentContractStartFrame >= 0 && now >= timerEndFrame)
	{
		// Expired window: advance on the SCHEDULED boundary, not 'now'
		startFrame = timerEndFrame;
	}
	else
	{
		// Early advance (e.g., a winner) or very first run without anchor
		startFrame = now;
	}

	currentContractStartFrame = startFrame;
	timerEndFrame = startFrame + frames;
	intermissionEndFrame = -1;

	// Play optional global jingle (deterministic RNG-guarded)
	if (activeContractIndex >= 0)
	{
		const auto& cd2 = Contracts[activeContractIndex];
		if (cd2.soundIndex >= 0)
		{
			PlayGlobal_Safe(cd2.soundIndex, 0x2000, 1.0f);
		}
	}

	// only announce “new contract” if no reward banner is currently up
	if (bannerFramesLeft <= 0)
	{
		bannerText = L"[Contracts] Contract finished. New contract: #"
			+ std::to_wstring(activeContractIndex + 1);
		bannerFramesLeft = intermissionFrames;
	}
}
// ---------- UI ----------

const wchar_t* Contracts::Manager::tryHouseName(HouseClass* /*h*/) const
{
	// Portable fallback: we don't rely on house/player name APIs here
	return L""; // returning empty enforces "Player N" fallback
}

static inline std::wstring ExpandContractText(const std::wstring& tmpl,
											  const ContractDef& cd,
											  int required)
{
	// choose a representative type name (first entry if any)
	std::wstring typeName;
	if (!cd.types.empty())
	{
		if (auto* t = cd.types.front())
		{
			typeName = GetTypeDisplayName(t);
		}
	}

	std::wstring out;
	out.reserve(tmpl.size() + 16);
	const size_t n = tmpl.size();
	for (size_t i = 0; i < n; ++i)
	{
		wchar_t c = tmpl[i];
		if (c != L'%')
		{
			out.push_back(c);
			continue;
		}
		// '%' at last position → literal '%'
		if (i + 1 >= n) { out.push_back(L'%'); break; }

		wchar_t k = tmpl[i + 1];
		if (k == L'%')
		{            // "%%" → "%"
			out.push_back(L'%');
			++i;
		}
		else if (k == L's' || k == L'S')
		{ // %s → type name
			out += typeName;
			++i;
		}
		else if (k == L'd' || k == L'D')
		{ // %d → required count
			out += std::to_wstring(required);
			++i;
		}
		else
		{
			// unknown specifier → keep literally
			out.push_back(L'%');
			// don't skip k so both chars are kept
		}
	}
	return out;
}

void Manager::DrawUI()
{
	if (PendingApplyAfterLoad) return;
	if (!gContractsEnabled) return;

	const int64_t now = Unsorted::CurrentFrame;
	const bool hasActive =
		(activeContractIndex >= 0 && activeContractIndex < (int)Contracts.size());

	// 1) Intermission HUD takes precedence and should render regardless of hasActive
	if (intermissionEndFrame >= 0)
	{
		const bool showBanner = (bannerFramesLeft > 0 && !bannerText.empty());

		std::wstring header;
		if (showBanner)
		{
			header = bannerText;
		}
		else
		{
			const int framesLeft = std::max<int>(0, (int)(intermissionEndFrame - now));
			const int sec = framesLeft / 15;
			wchar_t tb[32];
			_snwprintf_s(tb, _TRUNCATE, L"  (%d:%02d)", sec / 60, sec % 60);
			header = L"Next contract in";
			header += tb;
		}

		ensureHeaderMessageList();

		int colorIdx = uiHeaderColor;
		if (colorIdx < 0) { colorIdx = std::max(ColorScheme::FindIndex("LightGrey", 53), 0); }

		constexpr int kHeaderId = 1;
		if (auto* slot = HeaderML->GetMessage(kHeaderId))
		{
			if (wcsncmp(slot, header.c_str(), 160) != 0)
				wcsncpy_s(slot, 160 + 1, header.c_str(), _TRUNCATE);
		}
		else
		{
			HeaderML->AddMessage(nullptr, kHeaderId, header.c_str(),
								 colorIdx, static_cast<TextPrintType>(uiHeaderStyle),
								 0x7FFFFFFF, true);
		}
		HeaderML->Draw();
		return;
	}

	// 2) If there’s no active contract yet, still allow banners to show once
	if (!hasActive)
	{
		if (bannerFramesLeft > 0 && !bannerText.empty())
		{
			ensureHeaderMessageList();

			int colorIdx = uiHeaderColor;
			if (colorIdx < 0) { colorIdx = std::max(ColorScheme::FindIndex("LightGrey", 53), 0); }

			constexpr int kHeaderId = 1;
			if (auto* slot = HeaderML->GetMessage(kHeaderId))
			{
				if (wcsncmp(slot, bannerText.c_str(), 160) != 0)
					wcsncpy_s(slot, 160 + 1, bannerText.c_str(), _TRUNCATE);
			}
			else
			{
				HeaderML->AddMessage(nullptr, kHeaderId, bannerText.c_str(),
									 colorIdx, static_cast<TextPrintType>(uiHeaderStyle),
									 0x7FFFFFFF, true);
			}
			HeaderML->Draw();
		}
		return;
	}

		// === Normal header (current contract + timer) ===
	const ContractDef& cd = Contracts[activeContractIndex];

	std::wstring header = L"Contract ";
	header += std::to_wstring(activeContractIndex + 1);
	header += L": ";

	std::wstring desc = ExpandContractText(cd.textTemplate, cd, activeRequired);
	header += desc;

	if (uiShowTimer)
	{
		const int framesLeft = std::max<int>(0, (int)(timerEndFrame - now));
		const int sec = framesLeft / 15;
		wchar_t tb[32];
		_snwprintf_s(tb, _TRUNCATE, L"  (%d:%02d)", sec / 60, sec % 60);
		header += tb;
	}

	const std::wstring& headerToDraw = (bannerFramesLeft > 0 && !bannerText.empty())
		? bannerText
		: header;

	ensureHeaderMessageList();

	int colorIdx = uiHeaderColor;
	if (colorIdx < 0)
	{
		colorIdx = std::max(ColorScheme::FindIndex("LightGrey", 53), 0);
	}

	constexpr int kHeaderId = 1;
	if (auto* slot = HeaderML->GetMessage(kHeaderId))
	{
		if (wcsncmp(slot, headerToDraw.c_str(), 160) != 0)
		{
			wcsncpy_s(slot, 160 + 1, headerToDraw.c_str(), _TRUNCATE);
		}
	}
	else
	{
		HeaderML->AddMessage(
			/*Owner*/ nullptr,
			/*ID*/ kHeaderId,
			/*Text*/ headerToDraw.c_str(),
			/*ColorIdx*/ colorIdx,
			/*Style*/ static_cast<TextPrintType>(uiHeaderStyle),
			/*TTL*/ 0x7FFFFFFF,
			/*unknown*/ true
		);
	}
	HeaderML->Draw();

	// --- rows (unchanged) ---
	const int lineH = 12;
	const int yRows = uiY + std::max(lineH, uiHeaderHeight);
	const size_t lines = Competitors.size();
	ensureRowsMessageList(uiX, yRows, uiHeaderWidth, lines);

	const int req = std::max(1, activeRequired);
	for (size_t i = 0; i < lines; ++i)
	{
		const int msgId = 100 + static_cast<int>(i);
		const auto& comp = Competitors[i];

		HouseClass* rep = PickTeamRep(comp);
		const int prog = std::min<int>(comp.progress, req);
		const wchar_t* label = tryHouseName(rep);
		wchar_t line[160];
		if (label && *label)
		{
			_snwprintf_s(line, _TRUNCATE, L"%ls: %d / %d", label, prog, req);
		}
		else
		{
			_snwprintf_s(line, _TRUNCATE, L"Team %zu: %d / %d", i + 1, prog, req);
		}

		int scheme0 = SchemeIdx0_FromHouse(rep);
		if (scheme0 < 0) { scheme0 = SchemeIdx0_Default(); }

		if (auto* slot = RowsML->GetMessage(msgId))
		{
			if (wcsncmp(slot, line, 160) != 0)
			{
				wcsncpy_s(slot, 160 + 1, line, _TRUNCATE);
			}
		}
		else
		{
			RowsML->AddMessage(nullptr, msgId, line, scheme0,
							   static_cast<TextPrintType>(uiHeaderStyle),
							   0x7FFFFFFF, true);
		}
	}
	RowsML->Draw();
}

// ---------- events ----------

void Manager::OnFrame()
{
	if (intermissionEndFrame < 0                 // not in an intermission window
	&& activeContractIndex < 0               // no active contract yet
	&& !Contracts.empty() && !Rewards.empty()
	&& gContractsEnabled)
	{
		// capture per-match RNG once all peers are in lockstep at this frame
		CaptureMatchSeedIfDue((int64_t)Unsorted::CurrentFrame);

		if (!SeedsReady)
		{
			InitMatchSeed(); // derives salts from defs + MatchSeed (just captured)
		}

		startNewContract();  // sets activeContractIndex, timers, UI, etc.
		return;              // optional: skip the rest of this frame for cleanliness
	}

	if (PendingApplyAfterLoad)
	{
		const int now = (int)Unsorted::CurrentFrame;
		for (auto const& p : PendingTransientSWs)
		{
			auto* owner = HouseByIdx(p.ownerIdx);
			auto* type = SWTypeByIdx(p.typeIdx);
			if (!owner || !type) continue;

			// find/create the SuperClass safely now that the world is fully loaded
			SuperClass* sw = nullptr;
			for (int i = 0; i < owner->Supers.Count; ++i)
			{
				if (auto* s2 = owner->Supers.Items[i]; s2 && s2->Type == type) { sw = s2; break; }
			}
			if (!sw)
			{
				sw = new SuperClass(type, owner);
				owner->Supers.AddItem(sw);
			}
			TransientSWs.push_back({ sw, now + std::max(0, p.expireFramesLeft) });
		}
		PendingTransientSWs.clear();
		PendingApplyAfterLoad = false;

		if (!PendingTeamsMembersIdx.empty())
		{
			Competitors.resize(PendingTeamsMembersIdx.size());
			for (size_t i = 0; i < PendingTeamsMembersIdx.size(); ++i)
			{
				Competitors[i].members.clear();
				for (int idx : PendingTeamsMembersIdx[i])
					if (auto* h = HouseByIdx(idx))
						Competitors[i].members.push_back(h);
			}
			PendingTeamsMembersIdx.clear();
		}

		// rebuild money map now that we can resolve houses
		if (!PendingMoneyByHouse.empty())
		{
			moneyDuringContract.clear();
			for (auto const& kv : PendingMoneyByHouse)
				if (auto* h = HouseByIdx(kv.first))
					moneyDuringContract[h] = kv.second;
			PendingMoneyByHouse.clear();
		}

		// we’re done with all post-load work
		PendingApplyAfterLoad = false;

		// NEW: force a clean rebuild of UI on next frame, so rows recolor
		if (HeaderML) { delete HeaderML; HeaderML = nullptr; }
		if (RowsML) { delete RowsML;   RowsML = nullptr; }
		return; // optional: skip the rest of this frame to be extra safe
	}
	if (!gContractsEnabled) return;
	TickTransientSWs();
	if (bannerFramesLeft > 0) { --bannerFramesLeft; }
	if (activeContractIndex < 0) return;

	const int64_t now = Unsorted::CurrentFrame;

	// If the contract expired and no intermission has been scheduled yet, schedule it.
	if (intermissionEndFrame < 0 && now >= timerEndFrame)
	{
		bannerText = L"Contract expired.";
		bannerFramesLeft = intermissionFrames;            // ← 45s (or rules override)
		intermissionEndFrame = timerEndFrame + intermissionFrames;
	}

	// Catch up: if intermission has elapsed (or we froze past it), start next(s).
	int safety = 0;
	while (intermissionEndFrame >= 0 && now >= intermissionEndFrame && safety++ < 128)
	{
		startNewContract(); // this clears intermissionEndFrame to -1

		// If we froze long enough that the *new* window is already over, schedule its intermission immediately
		if (now >= timerEndFrame)
		{
			intermissionEndFrame = timerEndFrame + intermissionFrames;
		}
		else
		{
			intermissionEndFrame = -1; // remain in active window
		}
	}
}


void Manager::OnKill(TechnoClass* victim, HouseClass* killerOwner, bool victimIsBuilding)
{
	if (intermissionEndFrame >= 0) return;
	if (!gContractsEnabled) return; // NEW guard
	if (activeContractIndex < 0) return;
	const auto& cd = Contracts[activeContractIndex];

	if (cd.kind == ContractKind::KillUnits && victimIsBuilding) return;
	if (cd.kind == ContractKind::KillBuildings && !victimIsBuilding) return;
	if (cd.kind != ContractKind::KillUnits && cd.kind != ContractKind::KillBuildings) return;

	// --- prevent farming with own (or allied) kills ---
	HouseClass* victimOwner = victim ? victim->Owner : nullptr;
	if (!killerOwner || !victimOwner)
		return; // no valid ownership info -> don't count

	// block self-kills outright
	if (victimOwner == killerOwner)
		return;

	// OPTIONAL: if you also want to block allied kills, uncomment:
	 if (alliedMutual(victimOwner, killerOwner))
	    return;

	// OPTIONAL: if you want to ignore neutral/civilian kills:
	 if (!IsCompetitiveHouse(victimOwner))
	     return;

	if (!cd.types.empty())
	{
		auto* vt = victim ? victim->GetTechnoType() : nullptr;
		bool ok = false;
		for (auto* t : cd.types) if (t == vt) { ok = true; break; }
		if (!ok) return;
	}

	for (auto& c : Competitors)
	{
		bool belongs = false;
		for (auto* h : c.members) if (h == killerOwner) { belongs = true; break; }
		if (!belongs) continue;

		if (++c.progress >= activeRequired)
		{
			if (const auto* rw = pickRewardSync()) applyReward(rw, c);
			BeginIntermissionNow();    // <-- schedule the gap
			return;
		}
		break;
	}
}

void Manager::OnBuild(BuildingClass* built)
{
	if (intermissionEndFrame >= 0) return;
	if (!gContractsEnabled) return;
	if (activeContractIndex < 0 || !built) return;

	const auto& cd = Contracts[activeContractIndex];
	if (cd.kind != ContractKind::FirstBuild) return;

	// If Types list is given, only count those. If empty, you can either:
	//   (A) return (current behavior), or
	//   (B) count any building. Uncomment next line to allow any:
	// if (cd.types.empty()) { /* count any */ } else
	if (!cd.types.empty())
	{
		auto* bt = built->Type;
		bool ok = false;
		for (auto* t : cd.types) if (t == bt) { ok = true; break; }
		if (!ok) return;
	}
	else
	{
		// Keep existing semantics: no types => don't count.
		return;
	}

	// Add +1 for the team that owns this building
	HouseClass* owner = built->Owner;
	for (auto& c : Competitors)
	{
		bool belongs = false;
		for (auto* h : c.members) if (h == owner) { belongs = true; break; }
		if (!belongs) continue;

		// cumulative progress instead of "instant complete"
		++c.progress;

		if (c.progress >= activeRequired)
		{
			if (const auto* rw = pickRewardSync()) applyReward(rw, c);
			BeginIntermissionNow();    // <-- schedule the gap
			return;
		}
		return;
	}
}

void Manager::OnInfiltration(HouseClass* infiltrator, BuildingClass* /*target*/)
{
	if (intermissionEndFrame >= 0) return;
	if (!gContractsEnabled) return; // NEW guard
	if (activeContractIndex < 0) return;
	const auto& cd = Contracts[activeContractIndex];
	if (cd.kind != ContractKind::Infiltrate) return;

	for (auto& c : Competitors)
	{
		bool belongs = false; for (auto* h : c.members) if (h == infiltrator) { belongs = true; break; }
		if (!belongs) continue;

		c.progress = activeRequired = 1;
		if (const auto* rw = pickRewardSync()) applyReward(rw, c);
		BeginIntermissionNow();    // <-- schedule the gap
		return;

	}
}

void Manager::OnMoney(HouseClass* house, int amount)
{
	if (intermissionEndFrame >= 0) return;
	if (!gContractsEnabled) return; // NEW guard
	if (activeContractIndex < 0 || amount <= 0) return;
	const auto& cd = Contracts[activeContractIndex];
	if (cd.kind != ContractKind::EarnMoney) return;

	moneyDuringContract[house] += amount;

	for (auto& c : Competitors)
	{
		bool belongs = false; for (auto* h : c.members) if (h == house) { belongs = true; break; }
		if (!belongs) continue;

		c.progress += amount;
		if (c.progress >= activeRequired)
		{
			if (const auto* rw = pickRewardSync()) applyReward(rw, c);
			BeginIntermissionNow();    // <-- schedule the gap
			return;
		}
		return;
	}
}

// ---------- winner label helper ----------

// NOTE: This helper avoids touching private Manager members (like tryHouseName),
// so it compiles without header changes. It shows “Team N” for multi-house teams
// and “Player N” for single-house winners.
static std::wstring WinnerLabel(const Contracts::Manager& mgr, const Competitor& comp)
{
	HouseClass* rep = PickTeamRep(comp);

	// Prefer a stable “Team N” label when it’s a multi-house team
	int teamIdx = -1;
	for (size_t i = 0; i < mgr.Competitors.size(); ++i)
	{
		if (&mgr.Competitors[i] == &comp) { teamIdx = static_cast<int>(i); break; }
	}
	if (mgr.activeIsPerTeam && comp.members.size() > 1)
	{
		return (teamIdx >= 0)
			? (L"Team " + std::to_wstring(teamIdx + 1))
			: L"Team";
	}

	// Single house → “Player N” (spawn slot, then ArrayIndex as fallback)
	const int pn = PlayerNumberFromHouse(rep);
	return L"Player " + std::to_wstring(std::max(1, pn));
}

// ---------- rewards ----------

const RewardDef* Manager::pickRewardSync() const
{
	if (Rewards.empty() || totalRewardWeight <= 0)
	{
		Debug::Log("[Contracts][pickReward] EMPTY (totalWeight=%d)", totalRewardWeight);
		return nullptr;
	}

	// derive ticket deterministically from the *current* contract epoch
	int ticket = DeterministicWeightedPick(totalRewardWeight, RollSaltRewards, ActiveContractID);
	Debug::Log("[Contracts][pickReward] epoch=%u ticket=%d of %d (rewards=%zu)",
			ActiveContractID, ticket, totalRewardWeight, Rewards.size());

	for (const auto& rw : Rewards)
	{
		if ((ticket -= rw.weight) <= 0)
		{
			Debug::Log("[Contracts][pickReward] picked='%s' kind=%d",
				rw.id.c_str(), (int)rw.kind);
			return &rw;
		}
	}
	Debug::Log("[Contracts][pickReward] fell-through, using last");
	return &Rewards.back();
}

static SuperClass* FindSuper(HouseClass* h, SuperWeaponTypeClass* t)
{
	if (!h || !t) return nullptr;
	for (int i = 0; i < h->Supers.Count; ++i)
	{
		auto* s = h->Supers.Items[i]; // or operator[]
		if (s && s->Type == t) return s;
	}
	return nullptr;
}

void Manager::applyReward(const RewardDef* rw, const Competitor& winner)
{
	if (!rw) return;

	const std::wstring winnerTxt = WinnerLabel(*this, winner);

	switch (rw->kind)
	{
	case RewardKind::Money:
	{
		for (auto* h : winner.members)
		{
			h->GiveMoney(rw->moneyAmount);
		}
		const std::wstring pretty = ExpandRewardText(*rw);
		bannerText = L"Contract complete! " + winnerTxt + L" - " + pretty;
		bannerFramesLeft = intermissionFrames;

		if (HouseClass* owner = PickDeterministicOwner(winner))
		{
			BroadcastRewardPopup(owner, L"[Contracts] " + winnerTxt + L" received: " + pretty);
		}
		if (rw->soundIndex >= 0)
		{
			PlayGlobal_Safe(rw->soundIndex, 0x2000, 1.0f);
		}
	} break;

	case RewardKind::Unit:
	{
		if (!rw->unitType || rw->unitCount <= 0) break;
		for (auto* h : winner.members)
		{
			CellStruct spawnCell = h->Base.Center;
			const auto spawnCoord = CellToCoord(spawnCell);

			for (int i = 0; i < rw->unitCount; ++i)
			{
				if (auto* obj = rw->unitType->CreateObject(h))
				{
					if (auto* tc = abstract_cast<TechnoClass*>(obj))
					{
						tc->Unlimbo(spawnCoord, DirType::North);
					}
				}
			}
		}
		const std::wstring pretty = ExpandRewardText(*rw);
		bannerText = L"Contract complete! " + winnerTxt + L" - " + pretty;
		bannerFramesLeft = intermissionFrames;

		if (HouseClass* owner = PickDeterministicOwner(winner))
		{
			BroadcastRewardPopup(owner, L"[Contracts] " + winnerTxt + L" received: " + pretty);
		}
		if (rw->soundIndex >= 0)
		{
			PlayGlobal_Safe(rw->soundIndex, 0x2000, 1.0f);
		}
	} break;

	case RewardKind::SuperWeapon:
	{
		Debug::Log("[Contracts][SW] begin: rw='%s' swType=%p idx=%d",
		   rw->id.c_str(), rw->swType, SWIdx(rw->swType));

		if (!rw->swType)
		{
			Debug::Log("[Contracts][SW] abort: swType=null");
			break;
		}

		HouseClass* owner = PickDeterministicOwner(winner);
		if (!owner)
		{
			Debug::Log("[Contracts][SW] abort: owner=null");
			break;
		}

		// Now it's safe to build the pretty text & announce
		const std::wstring pretty = ExpandRewardText(*rw);
		bannerText = L"Contract complete! " + winnerTxt + L" - " + pretty;
		bannerFramesLeft = intermissionFrames;

		BroadcastRewardPopup(owner, L"[Contracts] " + winnerTxt + L" received: " + pretty);

		Debug::Log("[Contracts][SW] owner=%p idx=%d human=%d powered=%d",
			owner, owner->ArrayIndex, owner->IsHumanPlayer ? 1 : 0,
			owner->Is_Powered() ? 1 : 0);

		// Try to find an existing instance for this house/type.
		SuperClass* sw = FindSuper(owner, rw->swType);
		Debug::Log("[Contracts][SW] FindSuper => %p", sw);

		const bool created = (sw == nullptr);
		if (created)
		{
			sw = new SuperClass(rw->swType, owner);
			owner->Supers.AddItem(sw);
			Debug::Log("[Contracts][SW] created new SuperClass and added to house->Supers: sw=%p count=%d",
				sw, owner->Supers.Count);
		}

		// Snapshot state before mutating it
		Debug::Log("[Contracts][SW] pre: IsPresent=%d IsOneTime=%d IsReady=%d IsSuspended=%d CanHold=%d",
			sw->IsPresent ? 1 : 0, sw->IsOneTime ? 1 : 0, sw->IsReady ? 1 : 0,
			sw->IsSuspended ? 1 : 0, sw->CanHold ? 1 : 0);

		// Make it owned/available NOW (no firing here)
		const bool grantChanged = sw->Grant(/*oneTime*/ true, /*announce*/ true, /*onHold*/ false);
		Debug::Log("[Contracts][SW] Grant(oneTime=1,announce=1,onHold=0) -> changed=%d", grantChanged ? 1 : 0);

		const bool holdChanged = sw->SetOnHold(false);
		Debug::Log("[Contracts][SW] SetOnHold(false) -> changed=%d", holdChanged ? 1 : 0);

		sw->SetRechargeTime(0);
		sw->SetReadiness(true);

		if (rw->soundIndex >= 0)
		{
			PlayGlobal_Safe(rw->soundIndex, 0x2000, 1.0f);
		}

		// Snapshot after
		Debug::Log("[Contracts][SW] post: IsPresent=%d IsOneTime=%d IsReady=%d IsSuspended=%d CanHold=%d Name='%ls'",
			sw->IsPresent ? 1 : 0, sw->IsOneTime ? 1 : 0, sw->IsReady ? 1 : 0,
			sw->IsSuspended ? 1 : 0, sw->CanHold ? 1 : 0, sw->NameReadiness());

		// Optional diagnostic: can the game fire it *if asked*?
		const int canFire = sw->CanFire(); // 0 = no, nonzero = ok
		Debug::Log("[Contracts][SW] CanFire() -> %d (Type.Action=%d Manual=%d Powered=%d)",
			canFire, rw->swType->Action, rw->swType->ManualControl ? 1 : 0,
			rw->swType->IsPowered ? 1 : 0);

		Debug::Log("[Contracts][SW] done.");
	} break;

	}
}

ContractsSave Manager::CaptureForSave() const
{
	ContractsSave out;

	const int64_t now = Unsorted::CurrentFrame;

	// toggles / seeds
	out.enabled = gContractsEnabled;
	out.seedsReady = SeedsReady;
	out.matchSeedCaptured = MatchSeedCaptured;
	out.matchSeed = MatchSeed;
	out.gameSyncSeed = GameSyncSeed;
	out.rollSaltContracts = RollSaltContracts;
	out.rollSaltRewards = RollSaltRewards;

	// selection / epoch
	out.activeContractIndex = activeContractIndex;
	out.nextContractID = NextContractID;
	out.activeContractID = ActiveContractID;
	out.activeIsPerTeam = activeIsPerTeam;
	out.activeRequired = activeRequired;

	// timing — store relative frames
	if (intermissionEndFrame >= 0)
	{
		out.remainingIntermissionFrames = (int32_t)std::max<int64_t>(0, intermissionEndFrame - now);
		out.remainingContractFrames = 0;
		out.contractSpanFrames = 0; // not used in intermission
	}
	else
	{
		const int32_t remain = (int32_t)std::max<int64_t>(0, timerEndFrame - now);
		out.remainingContractFrames = remain;
		out.remainingIntermissionFrames = -1;
		// derive span of the active contract (needed to reconstruct start frame)
		int frames = 0;
		if (activeContractIndex >= 0 && activeContractIndex < (int)Contracts.size())
		{
			frames = std::max(1, Contracts[activeContractIndex].timerSeconds) * 15;
		}
		out.contractSpanFrames = frames;
	}

	// UI
	out.bannerFramesLeft = bannerFramesLeft;
	out.bannerText = bannerText;

	// competitors
	out.teams.clear(); out.teams.reserve(Competitors.size());
	for (auto const& c : Competitors)
	{
		ContractsSave::Team t;
		t.progress = c.progress;
		t.members.reserve(c.members.size());
		for (auto* h : c.members) t.members.push_back(HouseIdx(h));
		out.teams.push_back(std::move(t));
	}

	// money map
	out.moneyByHouse.clear();
	out.moneyByHouse.reserve(moneyDuringContract.size());
	for (auto const& kv : moneyDuringContract)
	{
		out.moneyByHouse.emplace_back(HouseIdx(kv.first), kv.second);
	}

	// transient SWs
	out.transients.clear();
	out.transients.reserve(TransientSWs.size());
	for (auto const& e : TransientSWs)
	{
		ContractsSave::TransientSW t {};
		HouseClass* owner = e.SW ? e.SW->Owner : nullptr;  // get owner from the SuperClass
		t.ownerIdx = HouseIdx(owner);
		t.typeIdx = SWTypeIdx(e.SW ? e.SW->Type : nullptr);
		t.expireFramesLeft = (int32_t)std::max(0, e.ExpireFrame - (int)now);
		out.transients.push_back(t);
	}

	return out;
}

void Manager::ApplyFromSave(const ContractsSave& s)
{
	// toggles / seeds
	gContractsEnabled = s.enabled;
	SeedsReady = s.seedsReady;
	MatchSeedCaptured = s.matchSeedCaptured;
	MatchSeed = s.matchSeed;
	GameSyncSeed = s.gameSyncSeed;
	RollSaltContracts = s.rollSaltContracts;
	RollSaltRewards = s.rollSaltRewards;

	// selection / epoch
	activeContractIndex = s.activeContractIndex;
	NextContractID = s.nextContractID;
	ActiveContractID = s.activeContractID;
	activeIsPerTeam = s.activeIsPerTeam;
	activeRequired = s.activeRequired;

	if (activeContractIndex < -1 || activeContractIndex >= (int)Contracts.size())
	{
		activeContractIndex = -1;
	}

	// UI
	bannerFramesLeft = s.bannerFramesLeft;
	bannerText = s.bannerText;

	// rebuild competitors
	PendingTeamsMembersIdx.clear();
	PendingTeamsMembersIdx.reserve(s.teams.size());
	for (auto const& t : s.teams)
		PendingTeamsMembersIdx.push_back(t.members);

	// still store progress now (we'll attach members later)
	Competitors.clear();
	Competitors.resize(s.teams.size());
	for (size_t i = 0; i < s.teams.size(); ++i)
		Competitors[i].progress = s.teams[i].progress;

	// defer money map for the same reason (indexes → houses later)
	PendingMoneyByHouse = s.moneyByHouse;

	// rebuild transient SWs
	PendingTransientSWs.clear();
	for (auto const& t : s.transients)
	{
		if (t.ownerIdx >= 0 && t.typeIdx >= 0)
		{
			PendingTransientSWs.push_back({ t.ownerIdx, t.typeIdx, t.expireFramesLeft });
		}
	}
	PendingApplyAfterLoad = true;

	// timers — reconstruct from relative frames
	if (s.remainingIntermissionFrames >= 0)
	{
		intermissionEndFrame = Unsorted::CurrentFrame + s.remainingIntermissionFrames;
		// keep last window frames just for reference; UI path ignores during intermission
		currentContractStartFrame = -1;
		timerEndFrame = Unsorted::CurrentFrame; // cosmetic clamp
	}
	else
	{
		intermissionEndFrame = -1;
		const int64_t now64 = Unsorted::CurrentFrame;
		timerEndFrame = now64 + s.remainingContractFrames;
		currentContractStartFrame = (s.contractSpanFrames > 0)
			? (timerEndFrame - s.contractSpanFrames)
			: (now64); // fallback
	}

	// force UI lists to rebuild on next draw
	HeaderML = nullptr;
	RowsML = nullptr;

	// IMPORTANT: do not start contracts or play sounds here.
}
