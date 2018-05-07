//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "player.h"
#include "dcfilter.h"
#include "vumonitor.h"
#include <getopt.h>
#include <string>
#include <bitset>
#include <memory>
#include <adlmidi.h>
#include <stdint.h>

extern std::unique_ptr<Player> player;
extern std::string player_bank_file;
extern int player_volume;
extern DcFilter dcfilter[2];
extern VuMonitor lvmonitor[2];
extern double lvcurrent[2];
extern double cpuratio;
static constexpr double dccutoff = 5.0;
static constexpr double lvrelease = 20e-3;

struct Program {
    unsigned gm = 0;
    /* TODO bank msb lsb */
};
extern Program channel_map[16];

extern unsigned midi_channel_note_count[16];
extern std::bitset<128> midi_channel_note_active[16];

static constexpr unsigned default_nchip = 4;
static constexpr unsigned midi_message_max_size = 64;
static constexpr unsigned midi_buffer_size = 1024;

extern Player_Type arg_player_type;
extern unsigned arg_nchip;
extern const char *arg_bankfile;
extern int arg_emulator;

void generic_usage(const char *progname, const char *more_options);
int generic_getopt(int argc, char *argv[], const char *more_options, void(&usagefn)());

void initialize_player(Player_Type pt, unsigned sample_rate, unsigned nchip, const char *bankfile, int emulator);
void player_ready();
void play_midi(const uint8_t *msg, unsigned len);
void generate_outputs(float *left, float *right, unsigned nframes, unsigned stride);

void interface_exec();
