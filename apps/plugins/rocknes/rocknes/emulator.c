#include "emulator.h"
#include "controller.h"
#include "mapper.h"
#include "timers.h"
#include "utils.h"
#include "rocknes.h"
#include <string.h>

static uint64_t PERIOD;
static uint16_t TURBO_SKIP;

void init_emulator(Emulator* emulator, const char* rom_path){
    memset(emulator, 0, sizeof(Emulator));
    load_file((char*)rom_path, NULL, &emulator->mapper);
    emulator->type = emulator->mapper.type;
    emulator->mapper.emulator = emulator;

    if(emulator->type == PAL) {
        PERIOD = 1000000000ULL / PAL_FRAME_RATE;
        TURBO_SKIP = PAL_FRAME_RATE / PAL_TURBO_RATE;
    } else {
        PERIOD = 1000000000ULL / NTSC_FRAME_RATE;
        TURBO_SKIP = NTSC_FRAME_RATE / NTSC_TURBO_RATE;
    }

    get_graphics_context(&emulator->g_ctx);

    init_mem(emulator);
    init_ppu(emulator);
    init_cpu(emulator);
    init_APU(emulator);
    init_timer(&emulator->timer, PERIOD);

    emulator->exit = 0;
    emulator->pause = 0;
}

void tick_master_clock(Emulator* emulator) {
    emulator->cpu.t_cycles++;
    if (emulator->ppu.enabled) {
        execute_ppu(&emulator->ppu);
        execute_ppu(&emulator->ppu);
        execute_ppu(&emulator->ppu);
        if (emulator->type == PAL) {
            emulator->PAL_check++;
            if (emulator->PAL_check == 5) {
                execute_ppu(&emulator->ppu);
                emulator->PAL_check = 0;
            }
        }
    }
    if (emulator->mem.strobe && emulator->apu.cycles & 1) {
        emulator->mem.joy1.reg = emulator->mem.joy1.status & 0xff;
        emulator->mem.joy2.reg = emulator->mem.joy2.status & 0xff;
    }
    execute_apu(&emulator->apu);
}

void run_emulator(Emulator* emulator){
    JoyPad* joy1 = &emulator->mem.joy1;
    JoyPad* joy2 = &emulator->mem.joy2;
    PPU* ppu = &emulator->ppu;
    ppu->enabled = 1;
    c6502* cpu = &emulator->cpu;
    APU* apu = &emulator->apu;
    GraphicsContext* g_ctx = &emulator->g_ctx;
    Timer* timer = &emulator->timer;

    while (!emulator->exit) {
        mark_start(timer);

        /* single-player only: the Mini 2G's control surface can't
         * usefully drive two pads, so joy2 stays idle */
        uint16_t status = rocknes_poll_input();
        update_joypad(joy1, status);
        update_joypad(joy2, 0);

        if (rocknes_should_exit()) {
            emulator->exit = 1;
            break;
        }

        /* soft-reset chord kept from upstream: START+SELECT together */
        if ((joy1->status & 0xc) == 0xc) {
            reset_emulator(emulator);
        }

        if (ppu->frames % TURBO_SKIP == 0) {
            turbo_trigger(joy1);
            turbo_trigger(joy2);
        }

        if (!emulator->pause) {
            while (!ppu->render) {
                execute(cpu);
            }
            render_graphics(g_ctx, ppu->screen);
            ppu->render = 0;
            queue_audio(apu, g_ctx);
            mark_end(timer);
            adjusted_wait(timer);
        } else {
            wait(IDLE_SLEEP);
        }
    }
}

void reset_emulator(Emulator* emulator) {
    reset_cpu(&emulator->cpu);
    reset_APU(&emulator->apu);
    reset_ppu(&emulator->ppu);
    if(emulator->mapper.reset != NULL) {
        emulator->mapper.reset(&emulator->mapper);
    }
}

void free_emulator(Emulator* emulator){
    exit_APU();
    exit_ppu(&emulator->ppu);
    free_mapper(&emulator->mapper);
    free_graphics(&emulator->g_ctx);
    release_timer(&emulator->timer);
}
