//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "jackmain.h"
#include "common.h"
#include <stdio.h>

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

int audio_main()
{
    jack_client_u client(jack_client_open("ADLjack", JackNoStartServer, nullptr));
    if (!client) {
        fprintf(stderr, "Error creating Jack client.\n");
        return 1;
    }

    ::program_title = std::string("ADLjack") + " [" + jack_get_client_name(client.get()) + "]";

    Audio_Context ctx;
    ctx.client = client.get();
    ctx.midiport = jack_port_register(client.get(), "MIDI", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput|JackPortIsTerminal, 0);
    ctx.outport[0] = jack_port_register(client.get(), "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);
    ctx.outport[1] = jack_port_register(client.get(), "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);

    if (!ctx.midiport || !ctx.outport[0] || !ctx.outport[1]) {
        fprintf(stderr, "Error creating Jack ports.\n");
        return 1;
    }

    jack_nframes_t bufsize = jack_get_buffer_size(client.get());
    unsigned samplerate = jack_get_sample_rate(client.get());
    fprintf(stderr, "Jack client \"%s\" fs=%u bs=%u\n",
            jack_get_client_name(client.get()), samplerate, bufsize);

    if (!initialize_player(arg_player_type, samplerate, arg_nchip, arg_bankfile, arg_emulator))
        return 1;

    jack_set_process_callback(client.get(), process, &ctx);
    jack_activate(client.get());
    player_ready();

    //
    interface_exec();

    //
    jack_deactivate(client.get());

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
    for (int c; (c = generic_getopt(argc, argv, "", usage)) != -1;) {
        switch (c) {
        default:
            usage();
            return 1;
        }
    }

    if (argc != optind)
        return 1;

    return audio_main();
}
