#pragma once

#include "cpu6502.h"
#include "ppu.h"
#include "mmu.h"
#include "apu.h"
#include "mapper.h"
#include "rocknes.h"   /* GraphicsContext now defined here, SDL-free */
#include "timers.h"

/* NOTE: this is a trimmed port for Rockbox / iPod Mini 2G. Compared to
 * upstream (https://github.com/ObaraEmmanuel/NES) this drops:
 *   - the NSF (NES Sound Format) music player mode
 *   - Android touch/gamepad glue
 *   - the nametable debug-view mode
 * None of that affects normal ROM (.nes) emulation.
 */

// frame rate in Hz
#define NTSC_FRAME_RATE 60
#define PAL_FRAME_RATE 50

// turbo keys toggle rate (Hz) - unused on this control scheme but kept
// so controller.c's turbo_trigger() has a consistent divisor
#define NTSC_TURBO_RATE 30
#define PAL_TURBO_RATE 25

// sleep time when emulator is paused in milliseconds
#define IDLE_SLEEP 50


typedef struct Emulator{
    c6502 cpu;
    PPU ppu;
    APU apu;
    Memory mem;
    Mapper mapper;
    GraphicsContext g_ctx;
    Timer timer;

    TVSystem type;

    double time_diff;

    uint8_t exit;
    uint8_t pause;
    uint8_t PAL_check;
} Emulator;


/* rom_path: full path to a .nes file, e.g. "/MUSIC/roms/mario.nes" */
void init_emulator(Emulator* emulator, const char* rom_path);
void reset_emulator(Emulator* emulator);
void tick_master_clock(Emulator* emulator);
void run_emulator(Emulator* emulator);
void free_emulator(Emulator* emulator);
