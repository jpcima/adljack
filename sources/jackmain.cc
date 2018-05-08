//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <stdexcept>
#include <system_error>
#include <stdio.h>

static jack_client_t *client;
static jack_port_t *midiport;
static jack_port_t *outport[2];

static int process(jack_nframes_t nframes, void *)
{
    void *midi = jack_port_get_buffer(midiport, nframes);
    float *left = (float *)jack_port_get_buffer(outport[0], nframes);
    float *right = (float *)jack_port_get_buffer(outport[1], nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        jack_midi_event_t event;
        if (jack_midi_event_get(&event, midi, i) == 0)
            play_midi(event.buffer, event.size);
    }

    generate_outputs(left, right, nframes, 1);
    return 0;
}

static void usage()
{
    generic_usage("adljack", "");
}

std::string get_program_title()
{
    std::string name = "ADLjack";
    if (client) {
        name.push_back(' ');
        name.push_back('[');
        name.append(jack_get_client_name(client));
        name.push_back(']');
    }
    return name;
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

    client = jack_client_open("ADLjack", JackNoStartServer, nullptr);
    if (!client)
        throw std::runtime_error("error creating Jack client");

    midiport = jack_port_register(client, "MIDI", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput|JackPortIsTerminal, 0);
    outport[0] = jack_port_register(client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);
    outport[1] = jack_port_register(client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);

    if (!midiport || !outport[0] || !outport[1])
        throw std::runtime_error("error creating Jack ports");

    jack_nframes_t bufsize = jack_get_buffer_size(client);
    unsigned samplerate = jack_get_sample_rate(client);
    fprintf(stderr, "Jack client \"%s\" fs=%u bs=%u\n",
            jack_get_client_name(client), samplerate, bufsize);

    initialize_player(arg_player_type, samplerate, arg_nchip, arg_bankfile, arg_emulator);

    jack_set_process_callback(client, process, nullptr);
    jack_activate(client);
    player_ready();

    //
    interface_exec();

    //
    jack_deactivate(client);
    jack_client_close(client);

    return 0;
}
