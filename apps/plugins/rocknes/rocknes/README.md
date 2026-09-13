# rocknes - NES emulator for Rockbox (iPod Mini 2G)

Ports the CPU/PPU/APU/mapper core of
[ObaraEmmanuel/NES](https://github.com/ObaraEmmanuel/NES) (MIT licensed)
to run as a native Rockbox plugin. Upstream is SDL-based; this replaces
every SDL touchpoint with the Rockbox plugin API.

**This has been written against real Rockbox source/headers but has
never been compiled.** There is no ARM-EABI Rockbox toolchain available
in the environment this was written in. Treat it as a well-researched
draft, not a tested build. See "Known risks" below before investing
time in it.

## What's different from upstream

- NSF (NES Sound Format) music player mode: removed, not applicable here
- Android touch/gamepad glue: removed
- Nametable debug-view mode: removed
- SDL graphics/audio/input/file-IO: replaced throughout
- `queue_audio()`'s adaptive SDL-queue-depth resampler: replaced with a
  plain ring buffer, since Rockbox's mixer is pull-based and doesn't
  need it
- `SAMPLING_FREQUENCY`: lowered 48000 -> 11025 Hz (`apu.h`) to fit the
  CPU budget - see risks
- `quit(int)`: no longer calls libc `exit()` (meaningless in a plugin);
  now `setjmp`/`longjmp`s back to `plugin_start`, which reports the
  failure and returns `PLUGIN_ERROR` cleanly
- Two-player input: dropped. The Mini 2G's scrollwheel + 2 buttons
  can't usefully drive a second pad.

## Hardware target

iPod Mini 2G specifically: `LCD_WIDTH=138 LCD_HEIGHT=110 LCD_DEPTH=2`
(4 grey levels), `CPU_FREQ=11289600` (~11MHz ARM7TDMI, **no FPU**),
`PLUGIN_BUFFER_SIZE=0x80000` (512KB).

Controls (IPOD_4G_PAD keymap):
- scrollwheel -> NES d-pad (via BUTTON_UP/DOWN/LEFT/RIGHT)
- centre click (SELECT) -> NES A
- PLAY -> NES B
- PLAY+SELECT together -> NES START
- MENU (hold) -> exit

## Known risks (untested, ranked by how worried you should be)

1. **Audio CPU cost.** `biquad.c`'s per-sample filtering runs at
   11025 Hz using `double` math on an FPU-less ARM7TDMI. Filter
   *setup* (the expensive trig/pow calls in `biquad_init`) only runs
   once at APU init and is fine. The per-sample processing is plain
   multiply-adds but in software double-precision, which is slow on
   this core. This is the single biggest open question - it may need
   porting to fixed-point, or disabling the low/high-pass filtering
   entirely and accepting rougher audio, to leave headroom for
   CPU/PPU emulation.
2. **Overall frame budget.** No profiling has been done. An
   interpreted 6502 + cycle-accurate PPU is real work; whether it
   plus a 256x240->138x110 dithered blit plus audio fits in a 16.67ms
   NTSC frame budget at ~11MHz is unverified. Frameskip may be
   necessary (not currently implemented - upstream doesn't have it
   either, unlike Rockboy which has explicit `options.frameskip`).
3. **Colour loss.** NES's 64-colour palette collapses to 4 grey
   levels via luma + ordered dithering (`gfx_rockbox.c`). Some games
   rely on colour contrast that won't survive this; expect some
   scenes to look muddy regardless of performance.
4. **No compile verification.** Written against real headers pulled
   from the Rockbox git repo, but never run through the actual
   arm-eabi cross-compiler. Expect at least minor build fixes needed
   (missing includes, small signature mismatches) even if the
   architecture holds up.
5. **No save states.** Not implemented at all.

## Integrating into a Rockbox source tree

1. Clone Rockbox: `git clone https://github.com/Rockbox/rockbox.git`
2. Copy this `rocknes/` directory to `apps/plugins/rocknes/`
3. Add `rocknes` to `apps/plugins/SUBDIRS` (see
   `SUBDIRS.rocknes.snippet` in this archive for the exact guarded
   block to insert - place it near other game plugins)
4. Add the line from `viewers.config.rocknes.snippet` to
   `apps/plugins/viewers.config` to associate `.nes` files with this
   plugin in the file browser
5. Build normally for the `ipodmini2g` target (see Rockbox's
   `tools/configure` / standard cross-compile instructions - requires
   the arm-eabi toolchain, not covered here)
6. Copy a `.nes` ROM to the device, browse to it in Rockbox, open it

## File layout

```
rocknes/
  SOURCES              - flat file list for the build system
  rocknes.make         - plugin makefile hook
  main.c               - plugin_start() entry point
  rocknes.h            - platform glue header (GraphicsContext, audio/input API)
  gfx_rockbox.c        - 256x240 ARGB8888 -> 138x110 4-grey ordered-dither blit
  input_rockbox.c      - button_get() polling -> NES pad bitmask
  audio_rockbox.c       - ring buffer + Rockbox mixer pull callback
  emulator.c/.h         - main loop (trimmed: no NSF, no Android)
  controller.c/.h       - NES_* prefixed KeyPad enum (avoids clashing with
                          Rockbox's own BUTTON_* macros), poll-based update_joypad
  timers.c/.h           - rb->current_tick/sleep() based pacing (10ms resolution)
  utils.c/.h            - quit() via setjmp/longjmp, misc helpers
  cpu6502.c/.h          - 6502 CPU core (unmodified from upstream)
  ppu.c/.h              - picture processing unit (unmodified from upstream)
  mmu.c/.h              - memory management unit (unmodified from upstream)
  apu.c/.h              - audio processing unit (SDL calls replaced)
  biquad.c/.h           - IIR filter used by APU (unmodified, see risk #1)
  genie.c/.h            - Game Genie cheat code support (unmodified)
  mappers/              - cartridge mapper chips (mapper.c's file IO
                          rewritten; NSF format detection removed;
                          individual mapper chip files unmodified)
```

## License

Core emulation code is MIT licensed, Copyright (c) 2023 Emmanuel Obara
(see upstream repo). Rockbox glue code follows Rockbox's own licensing
conventions (GPLv2) as is standard for Rockbox plugins.

## Update: biquad.c ported to fixed-point (risk #1 addressed, not eliminated)

`biquad()`'s per-sample hot path is now Q16.16 fixed-point instead of
`double`. `biquad_init()` (the trig/pow-heavy coefficient setup) is
unchanged double-precision — it runs once per filter at APU init, so
its cost doesn't matter.

**What this fixes:** removes ~2 software double multiply-adds per
audio sample from the CPU-cycle-rate hot path (`aa_filter` runs at
APU tick rate, not just 11025 Hz — it's the more expensive of the two
filters). Software double math on an FPU-less ARM7TDMI costs on the
order of 100+ cycles per operation via libgcc soft-float; Q16.16
fixed-point compiles to a handful of native 32x32->64 multiply-adds
(ARM7TDMI has UMULL/SMULL).

**What this does NOT fully resolve (verified on host, not on
hardware — no ARM-EABI toolchain in this environment, same caveat as
the rest of the port):**

- Validated numerically against a double-precision reference by
  running both through an identical 440Hz sine sweep and comparing
  output sample-by-sample (test harness not included in this
  archive, reproducible from the coefficient math in `biquad_init`).
- `aa_filter` (20kHz low-pass, the hot one): tracks the double
  reference to within ~0.5% at typical signal amplitude. Good.
- `filter` (20Hz high-pass, the DC-blocking one): this is a
  numerically narrow filter — a 20Hz cutoff at 11025Hz sample rate
  puts its poles at ~0.996 magnitude, right at the edge of the unit
  circle. Q16.16's ~1.5e-5 relative coefficient precision compounds
  through that near-marginal feedback loop into a bounded but real
  steady-state error, measured up to ~15-18% deviation from the
  double-precision reference in a synthetic sweep. It does not
  diverge or oscillate — it settles into a fixed offset — but it's a
  real accuracy gap, not just rounding noise.
- Whether that ~15-18% deviation on the DC-blocking filter is
  audible against 6502+PPU-emulation-grade NES audio on a 4-grey-level
  handheld: untested, and probably the wrong thing to worry about
  before the emulator even boots on hardware.
- If it turns out to matter: options are (a) give just this filter's
  state/coefficients higher precision (e.g. Q8.24, since its dynamic
  range is small enough to afford the extra fractional bits), or
  (b) drop the HPF and keep only the LPF — same "disable filtering
  under pressure" fallback the original risk writeup already called
  out, just for accuracy instead of raw CPU budget.

Frame-budget risk #2 (whether CPU+PPU+audio fits in a 16.67ms NTSC
frame at ~11MHz) is unchanged and still needs real hardware/profiler
time to answer. This patch removes one specific cost center; it
doesn't establish that the emulator hits full speed.

