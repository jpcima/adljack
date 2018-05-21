//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "jackmain.h"
#include "state.h"
#include "insnames.h"
#include "i18n.h"
#include "common.h"
#include <atomic>
#include <system_error>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>

static std::string program_title = "ADLjack";

static int process(jack_nframes_t nframes, void *user_data)
{
    const Audio_Context &ctx = *(Audio_Context *)user_data;

    void *midi = jack_port_get_buffer(ctx.midiport, nframes);
    float *left = (float *)jack_port_get_buffer(ctx.outport[0], nframes);
    float *right = (float *)jack_port_get_buffer(ctx.outport[1], nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        jack_midi_event_t event;
        if (jack_midi_event_get(&event, midi, i) == 0)
            play_midi(event.buffer, event.size);
    }

    generate_outputs(left, right, nframes, 1);
    return 0;
}

static int setup_audio(const char *client_name, Audio_Context &ctx, bool quiet = false)
{
    jack_client_t *client(jack_client_open(client_name, JackNoStartServer, nullptr));
    if (!client) {
        qfprintf(quiet, stderr, "Error creating Jack client.\n");
        return 1;
    }
    ctx.client.reset(client);

    ::program_title = std::string("ADLjack") + " [" + jack_get_client_name(client) + "]";

    ctx.midiport = jack_port_register(client, "MIDI", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput|JackPortIsTerminal, 0);
    ctx.outport[0] = jack_port_register(client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);
    ctx.outport[1] = jack_port_register(client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);

    if (!ctx.midiport || !ctx.outport[0] || !ctx.outport[1]) {
        qfprintf(quiet, stderr, "Error creating Jack ports.\n");
        return 1;
    }

    jack_nframes_t bufsize = jack_get_buffer_size(client);
    unsigned samplerate = jack_get_sample_rate(client);
    qfprintf(quiet, stderr, "Jack client \"%s\" fs=%u bs=%u\n",
             jack_get_client_name(client), samplerate, bufsize);

    if (!initialize_player(arg_player_type, samplerate, arg_nchip, arg_bankfile, arg_emulator, quiet))
        return 1;

    jack_set_process_callback(client, process, &ctx);
    return 0;
}

#if defined(ADLJACK_USE_NSM)
static bool session_is_open = false;
static std::string session_path;

static bool session_file_load(const char *path)
{
    FILE_u stream(fopen(session_path.c_str(), "rb"));

    if (!stream) {
        debug_printf("Cannot open session file '%s'.\n", session_path.c_str());
        return false;
    }

    if (fseeko(stream.get(), 0, SEEK_END) != 0) {
        debug_printf("Input error on file '%s'.\n", session_path.c_str());
        return false;
    }

    uint64_t size = ftello(stream.get());
    if (size > session_size_max) {
        debug_printf("Session file too large '%s'.\n", session_path.c_str());
        return false;
    }

    std::vector<uint8_t> data(size);
    debug_printf("Session file is %zu bytes long.\n", data.size());

    if (fseek(stream.get(), 0, SEEK_SET) != 0 || fread(data.data(), 1, size, stream.get()) != size) {
        debug_printf("Read error on file '%s'.\n", session_path.c_str());
        return false;
    }

    if (!load_state(data)) {
        debug_printf("Error loading state data '%s'.\n", session_path.c_str());
        return false;
    }

    return true;
}

static int session_open(const char *path, const char *display_name, const char *client_id, char **out_msg, void *user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;

    debug_printf("About to open the session.");

    if (::session_is_open) {
        debug_printf("The session is already open.");
        return 1;
    }

    const bool quiet = true;
    if (setup_audio(client_id, ctx, quiet) != 0)
        return 1;

    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        debug_printf("Error creating the session directory.");
        return 1;
    }

    std::string session_path = std::string(path) + "/session.dat";
    ::session_path = session_path;
    if (!session_file_load(session_path.c_str()))
        debug_printf("Could not load session file. Continuing anyway.");

    jack_client_t *client = ctx.client.get();
    jack_activate(client);
    player_ready(quiet);

    ::session_is_open = true;
    debug_printf("Session opened.");
    return 0;
}

static bool session_file_save(const char *path)
{
    std::vector<uint8_t> data;
    if (!save_state(data)) {
        debug_printf("Error saving state data.\n");
        return false;
    }

    if (data.size() > session_size_max) {
        debug_printf("Session data too large '%s'.\n");
        return false;
    }

    FILE_u stream(fopen(path, "wb"));

    if (!stream) {
        debug_printf("Cannot open session file '%s'.\n", session_path.c_str());
        return false;
    }

    if (fwrite(data.data(), 1, data.size(), stream.get()) != data.size() || fflush(stream.get()) != 0) {
        debug_printf("Write error on file '%s'.\n", session_path.c_str());
        return false;
    }

    return true;
}

static int session_save(char **out_msg, void *user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;

    debug_printf("About to save the session.");

    const std::string &session_path = ::session_path;
    if (!session_file_save(session_path.c_str())) {
        debug_printf("Could not save session file.");
        return ERR_GENERAL;
    }

    debug_printf("Session saved.");
    return 0;
}

