// GunsmithDev - in-game test harness for the Gunsmith plugin. Not shipped.
// Loads TrainingGrounds, takes the player's loadout rifle and, with Gunsmith installed alongside:
//   - logs the rifle's numbers bare ("REPORT"), fires shots to measure the rounds' speed and spread ("SHOTS");
//   - mounts a suppressor, a big scope, a laser and a grip the way a hand does (AttachToServer on the gun's own
//     tagged attach points), and logs and measures again;
//   - logs the gun station labels ("STATION");
//   - films the scope from in front for its glint ("SNAP <name>" = a game-rendered screenshot from our own camera);
//   - takes the attachments off again and checks the numbers come back.
// A run never saves anything (the runner snapshots and restores the saves and configs anyway).
#define NOMINMAX
#include <GML/GML.hpp>
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace gml;

GML_PLUGIN("com.remarrow.gunsmithdev", "GunsmithDev", "0.2.0");

struct Vec { double x, y, z; };
struct Rot { double pitch, yaw, roll; };
static constexpr double kD2R = 3.14159265358979 / 180;

static const char* kMap = "/Game/Maps/Training/TrainingGrounds";

static std::string TextOf(Object o, const char* prop) {
    char b[512] = "";
    if (void* t = o.PropPtr(prop)) API->TextToString(t, b, sizeof b);
    return b;
}
static std::string Cls(Object o) { return o ? Object((GUObject*)o.Class()).Name() : "-"; }
static Vec WorldOf(Object o) {
    Params p(o, o.IsA("Actor") ? "K2_GetActorLocation" : "K2_GetComponentLocation");
    p.Call();
    return p.Return<Vec>();
}
static GML_PropInfo TransformInfo() {
    static GML_PropInfo ti{};
    if (!ti.size)
        API->GetPropertyInfo(API->FindProperty((GUStruct*)API->FindFunction(Lib("KismetMathLibrary").Struct(), "MakeTransform"), "ReturnValue"), &ti);
    return ti;
}
static Vec ToWorld(Object actor, Vec local) {
    Params gt(actor, "GetTransform"); gt.Call();
    Params tw(Lib("KismetMathLibrary"), "TransformLocation");
    std::memcpy(tw.Ptr("T"), gt.Ptr("ReturnValue"), TransformInfo().size);
    tw.Set("Location", local).Call();
    return tw.Return<Vec>();
}
static Object PC() {
    Params p(Lib("GameplayStatics"), "GetPlayerController");
    p.SetObj("WorldContextObject", Object(API->WorldContext())).Set("PlayerIndex", (int32_t)0).Call();
    return p.Return<GUObject*>();
}
static Object Pawn() {
    Params p(Lib("GameplayStatics"), "GetPlayerPawn");
    p.SetObj("WorldContextObject", Object(API->WorldContext())).Set("PlayerIndex", (int32_t)0).Call();
    return p.Return<GUObject*>();
}
static Object SpawnAt(GUClass* c, Vec where) {
    Params mt(Lib("KismetMathLibrary"), "MakeTransform");
    mt.Set("Location", where).Set("Scale", Vec{1, 1, 1}).Call();
    Params b(Lib("GameplayStatics"), "BeginDeferredActorSpawnFromClass");
    b.SetObj("WorldContextObject", Object(API->WorldContext())).Set("ActorClass", c);
    std::memcpy(b.Ptr("SpawnTransform"), mt.Ptr("ReturnValue"), TransformInfo().size);
    b.Set("CollisionHandlingOverride", (uint8_t)1).Call();
    Object a = b.Return<GUObject*>();
    if (!a) return {};
    Params f(Lib("GameplayStatics"), "FinishSpawningActor");
    f.SetObj("Actor", a);
    std::memcpy(f.Ptr("SpawnTransform"), mt.Ptr("ReturnValue"), TransformInfo().size);
    f.Call();
    return a;
}

// ------------------------------------------------------------------ the gun and our camera

static Object s_gun, s_cam;
static Vec s_gunAt{};
static double s_gunYaw = 0;

