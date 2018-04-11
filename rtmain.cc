//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"
#include <ring_buffer/ring_buffer.h>
#include <RtAudio.h>
#include <RtMidi.h>
#include <getopt.h>
#include <stdexcept>
#include <system_error>
#include <stdio.h>

static RtAudio *audio_client;
static RtMidiIn *midi_client;
static Ring_Buffer *midi_rb;

static int process(void *outputbuffer, void *, unsigned nframes, double, RtAudioStreamStatus, void *)
{
    Ring_Buffer &midi_rb = *::midi_rb;
    uint8_t evsize;
    uint8_t evdata[midi_message_max_size];
    while (midi_rb.peek(evsize) && 1 + evsize <= midi_rb.size_used()) {
        midi_rb.discard(1);
        midi_rb.get(evdata, evsize);
        play_midi(evdata, evsize);
    }

    generate_outputs((float *)outputbuffer, (float *)outputbuffer + 1, nframes, 2);
    return 0;
}

static void midi_event(double, std::vector<uint8_t> *message, void *)
{
    size_t size = message->size();
    if (size > midi_message_max_size)
        return;
    Ring_Buffer &midi_rb = *::midi_rb;
    if (midi_rb.size_free() >= 1 + size) {
        midi_rb.put<uint8_t>(size);
        midi_rb.put(message->data(), size);
    }
}

static void usage()
{
    generic_usage("adlrt", " [-L latency-ms]");
}

int main(int argc, char *argv[])
{
    unsigned nchip = default_nchip;
    const char *bankfile = nullptr;
    double latency = 20e-3;  // audio latency, 20ms default
    int emulator = -1;

    for (int c; (c = getopt(argc, argv, "hp:n:b:e:L:")) != -1;) {
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
        case 'L':
            latency = std::stod(optarg) * 1e-3;
            if (latency <= 0) {
                fprintf(stderr, "invalid latency\n");
                return 1;
            }
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
    stream_opts.flags = RTAUDIO_ALSA_USE_DEFAULT;
    stream_opts.streamName = "adlrt";

    unsigned buffer_size = ceil(latency * sample_rate);
    fprintf(stderr, "Desired latency %f ms = buffer size %u\n",
            latency * 1e3, buffer_size);

    audio_client->openStream(
        &stream_param, nullptr, RTAUDIO_FLOAT32, sample_rate, &buffer_size,
        &process, nullptr, &stream_opts);

    RtMidiIn *midi_client = ::midi_client = new RtMidiIn(
        RtMidi::Api::UNSPECIFIED, "adlrt", midi_buffer_size);
    midi_rb = new Ring_Buffer(midi_buffer_size);
    midi_client->openVirtualPort("midi");
    midi_client->setCallback(&midi_event);

    latency = buffer_size / (double)sample_rate;
    fprintf(stderr, "RtAudio client \"%s\" fs=%u bs=%u latency=%f\n",
            device_info.name.c_str(), sample_rate, buffer_size, latency);

    buffer = new int16_t[2 * buffer_size];

    initialize_player(sample_rate, nchip, bankfile, emulator);

    audio_client->startStream();
    player_ready();

    //
    interface_exec();

    return 0;
}
