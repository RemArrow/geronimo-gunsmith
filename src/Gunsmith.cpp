// Gunsmith - every attachment changes how the gun handles, the way it would for real.
//
//   Weight     every attachment has its real weight (mount included). Weight held far from the pistol grip
//              (a can, a big scope, a light at the end of the rail) is felt most: the gun swings slower and
//              steadier in your hands, and kicks less (a heavier gun recoils less).
//   Recoil     muzzle devices and grips change muzzle rise, sideways recoil, roll, the push into the shoulder and
//              how fast the gun settles.
//   Sound      each suppressor quiets the shot as much as the real one (the AI hears it); a brake makes it louder.
//   Velocity   muzzle devices can change the round's speed.
//   Precision  each gun groups like the real one (MOA) instead of the game's laser-straight shots.
//   Glint      big scopes catch the sun: players and AI-held scopes facing you throw a glint you can spot.
//   Station    the gun station lists what each attachment does under its name.
//
// The game's own numbers (on AGunReceiver, its grip script GS_Gun and the motion-smoothing grip script GS_Lerp)
// are the base; every machine works the same numbers out from the replicated attachments, so multiplayer needs
// no traffic of its own. Rounds are fired by the host, so velocity and precision are the host's.
#define NOMINMAX
#include <GML/GML.hpp>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "defaults.h"

using namespace gml;

#define GUNSMITH_VERSION "0.1.0"
GML_PLUGIN("com.remarrow.gunsmith", "Gunsmith", GUNSMITH_VERSION);

struct Vec { double x, y, z; };
struct Rot { double pitch, yaw, roll; };
static Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec operator*(Vec a, double k) { return {a.x * k, a.y * k, a.z * k}; }
static double Dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static double Len(Vec a) { return std::sqrt(Dot(a, a)); }
static Vec Unit(Vec a) { double l = Len(a); return l > 1e-9 ? a * (1 / l) : Vec{0, 0, 0}; }

static ConfigEntry<bool> s_enabled, s_recoil, s_weightRecoil, s_handling, s_sound, s_velocity, s_precision, s_glint, s_glintSun,
    s_station, s_log;
static ConfigEntry<float> s_recoilStrength, s_handlingStrength, s_precisionScale, s_glintStrength;
#define DLOG(...) do { if (s_log.Value()) Log(__VA_ARGS__); } while (0)

// ------------------------------------------------------------------ tables

struct Stats {
    double weight = 0;  // attachments: grams; guns: kg
    double vertical = 0, horizontal = 0, roll = 0, kick = 0, recovery = 0, velocity = 0, precision = 0;  // %
    double sound = 0;   // dB
    double glint = 0;
    double moa = -1;    // guns
    std::string note;
    bool has = false;
};
static std::map<std::string, Stats> s_table;           // section name -> stats
static std::vector<std::pair<std::string, std::string>> s_gunOrder;  // [Gun x] in file order: match -> section