static void PlaceGun() {
    Params mv(s_gun, "K2_SetActorLocationAndRotation");
    mv.Set("NewLocation", s_gunAt).Set("NewRotation", Rot{0, s_gunYaw, 0}).Set("bSweep", false).Set("bTeleport", true).Call();
}
// Our camera at `from`, looking at `at`.
static void ViewFrom(Vec from, Vec at, float fov = 40) {
    if (!s_cam) {
        s_cam = SpawnAt(FindClass("CameraActor"), from);
        if (!s_cam) return;
    }
    if (Object cc = s_cam.GetObj("CameraComponent")) cc.Set("FieldOfView", fov);
    double dx = at.x - from.x, dy = at.y - from.y, dz = at.z - from.z;
    Rot look{std::atan2(dz, std::sqrt(dx * dx + dy * dy)) / kD2R, std::atan2(dy, dx) / kD2R, 0};
    Params mv(s_cam, "K2_SetActorLocationAndRotation");
    mv.Set("NewLocation", from).Set("NewRotation", look).Set("bSweep", false).Set("bTeleport", true).Call();
    Params vt(PC(), "SetViewTargetWithBlend");
    vt.SetObj("NewViewTarget", s_cam).Set("BlendTime", 0.0f).Call();
}
// The camera `dist` cm from the gun's muzzle end along a bearing `deg` off the bore (0 = straight down the barrel).
static void ViewMuzzle(double dist, double deg, double up = 0, float fov = 40) {
    double t = (s_gunYaw + deg) * kD2R;
    Vec scope = ToWorld(s_gun, {15, 0, 9});
    ViewFrom({scope.x + std::cos(t) * dist, scope.y + std::sin(t) * dist, scope.z + up}, scope, fov);
}
static void ViewSide() {
    double t = (s_gunYaw - 90) * kD2R;
    Vec mid = ToWorld(s_gun, {15, 0, 3});
    ViewFrom({mid.x + std::cos(t) * 140, mid.y + std::sin(t) * 140, mid.z + 15}, mid, 45);
}
static void Snap(const char* n) {
    Log("SNAP {}", n);
    API->ExecConsoleCommand("shot");
}

// ------------------------------------------------------------------ measuring

static int (*s_describe)(GUObject*, char*, int);
static void (*s_check)(GUObject*);
static void Report(const char* what) {
    if (s_check) s_check(s_gun.ptr);
    char b[1024] = "";
    if (s_describe) s_describe(s_gun.ptr, b, sizeof b);
    Vec rps = s_gun.Get<Vec>("RotationPerShot"), lps = s_gun.Get<Vec>("LocationPerShot");
    Object lerp = s_gun.GetObj("GSLerp");
    Log("REPORT [{}] {}", what, b);
    Log("  raw: rot/shot ({:.3f} {:.3f} {:.3f}) loc/shot x {:.3f} recov v {:.2f} | spread {:.6f} | loud {:.3f} suppMult {:.3f} | lerp lin {:.2f} ang {:.2f} min2h {:.3f} | attached {}",
        rps.x, rps.y, rps.z, lps.x, s_gun.Get<float>("VerticalRotRecoverySpeed"), s_gun.Get<float>("WeaponSpread"),
        s_gun.Get<float>("WeaponFireNoiseLoudness"), s_gun.Get<float>("SuppressedWeaponFireNoiseMultiplier"),
        lerp ? lerp.Get<float>("LinearLerpSpeed") : -1.f, lerp ? lerp.Get<float>("AngularLerpSpeed") : -1.f,
        lerp ? lerp.Get<float>("EuroSmoothingMinCutoff_TwoHanded") : -1.f, ((GML_TArray*)s_gun.PropPtr("RegisteredAttachments"))->Num);
}