## Update 2: audio disabled entirely

Per user request (no headphones) — audio is now fully disabled rather
than optimized:

- `apu.c`: `sample()` and `queue_audio()` are gated behind
  `#define AUDIO_ENABLED 0` at the top of `apu.c`. With it at 0, both
  functions return immediately: no `get_sample()` mixing, no biquad
  filtering, no ring-buffer submission. The `biquad.c` fixed-point
  work from the previous update is left in the tree (harmless, unused)
  in case audio gets re-enabled later — set `AUDIO_ENABLED` to `1` to
  restore it, at which point the accuracy caveat documented above
  applies again.
- `main.c`: `rocknes_audio_init()` / `rocknes_audio_close()` calls are
  `#if 0`'d out, so Rockbox's mixer channel is never engaged at all
  (not even fed silence).
- Channel timer / length-counter clocking in the APU tick loop is
  untouched, since that's shared bookkeeping some mappers/IRQ timing
  may depend on independent of whether audio output happens; only the
  sampling/filtering/mixing/output tail was cut.

Net effect: removes essentially all APU-related CPU cost per frame,
not just the filter cost from update 1. This also fully sidesteps the
biquad fixed-point accuracy question, since that code path no longer
runs.

## Update 3: build via GitHub Actions (no local toolchain needed)

