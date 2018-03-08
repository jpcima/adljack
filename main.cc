//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "dcfilter.h"
#include <adlmidi.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <getopt.h>
#include <stdexcept>
#include <system_error>
#include <stdio.h>
#include <unistd.h>

static jack_client_t *client;
static jack_port_t *midiport;
static jack_port_t *outport[2];
static ADL_MIDIPlayer *player;
static int16_t *buffer;
static DcFilter dcfilter[2];
static constexpr double dccutoff = 5.0;

static constexpr unsigned default_nchip = 4;

static void play_midi(const uint8_t *msg, unsigned len)
{
    ADL_MIDIPlayer &player = *::player;

    if (len <= 0)
        return;

    uint8_t status = msg[0];
    if ((status & 0xf0) == 0xf0)
        return;

    uint8_t channel = status & 0x0f;
    switch (status >> 4) {
    case 0b1001:
        if (len < 3) break;
        if (msg[2] != 0) {
            adl_rt_noteOn(&player, channel, msg[1], msg[2]);
            break;
        }
    case 0b1000:
        if (len < 3) break;
        adl_rt_noteOff(&player, channel, msg[1]);
        break;
    case 0b1010:
        if (len < 3) break;
        adl_rt_noteAfterTouch(&player, channel, msg[1], msg[2]);
        break;
    case 0b1101:
        if (len < 2) break;
        adl_rt_channelAfterTouch(&player, channel, msg[1]);
        break;
    case 0b1011:
        if (len < 3) break;
        adl_rt_controllerChange(&player, channel, msg[1], msg[2]);
        break;
    case 0b1100:
        if (len < 2) break;
        adl_rt_patchChange(&player, channel, msg[1]);
        break;
    case 0b1110:
        if (len < 3) break;
        adl_rt_pitchBendML(&player, channel, msg[2], msg[1]);
        break;
    }
}

static int process(jack_nframes_t nframes, void *)
{
    ADL_MIDIPlayer &player = *::player;
    void *midi = jack_port_get_buffer(midiport, nframes);
    float *left = (float *)jack_port_get_buffer(outport[0], nframes);
    float *right = (float *)jack_port_get_buffer(outport[1], nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        jack_midi_event_t event;
        if (jack_midi_event_get(&event, midi, i) == 0)
            play_midi(event.buffer, event.size);
    }

    int16_t *pcm = ::buffer;
    adl_generate(&player, 2 * nframes, pcm);

    DcFilter &dclf = dcfilter[0];
    DcFilter &dcrf = dcfilter[1];

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        constexpr double outputgain = 1.0; // 3.5;
        left[i] = dclf.process(pcm[2 * i] * (outputgain / 32768));
        right[i] = dcrf.process(pcm[2 * i + 1] * (outputgain / 32768));
    }

    return 0;
}

static void usage()
{
    fprintf(stderr, "Usage: adljack [-n num-chips] [-b bank.wopl]\n");
}

int main(int argc, char *argv[])
{
    unsigned nchip = default_nchip;
    const char *bankfile = nullptr;

    for (int c; (c = getopt(argc, argv, "hn:b:")) != -1;) {
        switch (c) {
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

    buffer = new int16_t[2 * bufsize];

    fprintf(stderr, "DC filter @ %f Hz\n", dccutoff);
    dcfilter[0].cutoff(dccutoff / samplerate);
    dcfilter[1].cutoff(dccutoff / samplerate);

    fprintf(stderr, "ADLMIDI version %s\n", adl_linkedLibraryVersion());

    ADL_MIDIPlayer *player = ::player = adl_init(samplerate);
    if (!player)
        throw std::runtime_error("error instantiating ADLMIDI");

    if (!bankfile) {
        fprintf(stderr, "Using default banks.\n");
    }
    else {
        if (adl_openBankFile(player, bankfile) < 0)
            throw std::runtime_error("error loading bank file");
        fprintf(stderr, "Using banks from WOPL file.\n");
        adl_reset(player);  // not sure if necessary
    }

    if (adl_setNumChips(player, nchip) < 0)
        throw std::runtime_error("error setting the number of chips");

    jack_set_process_callback(client, process, nullptr);
    jack_activate(client);

    fprintf(stderr, "OPL3 ready with %u chips.\n", adl_getNumChips(player));

    //
    int p[2];
    if (pipe(p) == -1)
        throw std::system_error(errno, std::generic_category(), "pipe");
    for (char c;;)
        read(p[0], &c, 1);

    return 0;
}