// Every live round's velocity, to tell which one a shot spawned or reused.
struct RoundSeen { GUObject* r; Vec v; };
static std::vector<RoundSeen> Rounds() {
    std::vector<RoundSeen> out;
    for (Object r : FindAllOf("EBBullet")) out.push_back({r.ptr, r.Get<Vec>("Velocity")});
    return out;
}
static void Shots(const char* what, int n) {
    double sum = 0, maxAng = 0, sumAng = 0;
    int got = 0;
    Params fw(s_gun, "GetActorForwardVector"); fw.Call();
    Vec f = fw.Return<Vec>();
    for (int i = 0; i < n; i++) {
        auto before = Rounds();
        Params p(s_gun, "FireRoundLocal");
        p.Call();
        Vec muzzle = ToWorld(s_gun, {60, 0, 2});
        for (auto& now : Rounds()) {
            bool same = false;
            for (auto& b : before) same |= b.r == now.r && b.v.x == now.v.x && b.v.y == now.v.y && b.v.z == now.v.z;
            if (same) continue;
            Vec v = now.v, at = WorldOf(Object(now.r));
            double s = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            double away = std::sqrt((at.x - muzzle.x) * (at.x - muzzle.x) + (at.y - muzzle.y) * (at.y - muzzle.y) + (at.z - muzzle.z) * (at.z - muzzle.z));
            if (s < 1 || away > 2000) continue;  // only this shot's round, still near the muzzle (not an old one ricocheting)
            double ang = std::acos(std::min(1.0, (v.x * f.x + v.y * f.y + v.z * f.z) / s)) / kD2R;
            sum += s; sumAng += ang; maxAng = std::max(maxAng, ang); got++;
            break;
        }
    }
    Log("SHOTS {}: {} of {} rounds seen, mean speed {:.0f} cm/s, angle off the bore mean {:.4f} max {:.4f} deg ({:.2f} MOA group)", what, got, n,
        got ? sum / got : 0, got ? sumAng / got : 0, maxAng, maxAng * 2 * 60);
}

// ------------------------------------------------------------------ mounting, as a hand does

static std::vector<Object> s_mounted;
static Object s_can;