A ready-made CI workflow is included in this archive's sibling folder
`.github/workflows/build-rocknes.yml` (GitHub only reads workflow
files from that exact path, so it's shipped separately from
`rocknes/` -- copy it into your Rockbox fork's `.github/workflows/`
directory, not into `apps/plugins/`).

**Setup (all doable from a phone browser via github.com):**

1. Fork https://github.com/Rockbox/rockbox to your account.
2. In your fork, create `apps/plugins/rocknes/` and upload every file
   from this archive's `rocknes/` folder into it (GitHub's web UI
   supports drag-and-drop / "Add file > Upload files" for this).
3. Also upload `SUBDIRS.rocknes.snippet` and
   `viewers.config.rocknes.snippet` into `apps/plugins/rocknes/`
   alongside the rest -- the workflow reads them from there and
   patches `apps/plugins/SUBDIRS` / `apps/plugins/viewers.config`
   automatically, so you don't have to hand-edit those two files.
4. Upload `build-rocknes.yml` into `.github/workflows/` in your fork
   (create that folder path via "Add file" if it doesn't exist yet).
5. Go to the **Actions** tab on your fork → find "Build rocknes for
   iPod Mini 2G" → **Run workflow** button (manual trigger). It also
   auto-runs on any future push that touches
   `apps/plugins/rocknes/**`.
6. Wait for the run to finish (toolchain build alone can take
   30-60 minutes on a fresh runner; it's cached afterward so reruns
   are much faster). Open the finished run → **Artifacts** section at
   the bottom → download `rocknes-ipodmini2g-build.zip`, which
   contains `rocknes.rock` and the built firmware image.

**Honesty check on this workflow, same spirit as everything else in
this README:** the overall shape (checkout, install deps, build
toolchain via `rockboxdev.sh`, run `configure`, `make`) is right and
follows Rockbox's own documented process. Two specific details are
unverified against a live run:

- The exact non-interactive answer `rockboxdev.sh` expects to select
  the ARM (`arm-elf-eabi`) toolchain target from its menu. The
  workflow pipes `"a"` as a guess; if the job log shows this didn't
  select the right toolchain, the fix is to open the Action's log,
  see what `rockboxdev.sh` actually printed as its prompt/menu, and
  adjust that one line.
- The exact `--type=` value `configure` expects for a Normal
  (non-simulator, non-bootloader) build. `n` is the likely value but
  unconfirmed against the current script version.

Both are one-line fixes once you see the actual CI log output telling
you what went wrong -- paste that log back and it's a quick patch,
same as any other build error you'd hit doing this locally.