static std::string Trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
static void ParseIni(const std::string& text, const char* source) {
    std::istringstream in(text);
    std::string line, section;
    int n = 0;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            if (section.rfind("Gun ", 0) == 0) {
                std::string match = Trim(section.substr(4));
                bool known = false;
                for (auto& g : s_gunOrder) known |= g.second == section;
                if (!known) s_gunOrder.push_back({match, section});
            }
            s_table[section].has = true;
            n++;
            continue;
        }
        size_t eq = line.find('=');
        if (section.empty() || eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq)), val = Trim(line.substr(eq + 1));
        if (size_t c = val.find(';'); c != std::string::npos) val = Trim(val.substr(0, c));
        Stats& s = s_table[section];
        if (key == "Note") { s.note = val; continue; }
        char* end = nullptr;
        double v = strtod(val.c_str(), &end);
        if (val.empty() || (end && *end)) { Warn("{}: [{}] {} = '{}' is not a number - ignored", source, section, key, val); continue; }
        if (key == "Weight") s.weight = v;
        else if (key == "Vertical") s.vertical = v;
        else if (key == "Horizontal") s.horizontal = v;
        else if (key == "Roll") s.roll = v;
        else if (key == "Kick") s.kick = v;
        else if (key == "Recovery") s.recovery = v;
        else if (key == "Velocity") s.velocity = v;
        else if (key == "Precision") s.precision = v;
        else if (key == "Sound") s.sound = v;
        else if (key == "Glint") s.glint = v;
        else if (key == "MOA") s.moa = v;
        else Warn("{}: [{}] unknown key '{}'", source, section, key);
    }
    Log("{}: {} sections", source, n);
}
static std::string ReadFile(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static void LoadTables() {
    s_table.clear();
    s_gunOrder.clear();
    ParseIni(kDefaultsIni, "built-in table");
    std::wstring defaults = PluginPath(L"defaults.ini"), overrides = PluginPath(L"overrides.ini");
    if (ReadFile(defaults) != kDefaultsIni) std::ofstream(defaults, std::ios::binary) << kDefaultsIni;
    if (GetFileAttributesW(overrides.c_str()) == INVALID_FILE_ATTRIBUTES)
        std::ofstream(overrides, std::ios::binary)
            << "; Gunsmith overrides - read after defaults.ini (same folder), which lists every attachment and gun.\n"
               "; Copy a section here and change what you like; a key you leave out keeps its default. Example:\n"
               ";\n"
               "; [PEQ15_C]\n"
               "; Weight = 250\n";
    else ParseIni(ReadFile(overrides), "overrides.ini");
}

static std::string ClassName(Object o) { return o ? Object((GUObject*)o.Class()).Name() : ""; }

// The stats an attachment brings: its own section, else the default for its kind.
static const Stats& StatsOfClass(GUClass* cls, Object sample) {
    static const Stats none;
    std::string name = Object((GUObject*)cls).Name();
    auto it = s_table.find(name);
    if (it != s_table.end() && it->second.has) return it->second;
    Object cdo = sample ? sample : DefaultObject(cls);
    const char* kind = "Rail";
    if (cdo.IsA("GunAttachment_Muzzle")) kind = cdo.GetBool("bIsSuppressor") ? "Default Suppressor" : "Default Muzzle";
    else if (cdo.IsA("GunAttachment_Optic")) {
        Object oc = cdo.GetObj("OpticComponent");
        auto* mags = oc ? (GML_TArray*)oc.PropPtr("MagnificationSettings") : nullptr;
        float top = 1;
        for (int i = 0; mags && i < mags->Num; i++) top = std::max(top, ((float*)mags->Data)[i]);
        kind = oc && oc.GetBool("bIsMagnifier") ? "Default Magnifier" : top > 1.5f ? "Default Magnified" : "Default RedDot";
    } else if (cdo.IsA("GunAttachment_Bipod")) kind = "Default Bipod";
    else if (cdo.IsA("GunAttachment_Underbarrel")) kind = "Default Underbarrel";
    else if (cdo.IsA("GunAttachment_LaserLight")) kind = "Default LaserLight";
    else kind = "Default Rail";
    it = s_table.find(kind);
    return it != s_table.end() ? it->second : none;
}

struct GunSpec { double kg = 3.2, moa = 2.5; std::string section; };
static GunSpec SpecOf(Object gun) {
    GunSpec g;
    std::string cls = ClassName(gun);
    const char* fallback = gun.IsA("Handgun") ? "Default Pistol" : gun.IsA("Shotgun") ? "Default Shotgun" : "Default Rifle";
    g.section = fallback;
    for (auto& [match, section] : s_gunOrder)
        if (cls.find(match) != std::string::npos) { g.section = section; break; }
    auto it = s_table.find(g.section);
    if (it != s_table.end()) {
        if (it->second.weight > 0) g.kg = it->second.weight;
        if (it->second.moa >= 0) g.moa = it->second.moa;
    }
    return g;
}

// ------------------------------------------------------------------ geometry

static Vec LocalOf(Object actor, Vec world) {
    static GML_PropInfo ti{};
    if (!ti.size)
        API->GetPropertyInfo(API->FindProperty((GUStruct*)API->FindFunction((GUStruct*)FindClass("Actor"), "GetTransform"), "ReturnValue"), &ti);
    Params gt(actor, "GetTransform");
    gt.Call();
    Params inv(Lib("KismetMathLibrary"), "InverseTransformLocation");
    std::memcpy(inv.Ptr("T"), gt.Ptr("ReturnValue"), ti.size);
    inv.Set("Location", world).Call();
    return inv.Return<Vec>();
}
static Vec WorldOf(Object o) {
    Params p(o, o.IsA("Actor") ? "K2_GetActorLocation" : "K2_GetComponentLocation");
    p.Call();
    return p.Return<Vec>();
}

// ------------------------------------------------------------------ a gun's numbers
//
// Every number the plugin changes is kept as the game's ("base") and what the plugin last wrote. When the value
// found is no longer what was written, the game or another plugin (the Barrett and Saiga set their recoil) has set
// it since: that becomes the new base. So nothing ever compounds.

template <class T> struct Tracked {
    T base{}, written{};
    bool init = false;
    const T& Base(const T& now) {
        if (!init || std::memcmp(&now, &written, sizeof(T))) base = now;
        init = true;
        return base;
    }
};

// AGunReceiver from RotationPerShot through RecoveryFactors_Mounted (contiguous; checked at start).
struct Recoil {
    Vec rot, maxRot, loc, maxLoc;   // rotation: X roll, Y pitch (muzzle rise, always up), Z yaw (either way)
    float recH, recV, recX, recYZ;  // recovery speeds
    float stab[12];                 // RecoveryFactors one-handed / two-handed / mounted
};
static_assert(sizeof(Recoil) == 0xA0);
static int s_recoilOffset = -1;

struct Smoothing { float lin, ang, min1, min2; };  // GS_Lerp

struct Effect {
    double massKg = 0, inertia = 0;   // what the attachments add (kg, kg m^2 about the pistol grip)
    double gunKg = 0, gunInertia = 0;
    double kV = 1, kH = 1, kRoll = 1, kKick = 1, kRecovery = 1, kVelocity = 1, kPrecision = 1, kTurn = 1, kMove = 1;
    double suppressorDb = 0, deviceDb = 0;
    bool suppressor = false;
    double moa = 0;
    int count = 0;
};

struct Glint {
    GUObject* optic = nullptr;
    GUObject* comp = nullptr;
    GUObject* mat = nullptr;
    double strength = 0;
    Vec lens{};  // optic space
};

struct Gun {
    std::vector<std::pair<GUObject*, bool>> sig;
    Tracked<Recoil> recoil;
    Tracked<float> loud, supp, spread;
    Tracked<Smoothing> smooth;
    GUObject* lerp = nullptr;
    Effect fx;
    double velocity = 1;
    std::vector<Glint> glints;
    bool reported = false;
};
static std::unordered_map<GUObject*, Gun> s_guns;

static std::vector<Object> Attachments(Object gun) {
    std::vector<Object> out;
    auto* arr = (GML_TArray*)gun.PropPtr("RegisteredAttachments");
    for (int i = 0; arr && i < arr->Num; i++) {
        Object a(((GUObject**)arr->Data)[i]);
        if (a && a.IsA("GunAttachment") && a.GetBool("bAttached")) out.push_back(a);
    }
    return out;
}
static std::vector<std::pair<GUObject*, bool>> Signature(Object gun) {
    std::vector<std::pair<GUObject*, bool>> sig;
    auto* arr = (GML_TArray*)gun.PropPtr("RegisteredAttachments");
    for (int i = 0; arr && i < arr->Num; i++) {
        GUObject* a = ((GUObject**)arr->Data)[i];
        sig.push_back({a, a && API->IsValid(a) && Object(a).GetBool("bAttached")});
    }
    sig.push_back({gun.GetObj("GSLerp").ptr, true});
    sig.push_back({gun.GetObj("GSGun").ptr, true});
    return sig;
}

// The pistol grip, where the hand pivots the gun: the PrimaryHandSocket, else the gun's origin.
static Vec GripOf(Object gun) {
    Params gc(gun, "K2_GetComponentsByClass");  // the returned array leaks a little: called once per change only
    static GUClass* socket = FindClass("PrimaryHandSocket");
    *(GUClass**)gc.Ptr("ComponentClass") = socket;
    gc.Call();
    auto* arr = (GML_TArray*)gc.Ptr("ReturnValue");
    if (arr && arr->Num) return LocalOf(gun, WorldOf(Object(((GUObject**)arr->Data)[0])));
    return {0, 0, 0};
}

static Effect Work(Object gun, Gun& g) {
    Effect fx;
    GunSpec spec = SpecOf(gun);
    fx.gunKg = spec.kg;
    fx.moa = spec.moa;
    // The gun's own moment of inertia about the grip: its mass spread along its length (a rod), centred on its bounds.
    double length = 0.8, cgFromGrip = 0.15;
    Vec grip = GripOf(gun);
    if (Object root = gun.GetObj("StaticMeshComponent")) {
        Params b(root, "GetLocalBounds");
        b.Call();
        Vec mn = b.Get<Vec>("Min"), mx = b.Get<Vec>("Max");
        length = std::max(0.15, (mx.x - mn.x) / 100);
        cgFromGrip = std::fabs((mn.x + mx.x) / 2 - grip.x) / 100;
    }
    fx.gunInertia = spec.kg * (length * length / 12 + cgFromGrip * cgFromGrip);

    double v = 0, h = 0, roll = 0, kick = 0, rec = 0, vel = 0, prec = 0;
    for (Object a : Attachments(gun)) {
        const Stats& s = StatsOfClass(a.Class(), a);
        double kg = s.weight / 1000;
        Vec at = LocalOf(gun, WorldOf(a)) - grip;
        double d = Len(at) / 100;
        fx.massKg += kg;
        fx.inertia += kg * d * d;
        v += s.vertical; h += s.horizontal; roll += s.roll; kick += s.kick; rec += s.recovery; vel += s.velocity; prec += s.precision;
        if (a.IsA("GunAttachment_Muzzle") && a.GetBool("bIsSuppressor")) { fx.suppressor = true; fx.suppressorDb += s.sound; }
        else fx.deviceDb += s.sound;
        fx.count++;
    }
    // A heavier gun recoils less: kick by its mass, rotation by its inertia (square root: the hands absorb part of it).
    double massRatio = fx.gunKg / (fx.gunKg + fx.massKg), turnRatio = fx.gunInertia / (fx.gunInertia + fx.inertia);
    double wLin = s_weightRecoil.Value() ? massRatio : 1, wRot = s_weightRecoil.Value() ? std::sqrt(turnRatio) : 1;
    auto pct = [](double p) { return std::clamp(1 + p / 100, 0.25, 3.0); };
    double k = std::max(0.0f, s_recoilStrength.Value());
    auto strength = [k](double f) { return std::max(0.1, 1 + (f - 1) * k); };
    fx.kV = strength(pct(v) * wRot);
    fx.kH = strength(pct(h) * wRot);
    fx.kRoll = strength(pct(roll) * wRot);
    fx.kKick = strength(pct(kick) * wLin);
    fx.kRecovery = strength(pct(rec));
    fx.kVelocity = pct(vel);
    fx.kPrecision = pct(prec);
    // Handling: the gun turns as fast as its inertia allows and moves as fast as its mass allows; the extra inertia
    // also damps the hands' tremor (the motion filter's minimum cutoff drops with it).
    double hs = std::max(0.0f, s_handlingStrength.Value());
    fx.kTurn = std::max(0.2, 1 + (std::sqrt(turnRatio) - 1) * hs);
    fx.kMove = std::max(0.2, 1 + (std::sqrt(massRatio) - 1) * hs);
    return fx;
}

// The grip script keeps its own copy of the recoil numbers while the gun is held (GS_Gun +0x678 in this build).
// Found by value, so a game update that moves it is followed and a copy that differs is left alone.
static void PatchCopy(Object holder, const void* was, const void* now, size_t size) {
    if (!holder) return;
    int span = API->StructSize(holder.Struct());
    auto* p = (uint8_t*)holder.ptr;
    for (int off = 0x28; off + (int)size <= span; off += 8)
        if (!std::memcmp(p + off, was, size)) {
            std::memcpy(p + off, now, size);
            return;
        }
}

static void WriteRecoil(Object gun, Gun& g) {
    auto* live = (Recoil*)((uint8_t*)gun.ptr + s_recoilOffset);
    Recoil before = *live;
    Recoil r = g.recoil.Base(before);
    if (s_enabled.Value() && s_recoil.Value()) {
        const Effect& f = g.fx;
        r.rot.y *= f.kV; r.maxRot.y *= f.kV;
        r.rot.z *= f.kH; r.maxRot.z *= f.kH;
        r.rot.x *= f.kRoll; r.maxRot.x *= f.kRoll;
        r.loc = r.loc * f.kKick; r.maxLoc = r.maxLoc * f.kKick;
        r.recH = (float)(r.recH * f.kRecovery); r.recV = (float)(r.recV * f.kRecovery);
        r.recX = (float)(r.recX * f.kRecovery); r.recYZ = (float)(r.recYZ * f.kRecovery);
    }
    *live = r;
    g.recoil.written = r;
    if (std::memcmp(&before, &r, sizeof r)) {
        Object gs = gun.GetObj("GSGun");
        PatchCopy(gs, &before, &r, offsetof(Recoil, recH));             // the four vectors
        PatchCopy(gs, &before.recH, &r.recH, sizeof(float) * 4);        // and the recovery speeds, if it keeps them
    }
}

static void WriteFloat(Object o, const char* prop, Tracked<float>& t, float value, bool apply) {
    float now = o.Get<float>(prop);
    float base = t.Base(now);
    float v = apply ? value : base;
    o.Set(prop, v);
    t.written = v;
}

static void WriteSound(Object gun, Gun& g) {
    bool on = s_enabled.Value() && s_sound.Value();
    float loud = gun.Get<float>("WeaponFireNoiseLoudness"), supp = gun.Get<float>("SuppressedWeaponFireNoiseMultiplier");
    g.loud.Base(loud);
    g.supp.Base(supp);
    // dB as the AI hears it: every 25 dB halves the loudness (the game's own suppressor halves it).
    WriteFloat(gun, "WeaponFireNoiseLoudness", g.loud, (float)(g.loud.base * std::pow(2.0, g.fx.deviceDb / 25)), on);
    WriteFloat(gun, "SuppressedWeaponFireNoiseMultiplier", g.supp,
               g.fx.suppressor && g.fx.suppressorDb != 0 ? (float)std::pow(2.0, g.fx.suppressorDb / 25) : g.supp.base, on);
}

static void WritePrecision(Object gun, Gun& g) {
    float now = gun.Get<float>("WeaponSpread");
    float base = g.spread.Base(now);
    bool on = s_enabled.Value() && s_precision.Value() && g.fx.moa > 0;
    // WeaponSpread is the widest angle a round leaves the bore at (radians); a group's size is twice that.
    double halfAngle = g.fx.moa * g.fx.kPrecision * std::max(0.0f, s_precisionScale.Value()) / 2 / 60 * 3.14159265358979 / 180;
    float v = on ? base + (float)halfAngle : base;
    gun.Set("WeaponSpread", v);
    g.spread.written = v;
}

static void WriteHandling(Object gun, Gun& g) {
    Object lerp = gun.GetObj("GSLerp");
    if (!lerp) return;
    if (lerp.ptr != g.lerp) { g.smooth = {}; g.lerp = lerp.ptr; }
    Smoothing now{lerp.Get<float>("LinearLerpSpeed"), lerp.Get<float>("AngularLerpSpeed"),
                  lerp.Get<float>("EuroSmoothingMinCutoff_OneHanded"), lerp.Get<float>("EuroSmoothingMinCutoff_TwoHanded")};
    Smoothing s = g.smooth.Base(now);
    if (s_enabled.Value() && s_handling.Value()) {
        s.lin = (float)(s.lin * g.fx.kMove);
        s.ang = (float)(s.ang * g.fx.kTurn);
        s.min1 = (float)(s.min1 * g.fx.kTurn);
        s.min2 = (float)(s.min2 * g.fx.kTurn);
    }
    lerp.Set("LinearLerpSpeed", s.lin);
    lerp.Set("AngularLerpSpeed", s.ang);
    lerp.Set("EuroSmoothingMinCutoff_OneHanded", s.min1);
    lerp.Set("EuroSmoothingMinCutoff_TwoHanded", s.min2);
    g.smooth.written = s;
}

static void RemoveGlint(Glint& gl) {
    if (gl.comp && API->IsValid(gl.comp)) {
        Params d(Object(gl.comp), "K2_DestroyComponent");
        d.SetObj("Object", Object(gl.comp)).Call();
    }
    gl.comp = nullptr;
}
static void SyncGlints(Object gun, Gun& g);

static std::string Describe(Object gun, const Gun& g) {
    const Effect& f = g.fx;
    auto pc = [](double k) { return std::format("{:+.0f}%", (k - 1) * 100); };
    return std::format("{} [{}]: {} attachment(s), +{:.2f} kg on a {:.2f} kg gun (inertia +{:.0f}%) | recoil: rise {} sideways {} roll {} kick {}"
                       " recovery {} | handling: turn {} move {} | sound {}{:.0f} dB device {:+.0f} dB | velocity {} | {:.2f} MOA",
                       gun.Name(), SpecOf(gun).section, f.count, f.massKg, f.gunKg, f.gunInertia > 0 ? f.inertia / f.gunInertia * 100 : 0,
                       pc(f.kV), pc(f.kH), pc(f.kRoll), pc(f.kKick), pc(f.kRecovery), pc(f.kTurn), pc(f.kMove),
                       f.suppressor ? "suppressed " : "", f.suppressorDb, f.deviceDb, pc(f.kVelocity), f.moa * f.kPrecision);
}

static void Apply(Object gun, Gun& g) {
    g.fx = Work(gun, g);
    g.velocity = s_enabled.Value() && s_velocity.Value() ? g.fx.kVelocity : 1;
    if (s_recoilOffset >= 0) WriteRecoil(gun, g);
    WriteSound(gun, g);
    WritePrecision(gun, g);
    WriteHandling(gun, g);
    SyncGlints(gun, g);
    DLOG("{}", Describe(gun, g));
}

// Rechecks a gun: anything mounted or taken off, a grip script made or swapped, or a number someone else set.
static void Check(Object gun) {
    Gun& g = s_guns[gun.ptr];
    auto sig = Signature(gun);
    bool changed = sig != g.sig;
    if (!changed && s_recoilOffset >= 0 && g.recoil.init &&
        std::memcmp((uint8_t*)gun.ptr + s_recoilOffset, &g.recoil.written, sizeof(Recoil)))
        changed = true;
    if (!changed && g.loud.init && gun.Get<float>("WeaponFireNoiseLoudness") != g.loud.written) changed = true;
    if (!changed && g.spread.init && gun.Get<float>("WeaponSpread") != g.spread.written) changed = true;
    if (!changed) return;
    g.sig = std::move(sig);
    Apply(gun, g);
}

// ------------------------------------------------------------------ velocity
//
// The rounds a shot fires are pooled actors whose speed is set from their own values as the shot spawns them, so
// the speed is changed on the round itself, right after AGunReceiver::FireRoundLocal (native) returns. That runs
// on the host for every player's shots; the round's velocity replicates to everyone else.

static const char* kFireRoundLocalSig =
    "40 55 53 56 57 41 57 48 8D 6C 24 80 48 81 EC 80 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 00 "
    "48 8B 99 A0 05 00 00 49 8B F0 4C 8B FA 48 8B F9";
// The same function once another plugin (Saiga-12K, Barrett M107A1) hooked it.
static const char* kFireRoundLocalHookedSig =
    "E9 ?? ?? ?? ?? 41 57 48 8D 6C 24 80 48 81 EC 80 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 00 "
    "48 8B 99 A0 05 00 00 49 8B F0 4C 8B FA 48 8B F9";
using FireRoundLocalFn = void (*)(GUObject* self, const void* direction, const void* origin);
static FireRoundLocalFn s_fireRoundLocal = nullptr;

static std::vector<GUObject*> s_rounds;  // every round actor seen (the pool), refreshed when a shot makes a new one
static int s_velocityOffset = -1;

static Vec& VelocityOf(GUObject* round) { return *(Vec*)((uint8_t*)round + s_velocityOffset); }

static void FireRoundLocalDetour(GUObject* self, const void* direction, const void* origin) {
    auto it = s_guns.find(self);
    double k = it != s_guns.end() ? it->second.velocity : 1;
    if (k == 1 || s_velocityOffset < 0) { s_fireRoundLocal(self, direction, origin); return; }
    std::vector<std::pair<GUObject*, Vec>> before;
    for (GUObject* r : s_rounds)
        if (API->IsValid(r)) before.push_back({r, VelocityOf(r)});
    s_fireRoundLocal(self, direction, origin);
    int scaled = 0;
    for (auto& [r, v] : before) {
        Vec& now = VelocityOf(r);
        if (now.x != v.x || now.y != v.y || now.z != v.z) { now = now * k; scaled++; }
    }
    if (!scaled) {  // a round the pool had to make
        std::unordered_set<GUObject*> known(s_rounds.begin(), s_rounds.end());
        s_rounds.clear();
        for (Object r : FindAllOf("EBBullet")) {
            s_rounds.push_back(r.ptr);
            if (!known.count(r.ptr)) { VelocityOf(r.ptr) = VelocityOf(r.ptr) * k; scaled++; }
        }
    }
    DLOG("{}: {} round(s) at x{:.3f} speed", Object(self).Name(), scaled, k);
}

static void* FollowJumps(void* p) {  // jmp rel32 / jmp [rip+0] chains, to the code they end at
    for (int i = 0; i < 16; i++) {
        auto* b = (uint8_t*)p;
        if (b[0] == 0xE9) p = b + 5 + *(int32_t*)(b + 1);
        else if (b[0] == 0xFF && b[1] == 0x25) p = *(void**)(b + 6 + *(int32_t*)(b + 2));
        else break;
    }
    return p;
}
static void InstallVelocity() {
    void* target = API->FindPattern(kFireRoundLocalSig);
    if (!target)
        if (void* hooked = API->FindPattern(kFireRoundLocalHookedSig)) {
            target = FollowJumps(hooked);  // chain after the other plugin's detour
            Log("velocity: FireRoundLocal is already hooked by another plugin - chaining after it");
        }
    if (!target) { Warn("AGunReceiver::FireRoundLocal not found (game updated?) - attachments don't change velocity"); return; }
    if (!API->HookNative(target, (void*)&FireRoundLocalDetour, (void**)&s_fireRoundLocal) || !s_fireRoundLocal) {
        Error("could not hook FireRoundLocal - attachments don't change velocity");
        s_fireRoundLocal = nullptr;
        return;
    }
    Log("velocity: FireRoundLocal hooked at exe+0x{:X}", (uintptr_t)target - (uintptr_t)API->ImageBase());
}

// ------------------------------------------------------------------ glint
//
// A small flare at the scope's objective lens, drawn for this machine's camera only. It shows when the lens faces
// you: brightest when the scope sits between you and the sun's reflection (the lens bisects the sun and your eye),
// and a little whenever it points right at you in daylight. It is sized to stay visible far away.

static GUObject* s_glintTex = nullptr;
static GUObject* s_glintParent = nullptr;
static GUObject* s_plane = nullptr;

// A 64x64 white star flare: a soft core and four thin spikes, as a TGA (alpha = brightness).
static GUObject* GlintTexture() {
    if (s_glintTex) return s_glintTex;
    const int n = 64;
    std::vector<uint8_t> tga(18 + n * n * 4, 0);
    tga[2] = 2;
    tga[12] = n; tga[14] = n;
    tga[16] = 32;
    tga[17] = 8 | 0x20;  // 8 alpha bits, top-left origin
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            double dx = (x + 0.5 - n / 2.0) / (n / 2.0), dy = (y + 0.5 - n / 2.0) / (n / 2.0);
            double r = std::sqrt(dx * dx + dy * dy);
            double core = std::exp(-r * r * 30) + 0.35 * std::exp(-r * r * 6);
            double spikes = std::exp(-std::fabs(dx) * 60) * std::max(0.0, 1 - std::fabs(dy)) + std::exp(-std::fabs(dy) * 60) * std::max(0.0, 1 - std::fabs(dx));
            double a = std::clamp(core + 0.6 * spikes, 0.0, 1.0) * std::clamp((1 - r) * 3, 0.0, 1.0);
            uint8_t* p = &tga[18 + (y * n + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(a * 255);
        }
    s_glintTex = API->ImportTextureFromMemory(tga.data(), tga.size(), "gunsmith_glint");
    if (s_glintTex) API->KeepAlive(s_glintTex);
    return s_glintTex;
}
static bool GlintAssets() {
    static bool tried = false;
    if (tried) return s_glintParent && s_plane && s_glintTex;
    tried = true;
    if (API->size < offsetof(GML_API, ImportTextureFromMemory) + sizeof(void*)) { Warn("glint needs GML 2.2 or newer"); return false; }
    s_glintParent = LoadObject("/InterchangeAssets/gltf/MaterialInstances/MI_Unlit_Blend_DS.MI_Unlit_Blend_DS").ptr;
    if (!s_glintParent) s_glintParent = LoadObject("/InterchangeAssets/gltf/MaterialInstances/MI_Unlit_Blend.MI_Unlit_Blend").ptr;
    s_plane = LoadObject("/Engine/BasicShapes/Plane.Plane").ptr;
    GlintTexture();
    if (!s_glintParent || !s_plane || !s_glintTex) Warn("glint assets missing (material {}, plane {}, texture {}) - no glint",
                                                       s_glintParent != nullptr, s_plane != nullptr, s_glintTex != nullptr);
    return s_glintParent && s_plane && s_glintTex;
}

static Object AddGlintComponent(Object optic, Glint& gl) {
    Params root(optic, "K2_GetRootComponent");
    root.Call();
    Object parent = root.Return<GUObject*>();
    if (!parent) return {};
    // The objective lens: the front of the optic's mesh, its centre across, one lens radius below the top.
    Params b(parent, "GetLocalBounds");
    b.Call();
    Vec mn = b.Get<Vec>("Min"), mx = b.Get<Vec>("Max");
    double radius = std::max(0.8, (mx.y - mn.y) / 2);
    gl.lens = {mx.x + 0.2, (mn.y + mx.y) / 2, std::max((mn.z + mx.z) / 2, mx.z - radius)};

    Params mt(Lib("KismetMathLibrary"), "MakeTransform");
    mt.Set("Location", gl.lens).Set("Scale", Vec{0.03, 0.03, 0.03}).Call();
    static GML_PropInfo ti{};
    if (!ti.size)
        API->GetPropertyInfo(API->FindProperty((GUStruct*)API->FindFunction(Lib("KismetMathLibrary").Struct(), "MakeTransform"), "ReturnValue"), &ti);
    Params add(optic, "AddComponentByClass");
    static GUClass* smc = FindClass("StaticMeshComponent");
    *(GUClass**)add.Ptr("Class") = smc;
    add.Set("bManualAttachment", true).Set("bDeferredFinish", false);
    std::memcpy(add.Ptr("RelativeTransform"), mt.Ptr("ReturnValue"), ti.size);
    add.Call();
    Object comp = add.Return<GUObject*>();
    if (!comp) return {};
    Params col(comp, "SetCollisionEnabled");
    col.Set("NewType", (uint8_t)0).Call();
    Params sm(comp, "SetStaticMesh");
    sm.SetObj("NewMesh", Object(s_plane)).Call();
    gl.mat = API->CreateMaterialInstance(s_glintParent);
    if (gl.mat) {
        API->SetMaterialTexture(gl.mat, "BaseColorTexture", s_glintTex);
        float c[4] = {0, 0, 0, 0};
        API->SetMaterialVector(gl.mat, "BaseColorFactor", c);
        Params m(comp, "SetMaterial");
        m.Set("ElementIndex", (int32_t)0).SetObj("Material", Object(gl.mat)).Call();
    }
    Params cs(comp, "SetCastShadow");
    cs.Set("NewCastShadow", false).Call();
    Params att(comp, "K2_AttachToComponent");
    att.SetObj("Parent", parent).Set("LocationRule", (uint8_t)2).Set("RotationRule", (uint8_t)2).Set("ScaleRule", (uint8_t)2)
        .Set("bWeldSimulatedBodies", false).Call();
    Params rel(comp, "K2_SetRelativeLocation");
    rel.Set("NewLocation", gl.lens).Set("bSweep", false).Set("bTeleport", true).Call();
    Params vis(comp, "SetVisibility");
    vis.Set("bNewVisibility", false).Set("bPropagateToChildren", false).Call();
    return comp;
}

static void SyncGlints(Object gun, Gun& g) {
    std::vector<Glint> keep;
    std::vector<Object> optics;
    if (s_enabled.Value() && s_glint.Value())
        for (Object a : Attachments(gun))
            if (a.IsA("GunAttachment_Optic") && StatsOfClass(a.Class(), a).glint > 0) optics.push_back(a);
    for (Glint& gl : g.glints) {
        bool still = API->IsValid(gl.optic) && std::any_of(optics.begin(), optics.end(), [&](Object o) { return o.ptr == gl.optic; });
        if (still) keep.push_back(gl);
        else RemoveGlint(gl);
    }
    for (Object o : optics) {
        if (std::any_of(keep.begin(), keep.end(), [&](const Glint& gl) { return gl.optic == o.ptr; })) continue;
        if (!GlintAssets()) break;
        Glint gl;
        gl.optic = o.ptr;
        gl.strength = StatsOfClass(o.Class(), o).glint;
        Object c = AddGlintComponent(o, gl);
        DLOG("glint on {}: {} (lens at {:.1f} {:.1f} {:.1f})", o.Name(), c ? c.Name() : "could not be added", gl.lens.x, gl.lens.y, gl.lens.z);
        if (c) { gl.comp = c.ptr; keep.push_back(gl); }
    }
    DLOG("{}: {} optic(s) that glint, {} glint(s)", gun.Name(), optics.size(), keep.size());
    g.glints = std::move(keep);
}

static GUObject* s_sun = nullptr;
static Vec SunDirection(bool& up) {  // towards the sun
    if (!s_sun || !API->IsValid(s_sun)) s_sun = FindFirstOf("DirectionalLightComponent").ptr;
    up = false;
    if (!s_sun) return {0, 0, 1};
    Params f(Object(s_sun), "GetForwardVector");
    f.Call();
    Vec toSun = f.Return<Vec>() * -1;
    float intensity = Object(s_sun).Get<float>("Intensity");
    up = toSun.z > 0.02 && intensity > 0.5f && Object(s_sun).GetBool("bVisible");
    return toSun;
}

static void DrawGlints() {
    if (!s_enabled.Value() || !s_glint.Value()) return;
    bool any = false;
    for (auto& [p, g] : s_guns) any |= !g.glints.empty();
    if (!any) return;
    Params cm(Lib("GameplayStatics"), "GetPlayerCameraManager");
    cm.SetObj("WorldContextObject", Object(API->WorldContext())).Set("PlayerIndex", (int32_t)0).Call();
    Object pcm = cm.Return<GUObject*>();
    if (!pcm) return;
    Params cl(pcm, "GetCameraLocation");
    cl.Call();
    Vec cam = cl.Return<Vec>();
    bool sunUp = false;
    Vec sun = SunDirection(sunUp);
    static GUObject* reported = nullptr;
    if (s_sun != reported) {
        reported = s_sun;
        DLOG("sun: {} towards ({:.2f} {:.2f} {:.2f}), {}", s_sun ? Object(s_sun).Name() : "none", sun.x, sun.y, sun.z, sunUp ? "up" : "down");
    }
    bool needSun = s_glintSun.Value();
    double strength = std::max(0.0f, s_glintStrength.Value());
    for (auto& [p, g] : s_guns)
        for (Glint& gl : g.glints) {
            if (!gl.comp || !API->IsValid(gl.comp) || !API->IsValid(gl.optic)) continue;
            Object comp(gl.comp), optic(gl.optic);
            Vec at = WorldOf(comp);
            Params fw(optic, "GetActorForwardVector");
            fw.Call();
            Vec n = fw.Return<Vec>(), toCam = cam - at;
            double dist = Len(toCam);
            Vec v = Unit(toCam);
            double facing = Dot(n, v), intensity = 0;
            if (facing > 0 && dist > 30) {
                double reflect = Dot(n, Unit(sun + v));
                double sunTerm = sunUp && Dot(n, sun) > 0 ? std::pow(std::max(0.0, reflect), 24) : 0;
                double skyTerm = (sunUp || !needSun) ? 0.5 * std::pow(facing, 60) : 0;
                intensity = std::min(1.0, gl.strength * strength * std::max(sunTerm, skyTerm));
            }
            bool show = intensity > 0.03;
            Params vis(comp, "SetVisibility");
            vis.Set("bNewVisibility", show).Set("bPropagateToChildren", false).Call();
            if (!show) continue;
            // Constant angular size (about half a degree) so it reads at any range, bigger when brighter.
            double size = std::clamp(dist * 0.009, 2.5, 900.0) * (0.6 + 0.9 * intensity);
            Params sc(comp, "SetWorldScale3D");
            sc.Set("NewScale", Vec{size / 100, size / 100, size / 100}).Call();
            Params mr(Lib("KismetMathLibrary"), "MakeRotFromZ");
            mr.Set("Z", v).Call();
            Params wr(comp, "K2_SetWorldRotation");
            wr.Set("NewRotation", mr.Return<Rot>()).Set("bSweep", false).Set("bTeleport", true).Call();
            if (gl.mat) {
                float hdr = (float)(4 + 40 * intensity);
                float c[4] = {hdr, hdr, hdr * 0.95f, (float)std::min(1.0, 0.4 + intensity)};
                API->SetMaterialVector(gl.mat, "BaseColorFactor", c);
            }
        }
}

// ------------------------------------------------------------------ the gun station

static std::unordered_set<GUObject*> s_labelled;  // attachment assets whose subtitle already lists their stats

static std::string Summary(const Stats& s) {
    std::vector<std::string> parts;
    if (!s.note.empty()) parts.push_back(s.note);
    if (s.weight > 0) parts.push_back(std::format("{:.0f} g", s.weight));
    std::string recoil;
    auto add = [&](const char* what, double v) { if (v) recoil += std::format("{}{} {:+.0f}%", recoil.empty() ? "" : ", ", what, v); };
    add("rise", s.vertical);
    add("sideways", s.horizontal);
    add("kick", s.kick);
    if (!recoil.empty()) parts.push_back("recoil " + recoil);
    if (s.recovery) parts.push_back(std::format("recovery {:+.0f}%", s.recovery));
    if (s.sound) parts.push_back(std::format("{:+.0f} dB", s.sound));
    if (s.velocity) parts.push_back(std::format("velocity {:+.0f}%", s.velocity));
    if (s.precision) parts.push_back(std::format("groups {:+.0f}%", s.precision));
    if (s.glint > 0) parts.push_back("glints");
    std::string out;
    for (auto& p : parts) out += (out.empty() ? "" : " | ") + p;
    return out;
}

// The data assets live as long as the game, so each is labelled once: its own subtitle, then what it does.
static void LabelStation() {
    if (!s_enabled.Value() || !s_station.Value()) return;
    for (Object a : FindAllOf("GunAttachmentAsset_C")) {
        if (s_labelled.count(a.ptr)) continue;
        void* text = a.PropPtr("Subtitle");
        GUClass* cls = a.Get<GUClass*>("AttachmentClass");
        if (!text || !cls) continue;
        s_labelled.insert(a.ptr);
        char b[512] = "";
        API->TextToString(text, b, sizeof b);
        std::string own = b, sum = Summary(StatsOfClass(cls, Object()));
        if (sum.empty()) continue;
        std::string label = own.empty() ? sum : own + " | " + sum;
        API->MakeText(label.c_str(), text);  // the replaced text's reference is dropped (a few bytes, once per asset)
    }
}

// ------------------------------------------------------------------ scanning

static void Scan(bool discover) {
    for (auto it = s_guns.begin(); it != s_guns.end();)
        if (!API->IsValid(it->first)) it = s_guns.erase(it);
        else ++it;
    if (discover)
        for (Object gun : FindAllOf("GunReceiver")) Check(gun);
    else
        for (auto& [p, g] : s_guns) Check(Object(p));
}

// Development hooks (tests\GunsmithDev).
extern "C" __declspec(dllexport) int Gunsmith_Describe(GUObject* gun, char* buf, int cap) {
    auto it = s_guns.find(gun);
    if (it == s_guns.end() || !buf || cap <= 0) return 0;
    std::string s = Describe(Object(gun), it->second);
    strncpy_s(buf, cap, s.c_str(), _TRUNCATE);
    return 1;
}
extern "C" __declspec(dllexport) void Gunsmith_Check(GUObject* gun) {
    if (gun && API->IsValid(gun)) Check(Object(gun));
}

// ------------------------------------------------------------------ plugin

GML_AWAKE() {
    s_enabled = Config.Bind("General", "Enabled", true, "Attachments change how guns handle.");
    s_recoil = Config.Bind("Recoil", "Enabled", true, "Muzzle devices and grips change recoil (rise, sideways, roll, kick, recovery).");
    s_recoilStrength = Config.Bind("Recoil", "Strength", 1.0f, "Every recoil change, x this (0 = none, 2 = double).");
    s_weightRecoil = Config.Bind("Recoil", "WeightReducesRecoil", true,
        "A heavier gun kicks less: added mass lowers the push into the shoulder, added weight out front lowers muzzle rise.");
    s_handling = Config.Bind("Handling", "Enabled", true,
        "Added weight makes the gun swing slower and steadier in your hands, most of all weight far from the pistol grip.");
    s_handlingStrength = Config.Bind("Handling", "Strength", 1.0f, "How much weight changes handling (0 = not at all).");
    s_sound = Config.Bind("Sound", "Enabled", true,
        "Each suppressor quiets the shot by its own dB for the AI's hearing; a muzzle brake makes it louder.");
    s_velocity = Config.Bind("Velocity", "Enabled", true, "Muzzle devices change the round's muzzle velocity.");
    s_precision = Config.Bind("Precision", "Enabled", true,
        "Guns group like the real ones (each gun's MOA in defaults.ini) instead of every round flying dead straight.");
    s_precisionScale = Config.Bind("Precision", "Scale", 1.0f, "Group size x this (0 = laser-straight, like the unmodded game).");
    s_glint = Config.Bind("Glint", "Enabled", true, "Magnified scopes facing you throw a glint you can spot.");
    s_glintStrength = Config.Bind("Glint", "Strength", 1.0f, "Glint brightness x this.");
    s_glintSun = Config.Bind("Glint", "NeedsSun", true, "Glint only while the sun is up.");
    s_station = Config.Bind("Station", "ShowStats", true, "The gun station lists what each attachment does under its name.");
    s_log = Config.Bind("Debug", "Log", false, "Log each gun's numbers whenever its attachments change.");
    LoadTables();

    GUClass* receiver = FindClass("GunReceiver");
    GFProperty* first = receiver ? API->FindProperty((GUStruct*)receiver, "RotationPerShot") : nullptr;
    GFProperty* last = receiver ? API->FindProperty((GUStruct*)receiver, "RecoveryFactors_Mounted") : nullptr;
    GML_PropInfo fi{}, li{};
    if (first && last && API->GetPropertyInfo(first, &fi) && API->GetPropertyInfo(last, &li) &&
        li.offset + li.size - fi.offset == (int)sizeof(Recoil))
        s_recoilOffset = fi.offset;
    else Warn("the gun's recoil numbers aren't laid out as expected (game updated?) - recoil is left alone");
    GML_PropInfo vi{};
    if (GFProperty* vp = API->FindProperty((GUStruct*)FindClass("EBBullet"), "Velocity"); vp && API->GetPropertyInfo(vp, &vi))
        s_velocityOffset = vi.offset;
    InstallVelocity();

    On(GML_EVENT_WORLD_BEGIN_PLAY, [](void*) {
        for (auto& [p, g] : s_guns) for (Glint& gl : g.glints) gl.comp = nullptr;  // gone with the old level
        s_guns.clear();
        s_rounds.clear();
        s_sun = nullptr;
    });
    On(GML_EVENT_TICK, [](void*) {
        static ULONGLONG lastScan = 0, lastFind = 0, lastLabel = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastScan >= 250) {  // known guns four times a second, new ones once a second
            lastScan = now;
            bool discover = now - lastFind >= 1000;
            if (discover) lastFind = now;
            Scan(discover);
        }
        if (now - lastLabel >= 2000) { lastLabel = now; LabelStation(); }
        DrawGlints();
    });
    Message("Gunsmith {} - {} attachments and guns in the tables (defaults.ini / overrides.ini)", GUNSMITH_VERSION, s_table.size());
    return 0;
}
