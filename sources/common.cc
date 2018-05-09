//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"
#include "tui.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#if defined(_WIN32)
#    include <windows.h>
#else
#    include <syslog.h>
#endif
namespace stc = std::chrono;

std::unique_ptr<Player> player[player_type_count];
std::string player_bank_file[player_type_count];

std::vector<Emulator_Id> emulator_ids;
unsigned active_emulator_id = (unsigned)-1;

int player_volume = 100;
DcFilter dcfilter[2];
VuMonitor lvmonitor[2];
double lvcurrent[2] = {};
double cpuratio = 0;
Program channel_map[16];
unsigned midi_channel_note_count[16] = {};
std::bitset<128> midi_channel_note_active[16];

Player_Type arg_player_type = Player_Type::OPL3;
unsigned arg_nchip = default_nchip;
const char *arg_bankfile = nullptr;
unsigned arg_emulator = 0;
#if defined(ADLJACK_USE_CURSES)
bool arg_simple_interface = false;
#endif

void generic_usage(const char *progname, const char *more_options)
{
    const char *usage_string =
        "Usage: %s [-p player] [-n num-chips] [-b bank.wopl] [-e emulator]"
#if defined(ADLJACK_USE_CURSES)
        " [-t]"
#endif
        "%s\n";

    fprintf(stderr, usage_string, progname, more_options);

    fprintf(stderr, "Available players:\n");
    for (Player_Type pt : all_player_types) {
        fprintf(stderr, "   * %s\n", Player::name(pt));
    }

    for (Player_Type pt : all_player_types) {
        std::vector<std::string> emus = Player::enumerate_emulators(pt);
        size_t emu_count = emus.size();
        fprintf(stderr, "Available emulators for %s:\n", Player::name(pt));
        for (size_t i = 0; i < emu_count; ++i)
            fprintf(stderr, "   * %zu: %s\n", i, emus[i].c_str());
    }
}

int generic_getopt(int argc, char *argv[], const char *more_options, void(&usagefn)())
{
    const char *basic_optstr = "hp:n:b:e:"
#if defined(ADLJACK_USE_CURSES)
        "t"
#endif
        ;

    std::string optstr = std::string(basic_optstr) + more_options;

    for (int c; (c = getopt(argc, argv, optstr.c_str())) != -1;) {
        switch (c) {
        case 'p':
            ::arg_player_type = Player::type_by_name(optarg);
            if ((int)::arg_player_type == -1) {
                fprintf(stderr, "Invalid player name.\n");
                exit(1);
            }
            break;
        case 'n':
            arg_nchip = std::stoi(optarg);
            if ((int)arg_nchip < 1) {
                fprintf(stderr, "Invalid number of chips.\n");
                exit(1);
            }
            break;
        case 'b':
            arg_bankfile = optarg;
            break;
        case 'e':
            arg_emulator = std::stoi(optarg);
            break;
        case 'h':
            usagefn();
            exit(0);
#if defined(ADLJACK_USE_CURSES)
        case 't':
            arg_simple_interface = true;
            break;
#endif
        default:
            return c;
        }
    }

    return -1;
}

bool initialize_player(Player_Type pt, unsigned sample_rate, unsigned nchip, const char *bankfile, unsigned emulator, bool quiet)
{
    qfprintf(quiet, stderr, "%s version %s\n", Player::name(pt), Player::version(pt));

    for (unsigned i = 0; i < player_type_count; ++i) {
        Player *player = Player::create((Player_Type)i, sample_rate);
        if (!player) {
            qfprintf(quiet, stderr, "Error instantiating player.\n");
            return false;
        }
        ::player[i].reset(player);

#pragma message("Using my own bank embed for OPN2. Remove this in the future.")
        if ((Player_Type)i == Player_Type::OPN2) {
            static const uint8_t bank[] = {
                #include "embedded-banks/opn2.h"
            };
            if (!player->load_bank_data(bank, sizeof(bank))) {
                qfprintf(quiet, stderr, "Error loading bank data.\n");
                return false;
            }
        }

        std::vector<std::string> emus = Player::enumerate_emulators((Player_Type)i);
        unsigned emu_count = emus.size();
        for (unsigned j = 0; j < emu_count; ++j) {
            Emulator_Id id { (Player_Type)i, j };
            emulator_ids.push_back(id);
        }
    }

    auto emulator_id_pos = std::find(
        emulator_ids.begin(), emulator_ids.end(),
        Emulator_Id{ pt, emulator });
    if (emulator_id_pos == emulator_ids.end()) {
        qfprintf(quiet, stderr, "The given emulator does not exist.\n");
        return 1;
    }
    ::active_emulator_id = std::distance(emulator_ids.begin(), emulator_id_pos);

    Player &player = *::player[(unsigned)pt];
    if (!player.set_emulator(emulator)) {
        qfprintf(quiet, stderr, "Error selecting emulator.\n");
        return 1;
    }

    qfprintf(quiet, stderr, "Using emulator \"%s\"\n", player.emulator_name());

    if (!bankfile) {
        qfprintf(quiet, stderr, "Using default banks.\n");
    }
    else {
        if (!player.load_bank_file(bankfile)) {
            qfprintf(quiet, stderr, "Error loading bank file.\n");
            return 1;
        }
        qfprintf(quiet, stderr, "Using banks from WOPL file.\n");
        ::player_bank_file[(unsigned)pt] = bankfile;
    }

    if (!player.set_chip_count(nchip)) {
        qfprintf(quiet, stderr, "Error setting the number of chips.\n");
        return 1;
    }

    qfprintf(quiet, stderr, "DC filter @ %f Hz, LV monitor @ %f ms\n", dccutoff, lvrelease * 1e3);
    for (unsigned i = 0; i < 2; ++i) {
        dcfilter[i].cutoff(dccutoff / sample_rate);
        lvmonitor[i].release(lvrelease * sample_rate);
    }

    return true;
}