static std::vector<std::string> TagsOf(Object c) {
    std::vector<std::string> out;
    auto* t = (GML_TArray*)c.PropPtr("ComponentTags");
    for (int i = 0; t && i < t->Num; i++) {
        char n[128] = "";
        API->NameToString(((uint64_t*)t->Data)[i], n, sizeof n);
        out.push_back(n);
    }
    return out;
}
static std::vector<Object> Places() {
    static std::vector<Object> comps;  // engine-returned arrays leak in the param block: once
    if (!comps.empty()) return comps;
    Params gc(s_gun, "K2_GetComponentsByClass");
    *(GUClass**)gc.Ptr("ComponentClass") = FindClass("SceneComponent");
    gc.Call();
    if (auto* arr = (GML_TArray*)gc.Ptr("ReturnValue"))
        for (int i = 0; i < arr->Num; i++) comps.push_back(Object(((GUObject**)arr->Data)[i]));
    return comps;
}
// Spawns an attachment and attaches it to the gun at the named place (a rail spline `along` its length, or a scene
// component such as the muzzle's).
static Object Mount(const char* asset, const char* placeName, float along) {
    Object data = LoadObject(std::format("/Game/Weapons/Data/Attachments/{}.{}", asset, asset).c_str());
    GUClass* c = data ? data.Get<GUClass*>("AttachmentClass") : nullptr;
    if (!c) { Log("MOUNT {}: no class", asset); return {}; }
    Object a = SpawnAt(c, ToWorld(s_gun, {0, 0, 20}));
    if (!a) { Log("MOUNT {}: spawn failed", asset); return {}; }
    if (Object root = a.GetObj("StaticMeshComponent")) { Params sp(root, "SetSimulatePhysics"); sp.Set("bSimulate", false).Call(); }
    Params sr(a, "SetAttachableReceiver"); sr.SetObj("NewReceiver", s_gun).Call();
    char tag[128] = "";
    API->NameToString(a.Get<uint64_t>("AttachPointTag"), tag, sizeof tag);
    Object place;
    for (Object p : Places())
        if (p.Name() == placeName) place = p;
    auto tags = place ? TagsOf(place) : std::vector<std::string>{};
    if (!place || std::find(tags.begin(), tags.end(), std::string(tag)) == tags.end()) {
        Log("MOUNT {}: no place '{}' tagged '{}'", asset, placeName, tag);
        return {};
    }
    std::vector<uint8_t> xf(0x60);
    if (place.IsA("SplineComponent")) {
        Params len(place, "GetSplineLength"); len.Call();
        Params at(place, "GetTransformAtDistanceAlongSpline");
        at.Set("Distance", len.Return<float>() * along).Set("CoordinateSpace", (uint8_t)1).Set("bUseScale", false).Call();
        std::memcpy(xf.data(), at.Ptr("ReturnValue"), 0x60);
    } else {
        Params wt(place, "K2_GetComponentToWorld"); wt.Call();
        std::memcpy(xf.data(), wt.Ptr("ReturnValue"), 0x60);
    }
    Params att(a, "AttachToServer");
    att.SetObj("Comp", place);
    std::memcpy(att.Ptr("WorldTransform"), xf.data(), 0x60);
    att.Call();
    Log("MOUNT {} [{}] on {} ({:.2f}): attached={} registered {}", asset, Cls(a), placeName, along, a.GetBool("bAttached"),
        ((GML_TArray*)s_gun.PropPtr("RegisteredAttachments"))->Num);
    if (a.GetBool("bAttached")) s_mounted.push_back(a);
    return a;
}
static void Unmount(Object a) {
    Params gt(a, "GetTransform"); gt.Call();
    Params d(a, "DetachFromServer");
    std::memcpy(d.Ptr("WorldTransform"), gt.Ptr("ReturnValue"), 0x60);
    d.Call();
    if (Object root = a.GetObj("StaticMeshComponent")) { Params sp(root, "SetSimulatePhysics"); sp.Set("bSimulate", false).Call(); }
    Log("UNMOUNT {}: attached={} registered {}", Cls(a), a.GetBool("bAttached"), ((GML_TArray*)s_gun.PropPtr("RegisteredAttachments"))->Num);
}
static void Station(const char* asset) {
    Object a = LoadObject(std::format("/Game/Weapons/Data/Attachments/{}.{}", asset, asset).c_str());
    Log("STATION {}: '{}' / '{}'", asset, a ? TextOf(a, "DisplayName") : "-", a ? TextOf(a, "Subtitle") : "-");
}
static void SetCfg(const char* section, const char* key, const char* value) {
    for (auto& s : Settings("com.remarrow.gunsmith"))
        if (s.section == section && s.key == key) { s.Set(value); Log("cfg {}.{} = {}", section, key, value); }
}
static void LogPlaces() {
    for (Object c : Places()) {
        auto tags = TagsOf(c);
        if (tags.empty()) continue;
        std::string ts;
        for (auto& t : tags) ts += t + " ";
        Log("  place {} [{}] tags [{}]", c.Name(), Cls(c), ts);
    }
}

// ------------------------------------------------------------------ held in a hand

