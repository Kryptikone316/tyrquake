# TyrQuake for the Data Frog SF3500 (TreeFrogUI)

This branch adds what the SF3500 needs on top of `libretro/tyrquake`.

| Change | Why |
| --- | --- |
| `pr_*.c` built at `-O1` (`build_sf3500.sh`) | At `-O2` a QuakeC read of an entity field sometimes returns the value with one high bit cleared on the real 74Kc (QEMU/x86 never do). `-O1` removes it in every test run. |
| `PR_GetString` repair | A negative string reference that lost bit 29 is put back (`0xDFFFFF79` -> `0xFFFFFF79`) and counted. Anything else out of range is still a normal error. |
| Joypad binds after `Host_Init` | picoarch sets the controller before the game starts, when `bind` does not exist yet; keys that are still unbound now get the chosen layout, so mission packs without a `config.cfg` are playable. |
| Audio mixed per real elapsed time | picoarch cannot report the real frame rate; with a fixed 1/60 s per run, 30 fps starved the audio. |
| Hunk sized from `/proc/meminfo` | picoarch has no memory-status call; half of `MemAvailable` (32..96 MB) lets big mods such as Honey load. `TYR_HUNK_MB` overrides. |
| Opt-in log, crash dump | Messages go to a 64 KB in-memory ring; on a Quake error, a repaired string reference or a fatal signal it is written to `/mnt/sdcard/tyrquake_crash.log`. Create `/mnt/sdcard/tyrquake_debug.txt` to stream everything to `/mnt/sdcard/tyrquake.log`. A once-a-second `perf:` line shows runs/s, time in the core, audio and repairs. |
| Test hooks | `TYR_CMD="map e1m1"` runs a console command on the first frame, `TYR_LOG=<file>` streams the log; used by an off-device libretro test host. |

`sf3500/quakeloader/` is the small gamepad loader the port runs first: it lists the game folders that hold a `pak0.pak` (Quake, the mission packs, mods) and saves the choice in `game.txt`. It reuses the display, input and font modules of the other SF3500 ports; `make preview` builds a PC version with scripted input for screenshots.

Content must be a `.pak` (the loader's convention): `id1/pak0.pak`, a mission pack's `pak0.pak`, or a mod
packed into `pak0.pak` (the mod folder name is the game directory).

Licence: GPL-2.0-or-later, as upstream (`LICENSE.txt`).