static std::vector<std::string> get_xdg_desktops()
{
    std::vector<std::string> desktops;
    desktops.reserve(8);
    for (const char *pos = getenv("XDG_CURRENT_DESKTOP"); pos && *pos;) {
        const char *end = strchr(pos, ';');
        std::string value = end ? std::string(pos, end) : std::string(pos);
        desktops.push_back(value);
        pos = end ? (end + 1) : nullptr;
    }
    return desktops;
}

static std::vector<std::vector<const char *>> get_terminal_choices()
{
    std::vector<std::vector<const char *>> terminals;
    terminals.reserve(8);
    for (const std::string &desktop : get_xdg_desktops()) {
#if 0
        if (desktop == "GNOME")
            // TODO does not close!
            terminals.push_back({"gnome-terminal", "--wait", "--hide-menubar", "--"});
        else
#endif
        if (desktop == "KDE")
            terminals.push_back({"konsole", "--hide-menubar", "--hide-tabbar", "-e"});
        else if (desktop == "MATE")
            terminals.push_back({"mate-terminal", "--disable-factory", "--hide-menubar", "-e"});
        else if (desktop == "XFCE") {
            terminals.push_back({"xfce-terminal", "--disable-server", "--hide-menubar", "--hide-toolbar", "--hide-scrollbar", "-e"});
            terminals.push_back({"xfce4-terminal", "--disable-server", "--hide-menubar", "--hide-toolbar", "--hide-scrollbar", "-e"});
        }
        else if (desktop == "LXDE")
            terminals.push_back({"lxterminal", "--no-remote", "-e"});
    }
    terminals.push_back({"xterm", "-e"});
    terminals.push_back({"rxvt", "-e"});
    return terminals;
}

static void execvp_in_xterminal(int argc, char *argv[])
{
    std::vector<const char *> args;
    args.reserve(argc + 8);
    for (const std::vector<const char *> &terminal_argv : get_terminal_choices()) {
        args.clear();
        for (const char *arg : terminal_argv)
            args.push_back(arg);
        for (unsigned i = 0, n = argc; i < n; ++i)
            args.push_back(argv[i]);
        args.push_back(nullptr);
        execvp(args[0], (char **)args.data());
    }
}

static void session_log(void *, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    debug_vprintf(fmt, ap);
    va_end(ap);
}

static int session_main(int argc, char *argv[], const char *url, Audio_Context &ctx)
{
    debug_printf("Entering session management.");

#if defined(ADLJACK_USE_GRAPHIC_TERMINAL)
    bool in_text_terminal = ::arg_simple_interface;
#else
    bool in_text_terminal = true;
#endif
    if (in_text_terminal && !getenv("ADLJACK_DEDICATED_XTERMINAL")) {
        if (setenv("ADLJACK_DEDICATED_XTERMINAL", "1", 1) == -1 ||
            setenv("ADLJACK_SESSION_PROGNAME", argv[0], 1) == -1)
            throw std::system_error(errno, std::generic_category(), "setenv");
        execvp_in_xterminal(argc, argv);
        return 1;
    }

    handle_signals();

    nsm_client_u nsm;
    nsm.reset(nsm_new());
    nsm_set_log_callback(nsm.get(), &session_log, nullptr);
    nsm_set_open_callback(nsm.get(), &session_open, &ctx);
    nsm_set_save_callback(nsm.get(), &session_save, &ctx);
    if (nsm_init(nsm.get(), url) != 0) {
        debug_printf("Error initializing session management.");
        return 1;
    }
    ctx.nsm = nsm.get();

    //
    const char *progname = argv[0];
    if (const char *env = getenv("ADLJACK_SESSION_PROGNAME"))
        progname = env;
    debug_printf("Announcing as %s.", progname);
    nsm_send_announce(nsm.get(), "ADLjack", "", progname);

    //
    auto idle_proc =
        [](void *user_data) {
            Audio_Context &ctx = *(Audio_Context *)user_data;
            nsm_check_nowait(ctx.nsm);
        };
    interface_exec(+idle_proc, &ctx);

    //
    nsm.reset();
    if (jack_client_t *client = ctx.client.get())
        jack_deactivate(client);

    return 0;
}
#endif

static int audio_main(int argc, char *argv[])
{
    Audio_Context ctx;

#if defined(ADLJACK_USE_NSM)
    if (const char *nsm_url = getenv("NSM_URL"))
        return session_main(argc, argv, nsm_url, ctx);
#endif

    handle_signals();

    if (setup_audio("ADLjack", ctx) != 0)
        return 1;

    jack_client_t *client = ctx.client.get();
    jack_activate(client);
    player_ready();

    //
    interface_exec(nullptr, nullptr);

    //
    jack_deactivate(client);

    return 0;
}

static void usage()
{
    generic_usage("adljack", "");
}

std::string get_program_title()
{
    return ::program_title;
}

int main(int argc, char *argv[])
{
    i18n_setup();
    midi_db.init();

    for (int c; (c = generic_getopt(argc, argv, "", usage)) != -1;) {
        switch (c) {
        default:
            usage();
            return 1;
        }
    }

    if (argc != optind)
        return 1;

    openlog("ADLjack", 0, LOG_USER);

    return audio_main(argc, argv);
}