static bool s_held = false;
static Object Hand() {
    Params vr(Pawn(), "GetVRComponents");
    vr.Call();
    return vr.Get<GUObject*>("RightController");
}
static void Grip(bool on) {
    Object hand = Hand();
    if (!hand) { Error("no right controller"); return; }
    if (on) {
        Params gt(s_gun, "GetTransform"); gt.Call();
        Params g(hand, "GripObjectByInterface");
        g.SetObj("ObjectToGrip", s_gun);
        std::memcpy(g.Ptr("WorldOffset"), gt.Ptr("ReturnValue"), TransformInfo().size);
        g.Set("bWorldOffsetIsRelative", false).Set("bIsSlotGrip", false).Call();
        s_held = g.Return<bool>();
        Log("GRIP gripped={}", s_held);
    } else {
        Params d(hand, "DropObjectByInterface");
        d.SetObj("ObjectToDrop", s_gun).Set("GripIDToDrop", (uint8_t)0).Call();
        s_held = false;
        if (Object root = s_gun.GetObj("StaticMeshComponent")) { Params sp(root, "SetSimulatePhysics"); sp.Set("bSimulate", false).Call(); }
        Log("GRIP dropped={}", d.Return<bool>());
    }
}
// Where the grip script keeps its copy of the receiver's recoil numbers, and whether it matches the receiver's.
static void Copy(const char* what) {
    Object gs = s_gun.GetObj("GSGun");
    if (!gs) { Log("COPY [{}]: no GSGun", what); return; }
    auto* live = (uint8_t*)s_gun.PropPtr("RotationPerShot");
    int span = API->StructSize(gs.Struct()), found = -1;
    for (int off = 0x28; off + 0x60 <= span && found < 0; off += 8)
        if (!std::memcmp((uint8_t*)gs.ptr + off, live, 0x60)) found = off;
    auto* c = (double*)((uint8_t*)gs.ptr + 0x678);
    Log("COPY [{}]: receiver rot/shot ({:.3f} {:.3f} {:.3f}) | GSGun+0x678 ({:.3f} {:.3f} {:.3f}) | receiver's numbers found in GSGun at {}", what,
        ((double*)live)[0], ((double*)live)[1], ((double*)live)[2], c[0], c[1], c[2], found >= 0 ? std::format("+0x{:X}", found) : std::string("none"));
}

// ------------------------------------------------------------------ the run

struct Stage { const char* name; double wait; void (*act)(); };
static const Stage kStages[] = {
    {"bare", 1.0, [] { SetCfg("Debug", "Log", "true"); LogPlaces(); Report("bare"); Shots("bare", 20); ViewSide(); }},
    {"@bare", 0.5, [] {}},
    {"mount", 1.0, [] {
        s_can = Mount("Attach_Muzzles_Supp_Socom338", "Fire Location", 0);
        Mount("Attach_Optic_Scope_Schmidt_Bender_PM2", "ReceiverAttachmentSpline", 0.35f);
        Mount("Attach_LaserLight_PEQ15", "ReceiverAttachmentSplineHG3", 0.6f);  // a side rail: on top it hides the objective
        Mount("Attach_FG_Pic_BCM_Gunfighter", "ReceiverAttachmentSplineHG6", 0.4f);
    }},
    {"kitted", 1.0, [] { Report("kitted"); Shots("kitted", 20); ViewSide(); }},
    {"@kitted", 0.5, [] {}},
    {"station", 0.5, [] {
        for (const char* a : {"Attach_Muzzles_Supp_Socom338", "Attach_Muzzles_Brake_SFMB338", "Attach_FG_Pic_BCM_Gunfighter",
                              "Attach_Optic_Scope_Schmidt_Bender_PM2", "Attach_LaserLight_PEQ15", "Attach_Optic_Dot_T2_Short"})
            Station(a);
    }},
    {"glint, sky only, 4 m", 1.0, [] { SetCfg("Glint", "NeedsSun", "false"); ViewMuzzle(400, 0, 0, 30); }},
    {"@glint_4m", 0.5, [] {}},
    {"glint 25 m", 1.0, [] { ViewMuzzle(2500, 0, 0, 20); }},
    {"@glint_25m", 0.5, [] {}},
    {"glint 25 m, 30 degrees off", 1.0, [] { ViewMuzzle(2500, 30, 0, 20); }},
    {"@glint_25m_offaxis", 0.5, [] {}},
    {"glint behind the scope", 1.0, [] { ViewMuzzle(150, 180, 20, 40); }},
    {"@glint_behind", 0.5, [] {}},
    {"unmount can", 1.0, [] { if (s_can) Unmount(s_can); }},
    {"no can", 1.0, [] { Report("scope+peq+grip"); Shots("no can", 20); }},
    {"unmount all", 1.0, [] { for (Object a : s_mounted) if (a.GetBool("bAttached")) Unmount(a); }},
    {"bare again", 1.0, [] { Report("bare again"); Shots("bare again", 20); }},
    {"grip", 1.0, [] { Copy("before grip"); Grip(true); }},
    {"held", 1.0, [] { Copy("held, bare"); s_can = Mount("Attach_Muzzles_Supp_Socom338", "Fire Location", 0); }},
    {"held with can", 1.0, [] { Report("held with can"); Copy("held, can mounted"); if (s_can) Unmount(s_can); }},
    {"held, can off", 1.0, [] { Report("held, can off"); Copy("held, can off"); Grip(false); }},
};

