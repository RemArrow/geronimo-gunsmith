# Gunsmith for Geronimo

A [GML](https://github.com/RemArrow/geronimo-mod-loader) plugin that makes weapon customization matter. Every
attachment in the gun station (all 102) changes how the gun handles, the way it would for real. Nothing is
replaced or added to the station: you build guns exactly as before, and they behave differently.

| What | How it works in the game |
|---|---|
| **Weight** | Every attachment has its real weight, mount included (PEQ-15 216 g, S&B PM II 1,300 g, Socom 338-Ti 600 g...). Weight far from the pistol grip counts most: the plugin works out the gun's moment of inertia about the grip from the gun's real weight and length, plus each attachment's weight times its distance squared. |
| **Handling** (VR's "ADS speed vs sway") | The game smooths a held gun with its grip script `GS_Lerp` (follow speeds plus a one-euro filter). Added inertia lowers the turn speed and the filter's minimum cutoff, so a gun with a can and a big scope swings slower and steadier in your hands. Added mass lowers the move speed. A bare carbine stays snappy. |
| **Recoil** | Muzzle devices and grips change muzzle rise, sideways recoil, roll, the push into the shoulder and how fast the gun settles. A heavier gun also kicks less: kick scales with mass, muzzle rise with inertia. A vertical grip mostly fights rise; an angled grip or handstop steadies sideways recoil and settles faster. A brake cuts kick hardest. |
| **Sound** | Each suppressor quiets the shot by its own dB for the AI's hearing (every 25 dB halves loudness, anchored on the game's own suppressor value). A brake makes the shot louder. |
| **Velocity** | Muzzle devices change muzzle velocity (cans cost 1-3%, per this mod's design; see the note in `defaults.ini`). |
| **Precision** | Each gun groups like the real one (M110/SR-25 1 MOA, HK416 2, M4A1 2.5, AK-74M 4...) instead of the unmodded game's dead-straight rounds. Attachments can tighten or open groups (`Precision`). |
| **Scope glint** | Magnified scopes throw a glint toward you when they point at you, brightest when the lens sits between you and the sun's reflection. It works on other players' and the AI's scopes. Your own scope never glints at you, and an attachment mounted in front of the objective (a PEQ on the top rail) hides it. |
| **Gun station** | Each attachment's subtitle in the station lists what it does, e.g. *Suppressor \| .338 can \| 600 g \| recoil rise -14%, sideways -7%, kick -18% \| -24 dB \| velocity -3%*. |

Mounting, removing or swapping a part, at the station or in your hands, takes effect immediately, even while you
hold the gun. Taking everything off restores the gun's numbers exactly. Changes from other plugins (the Barrett
and Saiga set their own recoil) become the new base, so nothing compounds.

## Tuning: `defaults.ini` and `overrides.ini`

`GML\plugins\Gunsmith\defaults.ini` is rewritten at every launch and lists every attachment and gun with all its
values and what each key means. To change one, copy its section into `overrides.ini` (same folder) and edit it
there. A key you leave out keeps its default.

```ini
[PEQ15_C]                ; the attachment's class (defaults.ini lists them all)
Weight = 250             ; grams
[Muzzle_Socom338_C]
Velocity = 1             ; real cans often add 1-2%
Sound = -28              ; dB
[Gun M4A1]               ; part of the gun's class name
MOA = 2
```

Attachment keys: `Weight` (g), `Vertical`, `Horizontal`, `Roll`, `Kick`, `Recovery`, `Velocity`, `Precision`
(all %), `Sound` (dB), `Glint` (0-1), `Note`. Gun keys: `Weight` (kg), `MOA`. Attachments without a section (from
another mod, or a game update) get the `[Default <kind>]` values for their kind.

## Settings: `GML\config\com.remarrow.gunsmith.cfg`

Each effect can be switched off or scaled: `[Recoil] Strength`, `WeightReducesRecoil`; `[Handling] Strength`;
`[Sound]`, `[Velocity]`; `[Precision] Scale` (0 = the unmodded game's straight shots); `[Glint] Strength`,
`NeedsSun`; `[Station] ShowStats`; `[Debug] Log` (logs each gun's numbers whenever its attachments change).

## Multiplayer

No network traffic of its own. Attachments replicate, and every machine running Gunsmith works the same numbers
out from them. Recoil and handling apply on the holder's machine. Velocity and precision apply on the host,
which fires every round. Glint is drawn by each machine for its own camera. Everyone should run the same
`overrides.ini`.

## How it is built

- `AGunReceiver`: `RotationPerShot` / `MaxRotation` (X roll, **Y pitch = muzzle rise, always up**, Z yaw either
  way; read from the native `GS_Gun::ApplyRecoil`), `LocationPerShot` / `MaxLocation` (kick), the recovery
  speeds, `WeaponSpread` (radians, widest angle), `WeaponFireNoiseLoudness`,
  `SuppressedWeaponFireNoiseMultiplier`, `RegisteredAttachments`.
- The grip script `GS_Gun` copies the recoil numbers when the gun is gripped (`+0x678` in this build, found by
  value), so a change made while the gun is held is patched into the copy as well.
- `GS_Lerp`: `LinearLerpSpeed`, `AngularLerpSpeed`, `EuroSmoothingMinCutoff_OneHanded/TwoHanded`.
- Rounds are pooled, so their speed comes from the pooled actor, not the class defaults: the plugin hooks
  `AGunReceiver::FireRoundLocal` (native, chained after the Saiga's and Barrett's hooks) and scales the fired
  round's velocity.
- Glint: a camera-facing quad on the objective lens (engine plane, glTF unlit material, generated flare
  texture), sized to a constant angle and brightened by the lens/sun/viewer geometry.

## Install

Needs [GML 2.2.0 or newer](https://github.com/RemArrow/geronimo-mod-loader/releases). The easiest way is
[GML Manager](https://github.com/RemArrow/geronimo-mod-loader#gml-manager): Gunsmith is in its Browse tab. By hand: close
the game, download `Gunsmith-<version>.zip` from [Releases](https://github.com/RemArrow/geronimo-gunsmith/releases) and
extract it into `...GERONIMOGeronimoBinariesWin64`. That adds `GMLplugins$d`. To remove the
plugin, delete that folder.

## Building

`build.bat` builds `build\Gunsmith\Gunsmith.dll` and the test harness. `build.bat install` copies the plugin into
the game (not while it runs). Needs the GML SDK (`external\gml`, `..\ModLoader\include` or `GML_SDK`).

`tests\GunsmithDev` (built, never installed) loads TrainingGrounds. It takes the loadout rifle, measures it bare,
mounts a Socom 338-Ti, S&B PM II, PEQ-15 and BCM grip the way a hand does, and measures again (numbers and fired
rounds' speed and spread). It then films the glint from 4 m and 25 m, on and off axis and from behind, and takes
everything off again. The same is repeated while the gun is gripped.

## Verified (2026-10-14, headless test runs, M110 loadout rifle)

- Kitted (can, S&B, PEQ, grip: +2.17 kg): rise -45%, kick -42%, turn speed -29%, suppressed -24 dB. Rounds 851 → 826
  m/s (-3.0%), 1.0 MOA groups. Everything off: stock numbers back exactly.
- Mounted / removed while gripped: the grip script's copy follows.
- Glint visible at 4 m and 25 m on axis; none 30 degrees off axis or from behind.
- Station subtitles show each attachment's stats.

**Not yet tested:** how it feels in VR with a headset, and two-player multiplayer.
