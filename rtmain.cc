//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "dcfilter.h"
#include <adlmidi.h>
#include <ring_buffer/ring_buffer.h>
#include <RtAudio.h>
#include <RtMidi.h>
#include <getopt.h>
#include <stdexcept>
#include <system_error>
#include <stdio.h>
#include <unistd.h>

static RtAudio *audio_client;
static RtMidiIn *midi_client;
static Ring_Buffer *midi_rb;
static ADL_MIDIPlayer *player;
static int16_t *buffer;
static DcFilter dcfilter[2];
static constexpr double dccutoff = 5.0;

static constexpr unsigned default_nchip = 4;
static constexpr unsigned midi_message_max_size = 64;
static constexpr unsigned midi_buffer_size = 1024;

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

static int process(void *outputbuffer, void *, unsigned nframes, double, RtAudioStreamStatus, void *)
{
    ADL_MIDIPlayer &player = *::player;

    Ring_Buffer &midi_rb = *::midi_rb;
    uint8_t evsize;
    uint8_t evdata[midi_message_max_size];
    while (midi_rb.peek(evsize) && 1 + evsize <= midi_rb.size_used()) {
        midi_rb.discard(1);
        midi_rb.get(evdata, evsize);
        play_midi(evdata, evsize);
    }

    int16_t *pcm = ::buffer;
    adl_generate(&player, 2 * nframes, pcm);

    DcFilter &dclf = dcfilter[0];
    DcFilter &dcrf = dcfilter[1];

    float *lr = (float *)outputbuffer;
    for (unsigned i = 0; i < 2 * nframes; ++i) {
        constexpr double outputgain = 1.0; // 3.5;
        DcFilter &dcf = (i & 1) ? dcrf : dclf;
        lr[i] = dcf.process(pcm[i] * (outputgain / 32768));
    }

    return 0;
}

static void midi_event(double, std::vector<uint8_t> *message, void *)
{
    size_t size = message->size();
    if (size > midi_message_max_size)
        return;
    Ring_Buffer &midi_rb = *::midi_rb;
    midi_rb.put<uint8_t>(size);
    midi_rb.put(message->data(), size);
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

    RtAudio *audio_client = ::audio_client = new RtAudio(RtAudio::Api::UNSPECIFIED);
    unsigned num_audio_devices = audio_client->getDeviceCount();
    if (num_audio_devices == 0) {
        fprintf(stderr, "no audio devices present for output\n");
        return 1;
    }

    unsigned output_device_id = audio_client->getDefaultOutputDevice();
    RtAudio::DeviceInfo device_info = audio_client->getDeviceInfo(output_device_id);
    unsigned sample_rate = device_info.preferredSampleRate;

    RtAudio::StreamParameters stream_param;
    stream_param.deviceId = output_device_id;
    stream_param.nChannels = 2;

    RtAudio::StreamOptions stream_opts;
    stream_opts.flags = RTAUDIO_MINIMIZE_LATENCY|RTAUDIO_ALSA_USE_DEFAULT;
    stream_opts.streamName = "adlrt";

    unsigned buffer_size = 0;
    audio_client->openStream(
        &stream_param, nullptr, RTAUDIO_FLOAT32, sample_rate, &buffer_size,
        &process, nullptr, &stream_opts);

    RtMidiIn *midi_client = ::midi_client = new RtMidiIn(
        RtMidi::Api::UNSPECIFIED, "adlrt", midi_buffer_size);
    midi_rb = new Ring_Buffer(midi_buffer_size);
    midi_client->openVirtualPort("midi");
    midi_client->setCallback(&midi_event);

    fprintf(stderr, "RtAudio client \"%s\" fs=%u bs=%u\n",
            device_info.name.c_str(), sample_rate, buffer_size);

    buffer = new int16_t[2 * buffer_size];

    fprintf(stderr, "DC filter @ %f Hz\n", dccutoff);
    dcfilter[0].cutoff(dccutoff / sample_rate);
    dcfilter[1].cutoff(dccutoff / sample_rate);

    fprintf(stderr, "ADLMIDI version %s\n", adl_linkedLibraryVersion());

    ADL_MIDIPlayer *player = ::player = adl_init(sample_rate);
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

    audio_client->startStream();

    fprintf(stderr, "OPL3 ready with %u chips.\n", adl_getNumChips(player));

    //
    int p[2];
    if (pipe(p) == -1)
        throw std::system_error(errno, std::generic_category(), "pipe");
    for (char c;;)
        read(p[0], &c, 1);

    return 0;
}
