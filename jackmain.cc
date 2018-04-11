//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"
#include <jack/jack.h>
#include <jack/midiport.h>
#include <getopt.h>
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

int main(int argc, char *argv[])
{
    unsigned nchip = default_nchip;
    const char *bankfile = nullptr;
    int emulator = -1;

    for (int c; (c = getopt(argc, argv, "hp:n:b:e:")) != -1;) {
        switch (c) {
        case 'p':
            ::player_type = player_by_name(optarg);
            if ((int)::player_type == -1) {
                fprintf(stderr, "invalid player name\n");
                return 1;
            }
            break;
        case 'n':
            nchip = std::stoi(optarg);
            if ((int)nchip < 1) {
                fprintf(stderr, "invalid number of chips\n");
                return 1;
            }
            break;
        case 'b':
            bankfile = optarg;
            break;
        case 'e':
            emulator = std::stoi(optarg);
            break;
        case 'h':
            usage();
            return 0;
        default:
            usage();
            return 1;
        }
    }

    if (argc != optind)
        return 1;

    client = jack_client_open("adljack", JackNoStartServer, nullptr);
    if (!client)
        throw std::runtime_error("error creating Jack client");

    midiport = jack_port_register(client, "midi", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput|JackPortIsTerminal, 0);
    outport[0] = jack_port_register(client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);
    outport[1] = jack_port_register(client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput|JackPortIsTerminal, 0);

    if (!midiport || !outport[0] || !outport[1])
        throw std::runtime_error("error creating Jack ports");

    jack_nframes_t bufsize = jack_get_buffer_size(client);
    unsigned samplerate = jack_get_sample_rate(client);
    fprintf(stderr, "Jack client \"%s\" fs=%u bs=%u\n",
            jack_get_client_name(client), samplerate, bufsize);

#ifdef TEST_PCM16_TO32
    buffer = new int32_t[2 * bufsize];
#else
    buffer = new int16_t[2 * bufsize];
#endif

    initialize_player(samplerate, nchip, bankfile, emulator);

    jack_set_process_callback(client, process, nullptr);
    jack_activate(client);
    player_ready();

    //
    interface_exec();

    return 0;
}