void player_ready(bool quiet)
{
    Player &player = active_player();
    qfprintf(quiet, stderr, "%s ready with %u chips.\n",
             Player::name(player.type()), player.chip_count());
}

void play_midi(const uint8_t *msg, unsigned len)
{
    Player &player = active_player();
    auto lock = player.take_lock(std::try_to_lock);
    if (!lock.owns_lock())
        return;

    if (len <= 0)
        return;

    uint8_t status = msg[0];
    if ((status & 0xf0) == 0xf0)
        return;

    uint8_t channel = status & 0x0f;
    switch (status >> 4) {
    case 0b1001: {
        if (len < 3) break;
        unsigned vel = msg[2] & 0x7f;
        if (vel != 0) {
            unsigned note = msg[1] & 0x7f;
            player.rt_note_on(channel, note, vel);
            if (!midi_channel_note_active[channel][note]) {
                ++midi_channel_note_count[channel];
                midi_channel_note_active[channel][note] = true;
            }
            break;
        }
    }
    case 0b1000: {
        if (len < 3) break;
        unsigned note = msg[1] & 0x7f;
        player.rt_note_off(channel, note);
        if (midi_channel_note_active[channel][note]) {
            --midi_channel_note_count[channel];
            midi_channel_note_active[channel][note] = false;
        }
        break;
    }
    case 0b1010:
        if (len < 3) break;
        player.rt_note_aftertouch(channel, msg[1] & 0x7f, msg[2] & 0x7f);
        break;
    case 0b1101:
        if (len < 2) break;
        player.rt_channel_aftertouch(channel, msg[1] & 0x7f);
        break;
    case 0b1011: {
        if (len < 3) break;
        unsigned cc = msg[1] & 0x7f;
        unsigned val = msg[2] & 0x7f;
        player.rt_controller_change(channel, cc, val);
        if (cc == 120 || cc == 123) {
            midi_channel_note_count[channel] = 0;
            midi_channel_note_active[channel].reset();
        }
        else if (cc == 0) {
            channel_map[channel].bank_msb = val;
        }
        else if (cc == 32) {
            channel_map[channel].bank_lsb = val;
        }
        break;
    }
    case 0b1100: {
        if (len < 2) break;
        unsigned pgm = msg[1] & 0x7f;
        player.rt_program_change(channel, pgm);
        channel_map[channel].gm = pgm;
        break;
    }
    case 0b1110:
        if (len < 3) break;
        unsigned value = (msg[1] & 0xf7) | ((msg[2] & 0xf7) << 7);
        player.rt_pitchbend(channel, value);
        break;
    }
}

void generate_outputs(float *left, float *right, unsigned nframes, unsigned stride)
{
    if (nframes <= 0)
        return;

    Player &player = active_player();
    auto lock = player.take_lock(std::try_to_lock);
    if (!lock.owns_lock()) {
        for (unsigned i = 0; i < nframes; ++i) {
            float *leftp = &left[i * stride];
            float *rightp = &right[i * stride];
            *leftp = 0;
            *rightp = 0;
        }
        return;
    }

    Player::Audio_Format format;
    format.type = ADLMIDI_SampleType_F32;
    format.containerSize = sizeof(float);
    format.sampleOffset = stride * sizeof(float);
    stc::steady_clock::time_point t_before_gen = stc::steady_clock::now();
    player.generate(nframes, left, right, format);
    stc::steady_clock::time_point t_after_gen = stc::steady_clock::now();
    lock.unlock();

    DcFilter &dclf = dcfilter[0];
    DcFilter &dcrf = dcfilter[1];
    double lvcurrent[2];

    const double outputgain = ::player_volume * (1.0 / 100.0);
    for (unsigned i = 0; i < nframes; ++i) {
        float *leftp = &left[i * stride];
        float *rightp = &right[i * stride];
        double left_sample = dclf.process(outputgain * *leftp);
        double right_sample = dcrf.process(outputgain * *rightp);
        lvcurrent[0] = lvmonitor[0].process(left_sample);
        lvcurrent[1] = lvmonitor[1].process(right_sample);
        *leftp = left_sample;
        *rightp = right_sample;
    }

    ::lvcurrent[0] = lvcurrent[0];
    ::lvcurrent[1] = lvcurrent[1];

    stc::steady_clock::duration d_gen = t_after_gen - t_before_gen;
    double d_sec = 1e-6 * stc::duration_cast<stc::microseconds>(d_gen).count();
    ::cpuratio = d_sec / ((double)nframes / player.sample_rate());
}