static int s_step = -1;
static double s_wait = 0;

static void Step(double dt) {
    if ((s_wait -= dt) > 0) return;
    if (s_step == 0) {
        Object gear = FindFirstOf("BP_GearManagerComponent_C");
        s_gun = gear ? gear.GetObj("PrimaryGun") : Object();
        if (!s_gun) { Error("no gun"); Log("DEV TEST COMPLETE"); s_step = 99; return; }
        Params det(s_gun, "K2_DetachFromActor");
        det.Set("LocationRule", (uint8_t)1).Set("RotationRule", (uint8_t)1).Set("ScaleRule", (uint8_t)1).Call();
        if (Object root = s_gun.GetObj("StaticMeshComponent")) { Params sp(root, "SetSimulatePhysics"); sp.Set("bSimulate", false).Call(); }
        // The gun hangs 15 m up in open sky in front of the player, pointing the way the player faces (rounds fly clear).
        Object pawn = Pawn();
        Params pr(pawn, "K2_GetActorRotation"); pr.Call();
        s_gunYaw = pr.Return<Rot>().yaw;
        Vec me = WorldOf(pawn);
        s_gunAt = {me.x + std::cos(s_gunYaw * kD2R) * 200, me.y + std::sin(s_gunYaw * kD2R) * 200, me.z + 1500};  // in open sky
        PlaceGun();
        Log("gun {} [{}] at ({:.0f} {:.0f} {:.0f}) yaw {:.0f}", s_gun.Name(), Cls(s_gun), s_gunAt.x, s_gunAt.y, s_gunAt.z, s_gunYaw);
        if (HMODULE m = GetModuleHandleW(L"Gunsmith.dll")) {
            s_describe = (int (*)(GUObject*, char*, int))GetProcAddress(m, "Gunsmith_Describe");
            s_check = (void (*)(GUObject*))GetProcAddress(m, "Gunsmith_Check");
        }
        Log("Gunsmith API: {}", s_describe && s_check ? "found" : "MISSING");
        s_step = 1;
        s_wait = 1.0;
        return;
    }
    int i = s_step - 1, n = (int)(sizeof kStages / sizeof kStages[0]);
    if (i >= n) { Log("DEV TEST COMPLETE"); s_step = 99; return; }
    Log("> {}", kStages[i].name);
    if (kStages[i].name[0] == '@') Snap(kStages[i].name + 1);
    else kStages[i].act();
    s_wait = kStages[i].wait;
    s_step++;
}

GML_AWAKE() {
    On(GML_EVENT_WORLD_BEGIN_PLAY, [](void* gm) {
        std::string level = Object((GUObject*)gm).Path();
        if (level.find("/Game/Maps/MainMenu/") == 0) {
            Params p(Lib("GameplayStatics"), "OpenLevel");
            p.SetObj("WorldContextObject", Object(API->WorldContext())).Set("LevelName", API->MakeName(kMap)).Set("bAbsolute", true).Call();
        } else if (level.find(kMap) == 0) {
            s_step = 0;
            s_wait = 6;
        }
    });
    On(GML_EVENT_TICK, [](void*) {
        static LARGE_INTEGER freq{}, last{};
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (!freq.QuadPart) { QueryPerformanceFrequency(&freq); last = now; }
        double dt = double(now.QuadPart - last.QuadPart) / double(freq.QuadPart);
        last = now;
        if (s_step >= 0 && s_step < 99) Step(dt);
        if (s_step >= 1 && s_step < 99 && s_gun && !s_held) PlaceGun();  // held still in mid-air (unless a hand has it)
    });
    return 0;
}
