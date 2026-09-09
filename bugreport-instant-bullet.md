# BI Bugtracker — Server-side AI projectile flight time applies only to the first shot

> **Filed:** https://report.bistudio.com/issues/DZG-700

## Issue Title

`Weapon_Base.Fire(mi, pos, dir, speed)` native: on a **server-side AI** (`INSTANCETYPE_AI_SERVER`) the ballistic projectile flies with real flight time only on the **first** shot; the **2nd** shot's impact callback never fires, and **3rd+** shots impact **instantly (0–1 ms)** while still landing at the correct ~420 m endpoint.

## What happened

A server-side AI bot (no client, `INSTANCETYPE_AI_SERVER`) fires a bolt-action `Mosin9130` (7.62x54R) at a ground target ~420 m away, single shots ~2.5 s apart, reloaded between shots via `WeaponManager.EjectBullet()` (bolt cycle) / `LoadMultiBullet` (chamber-load from loose ammo in cargo).

We log `GetGame().GetTime()` immediately before `Fire()` and inside an override of `DayZGame.FirearmEffects(...)`. Measured deltas:

| shot | `Fire()` call | `FirearmEffects` (impact) | Δ |
|---|---|---|---|
| 1 | 93282 | 93944 | **+662 ms — real flight** |
| 2 | 95782 | *(never fired)* | impact callback missing |
| 3 | 98282 | 98283 | +1 ms |
| 4+ | … | … | 0–1 ms (instant) |

Critical detail: even the "instant" impacts report the **correct** final position (the ground ~423 m from the muzzle: muzzle `z≈10831` → impact `z≈10461`), with a correct `inSpeed` decayed by air friction. So the **hit position and impact speed are right** — only the **flight-time simulation / scheduling is skipped** after the first shot, and the 2nd shot's impact is swallowed entirely.

Reproduced across multiple runs and bots; always the same pattern: 1st real, 2nd missing, 3rd+ instant.

## Reproduction steps

1. Spawn a server-side AI unit (`INSTANCETYPE_AI_SERVER`, no client / no `GetCameraPoint` input) holding a bolt-action `Mosin9130` + `Ammo_762x54` (chambered + spare loose rounds in cargo).
2. Aim at a stationary ground target ~400 m away; keep the weapon raised and ready.
3. Fire a single shot through the low-level native with an explicit muzzle origin and aim direction (see code below) — **not** `TryFireWeapon`, because an AI never drives the internal `GetCameraPoint` aim:
   ```c
   // origin = muzzle position, direction = normalized aim dir
   vector pos = origin + direction * 0.2;
   bool fired = Fire(muzzleIndex, pos, direction, direction); // speed arg = unit dir; magnitude from CfgAmmo initSpeed
   ```
4. Wrap `DayZGame.FirearmEffects(...)` in an override and log `GetGame().GetTime()` at both the `Fire()` call site and inside `FirearmEffects`.
5. Between shots, cycle the bolt (`EjectBullet`) / chamber-load (`LoadMultiBullet`) so the chamber is live again, wait ~2.5 s, fire again.
6. Repeat 3–5 several times and compare `FirearmEffects` arrival times.

Observed: shot 1 arrives ~660 ms later; shot 2 produces no `FirearmEffects` at all; shots 3+ arrive 0–1 ms after `Fire()`.

## Expected result

Every shot should spawn a real ballistic projectile that flies for the physical travel time (at 400 m with `initSpeed ≈ 830` and `airFriction ≈ -0.0009..-0.001`, roughly **550–700 ms**), with `FirearmEffects` firing once per shot at the actual arrival moment and `inSpeed` reflecting the air-friction-decayed velocity. The 2nd shot must not be swallowed, and 3rd+ shots must not hit instantly.

## Environment

- DayZ (Enfusion), current stable/experimental.
- **Server-side AI** (`DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER`) — the key difference from a human player: no client fires the weapon, so the whole fire path runs on the server through the `Fire()` native.
- Weapon `Mosin9130` (bolt, internal 5-rd mag); ammo `Ammo_762x54` → projectile `Bullet_762x54`.
- Single-shot cadence, ~2.5 s between shots.

## Code (minimal)

Fire path (server only):

```c
// reg/4_World/modded_WeaponBase.c
bool dmBot_Fire(int muzzleIndex)
{
    vector origin, direction, velocity;
    pawn.ComputeShot(this, muzzleIndex, origin, direction, velocity); // muzzle pos + aim dir
    vector pos = origin + direction * 0.2;
    // timing marker:
    dmBotLog.Debug("[Ballistics] FIRE time=" + GetGame().GetTime());
    return Fire(muzzleIndex, pos, direction, velocity); // velocity == unit direction
}
```

Impact marker:

```c
// core/3_Game/modded/modded_DayZGame.c
modded class DayZGame
{
    override void FirearmEffects(Object source, Object directHit, int componentIndex,
        string surface, vector pos, vector surfNormal, vector exitPos,
        vector inSpeed, vector outSpeed, bool isWater, bool deflected, string ammoType)
    {
        super.FirearmEffects(...);
        dmBotLog.Debug("[Ballistics] IMPACT time=" + GetGame().GetTime()
            + " pos=" + pos + " inSpeed=" + inSpeed);
    }
}
```

## Log evidence (abridged)

```
08:30:16 [Ballistics] FIRE time=93282  origin=<4130.03,340.46,10831.08> dir=<0.486,-0.000,-0.873>
08:30:16 [Ballistics] IMPACT time=93944 pos=<4335.62,338.28,10461.24> inSpeed=<254,-5,-458>   ← +662 ms
08:30:18 [Ballistics] FIRE time=95782  origin=<4130.03,340.46,10831.08>                       ← no IMPACT follows
08:30:21 [Ballistics] FIRE time=98282  origin=<4130.03,340.46,10831.08>
08:30:21 [Ballistics] IMPACT time=98283 pos=<4335.04,338.27,10463.39> inSpeed=<255,-5,-459>   ← +1 ms
```

## Related / suspected root cause

- A related vanilla timing bug is referenced in the DayZ-Expansion source as **T186177** ("firing over a longer distance … to ensure the projectile **is in flight for a certain amount of time**"); Expansion AI does **not** trust the `Fire()` native's timing/entity — it does its own hitscan, records the shot, and defers `ProcessDirectDamage` via `CallLater` to re-create the travel time in script.
- Separate (same area, different symptom): `Fire()` does **not** mark the chamber `IsChamberFiredOut` (chamber stays "loaded" after `Fire()`), so the weapon FSM re-syncs to a wrong state — reported separately.