void dynamic_switch_emulator_id(unsigned index)
{
    if (index == active_emulator_id)
        return;

    Emulator_Id old_id = emulator_ids[active_emulator_id];
    Emulator_Id new_id  = emulator_ids[index];

    Player &player = active_player();
    auto lock = player.take_lock();

    player.panic();
    if (old_id.player == new_id.player) {
        player.set_emulator(new_id.emulator);
    }
    else {
        Player &new_player = *::player[(unsigned)new_id.player];
        new_player.set_emulator(new_id.emulator);
        new_player.set_chip_count(player.chip_count());
        // transmit bank change and program change events
        for (unsigned channel = 0; channel < 16; ++channel) {
            new_player.rt_bank_change_msb(channel, channel_map[channel].bank_msb);
            new_player.rt_bank_change_lsb(channel, channel_map[channel].bank_lsb);
            new_player.rt_program_change(channel, channel_map[channel].gm);
        }
    }

    ::active_emulator_id = index;
}

//------------------------------------------------------------------------------
static void print_volume_bar(FILE *out, unsigned size, double vol)
{
    if (size < 2)
        return;
    fprintf(out, "[");
    for (unsigned i = 0; i < size; ++i) {
        double ref = (double)i / size;
        fprintf(out, "%c", (vol > ref) ? '*' : '-');
    }
    fprintf(out, "]");
}

static void simple_interface_exec(void(*idle_proc)(void *), void *idle_data)
{
    while (1) {
        if (interface_interrupted()) {
            fprintf(stderr, "Interrupted.\n");
            break;
        }

        if (idle_proc)
            idle_proc(idle_data);

        fprintf(stderr, "\033[2K");
        double volumes[2] = {lvcurrent[0], lvcurrent[1]};
        const char *names[2] = {"Left", "Right"};

        // enables logarithmic view for perceptual volume, otherwise linear.
        //  (better use linear to watch output for clipping)
        const bool logarithmic = false;

        for (unsigned channel = 0; channel < 2; ++channel) {
            double vol = volumes[channel];
            if (logarithmic && vol > 0) {
                double db = 20 * log10(vol);
                const double dbmin = -60.0;
                vol = (db - dbmin) / (0 - dbmin);
            }
            fprintf(stderr, " %c ", names[channel][0]);
            print_volume_bar(stderr, 30, vol);
            fprintf(stderr, (vol > 1.0) ? " \033[7mCLIP\033[0m" : "     ");
        }

        fprintf(stderr, "\r");
        fflush(stderr);

        std::this_thread::sleep_for(stc::milliseconds(50));
    }
}

void interface_exec(void(*idle_proc)(void *), void *idle_data)
{
#if defined(ADLJACK_USE_CURSES)
    if (arg_simple_interface)
        simple_interface_exec(idle_proc, idle_data);
    else
        curses_interface_exec(idle_proc, idle_data);
#else
    simple_interface_exec();
#endif
}

static sig_atomic_t interrupted_by_signal = 0;

void handle_signals()
{
#if defined(_WIN32)
    /* not implemented */
#else
    sigset_t sigs;
    int nsignal = 0;

    sigemptyset(&sigs);
    for (int signo : {SIGINT, SIGTERM}) {
        sigaddset(&sigs, signo);
        nsignal = signo + 1;
    }

    for (int sig = 1; sig < nsignal; ++sig) {
        if (!sigismember(&sigs, sig))
            continue;
        struct sigaction sa = {};
        sa.sa_handler = +[](int) { ::interrupted_by_signal = 1; };
        sa.sa_mask = sigs;
        if (sigaction(sig, &sa, nullptr) == -1)
            throw std::system_error(errno, std::generic_category(), "sigaction");
    }

    std::thread handler_thread([]() { for (;;) pause(); });
    handler_thread.detach();

    if (pthread_sigmask(SIG_BLOCK, &sigs, nullptr) == -1)
        throw std::system_error(errno, std::generic_category(), "pthread_sigmask");
#endif
}

bool interface_interrupted()
{
    return ::interrupted_by_signal;
}

void debug_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    debug_vprintf(fmt, ap);
    va_end(ap);
}

#if defined(_WIN32)
void debug_vprintf(const char *fmt, va_list ap)
{
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    buf[sizeof(buf) - 1] = '\0';
    OutputDebugStringA(buf);
}
#else
void debug_vprintf(const char *fmt, va_list ap)
{
    vsyslog(LOG_INFO, fmt, ap);
}
#endif

void qfprintf(bool q, FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    qvfprintf(q, stream, fmt, ap);
    va_end(ap);
}

void qvfprintf(bool q, FILE *stream, const char *fmt, va_list ap)
{
    if (q)
        debug_vprintf(fmt, ap);
    else
        vfprintf(stream, fmt, ap);
}
