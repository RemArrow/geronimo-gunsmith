// Gunsmith's built-in tables, in the same INI form a player's overrides.ini uses. Written out at every launch as
// GML\plugins\Gunsmith\defaults.ini (for reference); overrides.ini, read after it, changes any value.
#pragma once

static const char kDefaultsIni[] = R"INI(; Gunsmith - built-in tables. This file is rewritten at every launch: copy a section into overrides.ini
; (same folder) and change it there. A section in overrides.ini replaces only the keys it sets.
;
; ATTACHMENTS: one section per attachment class (the game's blueprint class name, ending in _C).
;   Weight      grams, mount included. Weight is what makes a gun handle (and kick) differently:
;               every gram is felt, more the further it sits from the pistol grip.
;   Vertical    % change to muzzle rise per shot              (negative = less recoil)
;   Horizontal  % change to sideways (yaw) recoil per shot
;   Roll        % change to the roll (cant) each shot gives
;   Kick        % change to the straight-back push into the shoulder
;   Recovery    % change to how fast the gun settles back after a shot
;   Velocity    % change to muzzle velocity
;   Precision   % change to the gun's group size (negative = tighter groups)
;   Sound       dB the device adds to the shot as the AI hears it (suppressors negative, brakes positive)
;   Glint       0..1: how strongly the front lens throws a sun glint others can see
;   Note        a word or two shown in the gun station
;
; GUNS: [Gun <part of the class name>] - first match wins, so specific names come first.
;   Weight      kg, unloaded, no attachments. Moment of inertia is estimated from it and the gun's length.
;   MOA         typical group size with issue ammunition (minutes of angle). 0 = leave the game's (shotguns).
;
; DEFAULTS: [Default <kind>] values for an attachment that has no section of its own (another mod's, or a
; future update's). Kinds: Suppressor, Muzzle, RedDot, Magnified, Magnifier, Underbarrel, Bipod, LaserLight, Rail.

; ------------------------------------------------------------------ muzzle devices
; Suppressors trap the gas that otherwise kicks the muzzle up and back. A can's weight hanging off the muzzle does
; much of what is felt (15-25% less recoil in all); that is counted from Weight, so the numbers here are the gas alone.
; Sound: published reductions (dB) where the maker gives one, else from same-size cans on the same calibre.
; Velocity: per this mod's design a can costs a little speed (real cans often ADD 1-2% - set Velocity = 1).

[Muzzle_Suppressor_AAC_SDN_C]
Note = 9mm can
Weight = 340
Vertical = -8
Horizontal = -5
Roll = -5
Kick = -10
Sound = -30
Velocity = -2

[Muzzle_GM9_C]
Note = 9mm can
Weight = 215
Vertical = -8
Horizontal = -5
Roll = -5
Kick = -10
Sound = -28
Velocity = -2

[Muzzle_Suppressor_HXFlow556K_C]
Note = flow-through can
Weight = 397
Vertical = -10
Horizontal = -6
Roll = -6
Kick = -12
Sound = -18
Velocity = -1

[Muzzle_Osprey45K_C]
Note = .45 can
Weight = 270
Vertical = -8
Horizontal = -5
Roll = -5
Kick = -10
Sound = -26
Velocity = -2

[Muzzle_Suppressor_SIG_SLX556C_QD_C]
Note = 5.56 can
Weight = 450
Vertical = -12
Horizontal = -6
Roll = -6
Kick = -15
Sound = -22
Velocity = -2

[Muzzle_Socom338_C]
Note = .338 can
Weight = 600
Vertical = -14
Horizontal = -7
Roll = -7
Kick = -18
Sound = -24
Velocity = -3

[Muzzle_Socom338_FDE_C]
Note = .338 can
Weight = 600
Vertical = -14
Horizontal = -7
Roll = -7
Kick = -18
Sound = -24
Velocity = -3

[Muzzle_Suppressor_Surefire_RC3_C]
Note = 5.56 can
Weight = 527
Vertical = -12
Horizontal = -6
Roll = -6
Kick = -15
Sound = -24
Velocity = -2

[Muzzle_Tirant45S_C]
Note = .45 can
Weight = 300
Vertical = -8
Horizontal = -5
Roll = -5
Kick = -10
Sound = -27
Velocity = -2

; A brake vents gas sideways and back: the least kick of any device, and the loudest shot for everyone else.
[Muzzle_SFMB338_C]
Note = muzzle brake
Weight = 230
Vertical = -15
Horizontal = -10
Kick = -40
Sound = 6

; ------------------------------------------------------------------ optics
; Weight with the mount. Glint: big objective lenses throw the strongest glint.

[ACRO_Short_C]
Weight = 105
[ACRO_Tall_C]
Weight = 125
[AimpointPRO_QRP2_C]
Weight = 330
[AimpointPRO_Short_C]
Weight = 320
[AimpointPRO_Tall_C]
Weight = 340
[CompM4s_C]
Weight = 400
[CompM5_Short_C]
Weight = 215
[CompM5_Tall_C]
Weight = 235
[CompM5_Unity_Blk_C]
Weight = 240
[CompM5_Unity_FDE_C]
Weight = 240
[CompM5B_30mm_C]
Weight = 245
[CompM5B_39mm_C]
Weight = 255
[EXPS3_C]
Note = holographic
Weight = 320
[EXPS3_FDE_C]
Note = holographic
Weight = 320
[Att_Sight_Holosun_507C_C]
Weight = 75
[MRO_Short_C]
Weight = 180
[MRO_Tall_C]
Weight = 200
[Romeo1Pro_C]
Weight = 70
[Romeo4T_Short_C]
Weight = 165
[Romeo4T_Short_FDE_C]
Weight = 165
[Romeo4T_Tall_C]
Weight = 185
[Romeo4T_Tall_FDE_C]
Weight = 185
[SRO_Direct_C]
Weight = 75
[SRO_Short_C]
Weight = 130
[SRO_Short_FDE_C]
Weight = 130
[SRO_Tall_C]
Weight = 150
[SRO_Tall_FDE_C]
Weight = 150
[SRS_C]
Weight = 230
[MicroT2_Canted_C]
Weight = 120
[MicroT2_Short_C]
Weight = 100
[MicroT2_Tall_C]
Weight = 115
[MicroT2_Unity_C]
Weight = 125
[MicroT2_Unity_FDE_C]
Weight = 125
[Att_Sight_Valday_PK122_C]
Weight = 260

[ACOG_4x_C]
Weight = 450
Glint = 0.5
[ACOG_4x_Tan_C]
Weight = 450
Glint = 0.5
[ElcanSpecter_C]
Weight = 650
Glint = 0.6
[ElcanSpecter_Brown_C]
Weight = 650
Glint = 0.6
[SB_PM2_C]
Weight = 1300
Glint = 1.0
[SB_PM2_Tan_C]
Weight = 1300
Glint = 1.0
[Optic_Razor_1_10_C]
Weight = 810
Glint = 0.5
[Optic_RazorHD_5_20_C]
Weight = 1150
Glint = 0.9
[Vudu_C]
Weight = 780
Glint = 0.4

[Aimpoint_Magnifier_Unity_C]
Weight = 340
Glint = 0.2
[Aimpoint_Magnifier_Unity_FDE_C]
Weight = 340
Glint = 0.2
[Scope_Magnifier_Aimpoint_3XP_30mmFlip_C]
Weight = 330
Glint = 0.2
[Scope_Magnifier_Aimpoint_3XP_39mmFlip_C]
Weight = 335
Glint = 0.2
[Scope_Magnifier_EOTech_G33_STSTall_C]
Weight = 400
Glint = 0.2
[Scope_Magnifier_EOTech_G33_STSTall_FDE_C]
Weight = 400
Glint = 0.2

; ------------------------------------------------------------------ grips and handstops
; A vertical grip gives the support hand leverage against muzzle rise; an angled grip or handstop lets you pull
; the gun back into the shoulder (less kick, faster settling) and holds it steadier side to side.

[FG_MLOK_Arisaka_C]
Note = handstop
Weight = 25
Horizontal = -4
Kick = -4
Recovery = 6
[FG_Pic_Arisaka_C]
Note = handstop
Weight = 30
Horizontal = -4
Kick = -4
Recovery = 6
[FG_MLOK_BCM_Gunfighter_C]
Note = vertical grip
Weight = 45
Vertical = -8
Horizontal = -4
Recovery = 4
[FG_Pic_BCM_Gunfighter_C]
Note = vertical grip
Weight = 50
Vertical = -8
Horizontal = -4
Recovery = 4
[FG_MLOK_BCM_Gunfighter_Reverse_C]
Note = angled grip
Weight = 40
Vertical = -4
Horizontal = -6
Kick = -4
Recovery = 8
[FG_Pic_BCM_Gunfighter_Reverse_C]
Note = angled grip
Weight = 45
Vertical = -4
Horizontal = -6
Kick = -4
Recovery = 8
[FG_MLOK_Railscales_RSB_C]
Note = handstop
Weight = 20
Horizontal = -4
Kick = -3
Recovery = 6
[FG_Pic_HK_Vertical_C]
Note = vertical grip
Weight = 120
Vertical = -10
Horizontal = -4
Recovery = 2
[FG_Pic_Viking_UVG_C]
Note = vertical grip
Weight = 70
Vertical = -8
Horizontal = -5
Recovery = 4
[FG_Pic_Zenitco_RK0_C]
Note = vertical grip
Weight = 70
Vertical = -8
Horizontal = -4
Recovery = 3
[FG_Pic_Zenitco_RK1_B25L_C]
Note = vertical grip
Weight = 90
Vertical = -9
Horizontal = -6
Recovery = 4
[FG_Pic_Zenitco_RK4_C]
Note = angled grip
Weight = 60
Vertical = -5
Horizontal = -6
Kick = -3
Recovery = 6
; The game itself steadies a gun resting on its bipod; here it is weight.
[BP_Bipod_C]
Note = bipod
Weight = 400

; ------------------------------------------------------------------ lasers and lights

[ViridianC5L_C]
Weight = 145
[M600V_C]
Weight = 165
[M600V_FDE_C]
Weight = 165
[M600V_Offset_C]
Weight = 190
[M600V_Offset_FDE_C]
Weight = 190
[MawlC1_C]
Weight = 305
[Light_Modlite_OKW_MLOK_OffsetL_C]
Weight = 160
[Light_Modlite_OKW_MLOK_OffsetR_C]
Weight = 160
[NGAL_C]
Weight = 275
[Att_Laser_EOTech_OGL_C]
Weight = 125
[PEQ15_C]
Weight = 216
[PEQ15_FDE_C]
Weight = 216
[Attachment_Light_Insight_WMX200_C]
Weight = 136
[Light_Pistol_Surefire_X300UB_C]
Weight = 128
[Light_Pistol_Surefire_X400_C]
Weight = 145
[Att_Light_Zentico_Klesch2P_C]
Weight = 200
[Att_Laser_Zenitco_Perst4_C]
Weight = 300

; ------------------------------------------------------------------ rails and risers

[Rail_AK_RSRegulate_C]
Weight = 110
[Rail_Adapter_MLOK_3Slot_C]
Weight = 15
[Rail_Adapter_MLOK_5Slot_C]
Weight = 22
[Rail_Adapter_MLOK_7Slot_C]
Weight = 30
[Rail_Adapter_MLOK_9Slot_C]
Weight = 38
[Rail_Adapter_MLOK_11Slot_C]
Weight = 46
[Rail_Adapter_MLOK_Cantilever_C]
Weight = 40
[Rail_Mp5_Claw_C]
Weight = 150
[FTC_Riser_C]
Weight = 55
[GBRS_C]
Weight = 65
[Rail_Riser_Wilcox_C]
Weight = 50

; ------------------------------------------------------------------ anything without a section of its own

[Default Suppressor]
Weight = 450
Vertical = -12
Horizontal = -6
Roll = -6
Kick = -15
Sound = -24
Velocity = -2
[Default Muzzle]
Weight = 150
Vertical = -10
Kick = -15
Sound = 3
[Default RedDot]
Weight = 200
[Default Magnified]
Weight = 700
Glint = 0.6
[Default Magnifier]
Weight = 340
Glint = 0.2
[Default Underbarrel]
Weight = 70
Vertical = -6
Horizontal = -4
Recovery = 3
[Default Bipod]
Weight = 400
[Default LaserLight]
Weight = 180
[Default Rail]
Weight = 40

; ------------------------------------------------------------------ guns

[Gun AK12]
Weight = 3.3
MOA = 3.0
[Gun AK74M]
Weight = 3.4
MOA = 4.0
[Gun G19]
Weight = 0.67
MOA = 4.0
[Gun Staccato]
Weight = 0.95
MOA = 2.5
[Gun G28]
Weight = 5.8
MOA = 1.5
[Gun HK416A5_279]
Weight = 3.2
MOA = 2.0
[Gun HK416A5_368]
Weight = 3.5
MOA = 1.8
[Gun KS1]
Weight = 3.0
MOA = 1.8
[Gun M110]
Weight = 5.2
MOA = 1.0
[Gun SR25]
Weight = 4.9
MOA = 1.0
[Gun BenelliM4]
Weight = 3.8
MOA = 0
[Gun 870]
Weight = 3.4
MOA = 0
[Gun BlockIICQBR]
Weight = 2.7
MOA = 3.0
[Gun M4A1]
Weight = 3.0
MOA = 2.5
[Gun MCX_LT_CSAW]
Weight = 2.7
MOA = 2.0
[Gun MCX_LT_Rattler]
Weight = 2.3
MOA = 2.5
[Gun MP5SD]
Weight = 3.0
MOA = 3.0
[Gun MP5]
Weight = 2.9
MOA = 3.0
[Gun MP7]
Weight = 1.9
MOA = 3.0
[Gun PKP]
Weight = 8.2
MOA = 3.5
[Gun R700_MDTESS]
Weight = 7.5
MOA = 0.75
[Gun Remington700]
Weight = 6.5
MOA = 0.75
[Gun P226]
Weight = 0.96
MOA = 3.5
[Gun P320]
Weight = 0.83
MOA = 3.5
[Gun USP45]
Weight = 0.78
MOA = 3.5
[Gun Tavor]
Weight = 3.3
MOA = 2.5
[Default Rifle]
Weight = 3.2
MOA = 2.5
[Default Pistol]
Weight = 0.85
MOA = 4.0
[Default Shotgun]
Weight = 3.6
MOA = 0
)INI";
