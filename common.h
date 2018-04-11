//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "player_traits.h"
#include "dcfilter.h"
#include "vumonitor.h"
#include <string>
#include <vector>
#include <adlmidi.h>
#include <stdint.h>

// TESTING 32bitmix branch of ADLMIDI
#define TEST_PCM16_TO32 1

extern void *player;
extern Player_Type player_type;
#ifdef TEST_PCM16_TO32
extern int32_t *buffer;
#else
extern int16_t *buffer;
#endif
extern DcFilter dcfilter[2];
extern VuMonitor lvmonitor[2];
extern double lvcurrent[2];
static constexpr double dccutoff = 5.0;
static constexpr double lvrelease = 20e-3;

static constexpr unsigned default_nchip = 4;
static constexpr unsigned midi_message_max_size = 64;
static constexpr unsigned midi_buffer_size = 1024;

void generic_usage(const char *progname, const char *more_options);

void initialize_player(unsigned sample_rate, unsigned nchip, const char *bankfile, int emulator);
void player_ready();
void play_midi(const uint8_t *msg, unsigned len);
void generate_outputs(float *left, float *right, unsigned nframes, unsigned stride);

const char *player_name(Player_Type pt);
Player_Type player_by_name(const char *name);
std::vector<std::string> enumerate_emulators(Player_Type pt);

void interface_exec();
