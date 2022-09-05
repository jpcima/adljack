//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"
#include "tui.h"
#include "i18n.h"
#include <algorithm>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#if defined(ADLJACK_HAVE_MLOCKALL)
#    include <sys/mman.h>
#endif
#if defined(ADLJACK_GTK3)
#   include <gtk/gtk.h>
#endif
#if defined(_WIN32)
#    include <windows.h>
#else
#    include <syslog.h>
#endif
namespace stc = std::chrono;

std::unique_ptr<Player> player[player_type_count];
std::string player_bank_file[player_type_count];

IniProcessing configFile;

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
unsigned midi_channel_last_note_p1[16] = {};
static unsigned sysex_device_id = 0x10;
static constexpr unsigned sysex_broadcast_id = 0x7f;

std::unique_ptr<Ring_Buffer> fifo_notify;

Player_Type arg_player_type = Player_Type::OPL3;
unsigned arg_nchip = default_nchip;
const char *arg_bankfile = nullptr;
std::string arg_config_file;
unsigned arg_emulator = 0;
bool arg_autoconnect = false;
#if defined(ADLJACK_USE_CURSES)
bool arg_simple_interface = false;
#endif

static bool has_nchip_arg = false;
static bool has_emulator_arg = false;
static bool has_volume_arg = false;
static bool has_playertype_arg = false;

static double channels_update_delay = 50e-3;
static unsigned channels_update_frames;
static unsigned channels_update_left;

void generic_usage(const char *progname, const char *more_options)
{
    std::string usage_string =
        _("Usage:\n    %s [-p player] [-n num-chips] [-b bank.wopl] [-e emulator] [-v volume percent] [-a]");
#if defined(ADLJACK_USE_CURSES)
    usage_string += " [-t]";
#endif
    usage_string += "%s\n";

    fprintf(stderr, usage_string.c_str(), progname, more_options);

    fprintf(stderr, "%s\n", _("Available players:"));
    for (Player_Type pt : all_player_types) {
        fprintf(stderr, "   * %s\n", Player::name(pt));
    }

    for (Player_Type pt : all_player_types) {
        std::vector<Player::Emulator> emus = Player::enumerate_emulators(pt);
        size_t emu_count = emus.size();
        fprintf(stderr, _("Available emulators for %s:\n"), Player::name(pt));
        for (size_t i = 0; i < emu_count; ++i)
            fprintf(stderr, "   * %zu: %s\n", i, emus[i].name);
    }
}

int generic_getopt(int argc, char *argv[], const char *more_options, void(&usagefn)())
{
    const char *basic_optstr = "hp:n:b:e:v:a"
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
                fprintf(stderr, "%s\n", _("Invalid player name."));
                exit(1);
            }
            has_playertype_arg = true;
            break;
        case 'n':
            arg_nchip = std::stoi(optarg);
            if ((int)arg_nchip < 1) {
                fprintf(stderr, "%s\n", _("Invalid number of chips."));
                exit(1);
            }
            has_nchip_arg = true;
            break;
        case 'b':
            arg_bankfile = optarg;
            break;
        case 'e':
            arg_emulator = std::stoi(optarg);
            has_emulator_arg = true;
            break;
        case 'a':
            arg_autoconnect = true;
            break;
        case 'v':
            player_volume = std::stoi(optarg);
            if (player_volume < 0 || player_volume > volume_max) {
                fprintf(stderr, _("Invalid volume (0-%d).\n"), volume_max);
                exit(1);
            }
            has_volume_arg = true;
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

void load_config()
{
    if (arg_config_file.empty()) {
        return; // Load nothing
    }

    configFile.open(arg_config_file);
}

bool initialize_player(Player_Type pt, unsigned sample_rate, unsigned nchip, const char *bankfile, unsigned emulator, bool quiet)
{
    configFile.beginGroup("synth");
    if (!has_emulator_arg) emulator = configFile.value("emulator", emulator).toUInt();
    if (!has_volume_arg) player_volume = configFile.value("volume", player_volume).toInt();
    if (!has_nchip_arg) nchip = configFile.value("nchip", nchip).toUInt();
    if (!has_playertype_arg) pt = (Player_Type)configFile.value("pt", (int)pt).toUInt();

    qfprintf(quiet, stderr, _("%s version %s\n"), Player::name(pt), Player::version(pt));

#if defined(ADLJACK_HAVE_MLOCKALL)
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1)
        qfprintf(quiet, stderr, _("Error locking memory."));
