//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include "player.h"
#include "dcfilter.h"
#include "vumonitor.h"
#include "ini_processing.h"
#include <ring_buffer/ring_buffer.h>
#include <getopt.h>
#include <string>
#include <bitset>
#include <memory>
#include <adlmidi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

extern std::unique_ptr<Player> player[player_type_count];
extern std::string player_bank_file[player_type_count];

extern IniProcessing configFile;

struct Emulator_Id {
    Emulator_Id()
        {}
    Emulator_Id(Player_Type player, unsigned emulator)
        : player(player), emulator(emulator) {}
    explicit operator bool() const
        { return player != (Player_Type)-1; }
    Player_Type player = (Player_Type)-1;
    unsigned emulator = 0;
};

inline bool operator==(const Emulator_Id &a, const Emulator_Id &b)
    { return a.player == b.player && a.emulator == b.emulator; }
inline bool operator!=(const Emulator_Id &a, const Emulator_Id &b)
    { return !operator==(a, b); }

extern std::vector<Emulator_Id> emulator_ids;
extern unsigned active_emulator_id;

inline bool have_active_player()
    { return ::active_emulator_id != (unsigned)-1; }
inline unsigned active_player_count()
    { return emulator_ids.size(); }
inline unsigned active_player_index()
    { return (unsigned)emulator_ids[::active_emulator_id].player; }
inline Player &active_player()
    { return *::player[active_player_index()]; }
inline std::string &active_bank_file()
    { return ::player_bank_file[active_player_index()]; }

extern int player_volume;
extern DcFilter dcfilter[2];
extern VuMonitor lvmonitor[2];
extern double lvcurrent[2];
extern double cpuratio;
static constexpr double dccutoff = 5.0;
static constexpr double lvrelease = 20e-3;

static constexpr int volume_min = 0;
static constexpr int volume_max = 500;

struct Program {
    unsigned gm = 0;
    unsigned bank_msb = 0;
    unsigned bank_lsb = 0;
};
extern Program channel_map[16];

extern unsigned midi_channel_note_count[16];
extern std::bitset<128> midi_channel_note_active[16];
extern unsigned midi_channel_last_note_p1[16];

extern std::unique_ptr<Ring_Buffer> fifo_notify;
static constexpr unsigned fifo_notify_size = 8192;

enum Notification_Type {
    Notify_TextInsert,
    Notify_Channels,
};
struct Notify_Header {
    Notification_Type type;
    unsigned size;
};

bool notify(Notification_Type type, const uint8_t *data, unsigned len);

static constexpr unsigned default_nchip = 2;
static constexpr unsigned midi_message_max_size = 64;
static constexpr unsigned midi_buffer_size = 64 * 1024;

extern Player_Type arg_player_type;
extern unsigned arg_nchip;
extern const char *arg_bankfile;
extern std::string arg_config_file;
extern unsigned arg_emulator;
extern bool arg_autoconnect;
#if defined(ADLJACK_USE_CURSES)
extern bool arg_simple_interface;
#endif

void generic_usage(const char *progname, const char *more_options);
int generic_getopt(int argc, char *argv[], const char *more_options, void(&usagefn)());
void load_config();

bool initialize_player(Player_Type pt, unsigned sample_rate, unsigned nchip, const char *bankfile, unsigned emulator, bool quiet = false);
void player_ready(bool quiet = false);
void play_midi(const uint8_t *msg, unsigned len);
void play_sysex(const uint8_t *msg, unsigned len);
void generate_outputs(float *left, float *right, unsigned nframes, unsigned stride);

void dynamic_switch_emulator_id(unsigned index);

void interface_exec(void(*idle_proc)(void *), void *idle_data);

void handle_signals();
bool interface_interrupted();

void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);

void qfprintf(bool q, FILE *stream, const char *fmt, ...);
void qvfprintf(bool q, FILE *stream, const char *fmt, va_list ap);

struct FILE_Deleter {
    void operator()(FILE *x) { fclose(x); }
};
typedef std::unique_ptr<FILE, FILE_Deleter> FILE_u;

struct STDC_Deleter {
    void operator()(void *x) { free(x); }
};