#endif

    ::fifo_notify.reset(new Ring_Buffer(fifo_notify_size));

    for (unsigned i = 0; i < player_type_count; ++i) {
        Player_Type pt = (Player_Type)i;
        Player *player = Player::create(pt, sample_rate);
        if (!player) {
            qfprintf(quiet, stderr, "%s\n", _("Error instantiating player."));
            return false;
        }
        ::player[i].reset(player);

        if (!player->set_embedded_bank(0))
            qfprintf(quiet, stderr, "%s\n", _("Error setting default bank."));

        player->set_soft_pan_enabled(1);

        for (const Player::Emulator &e : Player::enumerate_emulators(pt)) {
            Emulator_Id id { pt, e.id };
            emulator_ids.push_back(id);
        }

        std::string bankname_field = "bankfile-" + std::to_string(i);
        player_bank_file[i] = configFile.value(bankname_field.c_str(), player_bank_file[i]).toString();
        if (!player_bank_file[i].empty() && !::player[i]->load_bank_file(player_bank_file[i].c_str()))
        {
            qfprintf(quiet, stderr, "%s\n", _("Error loading saved bank file for player."));
            return 1;
        }
    }

    auto emulator_id_pos = std::find(
        emulator_ids.begin(), emulator_ids.end(),
        Emulator_Id{ pt, emulator });
    if (emulator_id_pos == emulator_ids.end()) {
        qfprintf(quiet, stderr, "%s\n", _("The given emulator does not exist."));
        return 1;
    }
    ::active_emulator_id = std::distance(emulator_ids.begin(), emulator_id_pos);

    Player &player = *::player[(unsigned)pt];
    if (!player.set_emulator(emulator)) {
        qfprintf(quiet, stderr, "%s\n", _("Error selecting emulator."));
        return 1;
    }

    qfprintf(quiet, stderr, _("Using emulator \"%s\"\n"), player.emulator_name());

    if (!bankfile) {
        qfprintf(quiet, stderr, "%s\n", _("Using default banks."));
    }
    else {
        if (!player.load_bank_file(bankfile)) {
            qfprintf(quiet, stderr, "%s\n", _("Error loading bank file."));
            return 1;
        }
        qfprintf(quiet, stderr, "%s\n", _("Using banks from WOPL file."));
        ::player_bank_file[(unsigned)pt] = bankfile;
    }

    if (!player.set_chip_count(nchip)) {
        qfprintf(quiet, stderr, "%s\n", _("Error setting the number of chips."));
        return 1;
    }

    if (configFile.hasKey("chanalloc")) {
        player.set_channel_alloc_mode(configFile.value("chanalloc", -1).toInt());
    }

    qfprintf(quiet, stderr, _("DC filter @ %f Hz, LV monitor @ %f ms\n"), dccutoff, lvrelease * 1e3);
    for (unsigned i = 0; i < 2; ++i) {
        dcfilter[i].cutoff(dccutoff / sample_rate);
        lvmonitor[i].release(lvrelease * sample_rate);
    }

    ::channels_update_frames = std::ceil(channels_update_delay * sample_rate);
    ::channels_update_left = ::channels_update_frames;

    configFile.endGroup();

    return true;
}

void player_ready(bool quiet)
{
    Player &player = active_player();
    qfprintf(quiet, stderr, _("%s ready with %u chips.\n"),
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
    if (status == 0xf0)
        return play_sysex(msg, len);

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
            midi_channel_last_note_p1[channel] = note + 1;
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
        unsigned value = (msg[1] & 0x7f) | ((msg[2] & 0x7f) << 7);
        player.rt_pitchbend(channel, value);
        break;
    }
}

static void play_roland_sysex(unsigned address, const uint8_t *data, unsigned len)
{
    switch (address) {
    }
}

static void play_roland_sc_sysex(unsigned address, const uint8_t *data, unsigned len)
{
    switch (address) {
    case 0x100000:  // text insert
        notify(Notify_TextInsert, data, (len < 256) ? len : 256); break;
    }
}

void play_sysex(const uint8_t *msg, unsigned len)
{
    if (len < 4 || msg[0] != 0xf0 || msg[len - 1] != 0xf7 ||
        (msg[2] != sysex_device_id && msg[2] != sysex_broadcast_id))
        return;

    Player &player = active_player();
    uint8_t manufacturer = msg[1];
    switch (manufacturer) {
        case 0x7f: // Universal realtime
            player.rt_system_exclusive(msg, len);
            break;
        case 0x41:  // Roland
            if (len < 10)
                break;
            else {
                uint8_t model = msg[3];
                uint8_t mode = msg[4];
                unsigned address = (msg[5] << 16) | (msg[6] << 8) | msg[7];
                // uint8_t checksum = msg[len - 2];
                if (mode == 0x12) {  // receive
                    const uint8_t *data = &msg[8];
                    unsigned datalen = len - 10;
                    switch (model) {
                    case 0x42:
                        play_roland_sysex(address, data, datalen); break;
                    case 0x45:
                        play_roland_sc_sysex(address, data, datalen); break;
                    }
                }
            }
            break;
    }
}

bool notify(Notification_Type type, const uint8_t *data, unsigned len)
{
    Ring_Buffer *fifo = ::fifo_notify.get();
    if (!fifo)
        return false;
    Notify_Header hdr = {type, len};
    if (fifo->size_free() < sizeof(hdr) + len)
        return false;
    fifo->put(hdr);
    fifo->put(data, len);
    return true;
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

    const double outputgain = ::player_volume * (1.0 / 100.0) * player.output_gain();
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

    if (::channels_update_left > nframes)
        ::channels_update_left -= nframes;
    else {
        ::channels_update_left = ::channels_update_frames -
            (nframes - ::channels_update_left) % ::channels_update_frames;

        char buf[2 * (player_max_chips * player_max_channels + 1)];
        char *text = buf;
        char *attr = buf + player_max_chips * player_max_channels + 1;
        player.describe_channels(text, attr, sizeof(buf) / 2);

        unsigned len = std::char_traits<char>::length(text);
        std::move(attr, attr + len, text + len);
        notify(Notify_Channels, (const uint8_t *)buf, 2 * len);
    }
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
        new_player.set_channel_alloc_mode(player.get_channel_alloc_mode());
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
            fprintf(stderr, "%s\n", _("Interrupted."));
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

#ifdef ADLJACK_GTK3
        gtk_main_iteration_do(false);
#endif

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
