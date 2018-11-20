//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "haikumain.h"
#include "insnames.h"
#include "i18n.h"
#include "common.h"
#include <stdio.h>

static std::string program_title = "ADLhaiku";

static double arg_latency = 20e-3;  // audio latency, 20ms default
static FILE *logstream = stderr;

struct Midi_Message_Header {
    uint8_t size;
    double timestamp;
};

static void play_buffer(void *cookie, void *buffer, size_t size, const media_raw_audio_format &format)
{
    Audio_Context &ctx = *(Audio_Context *)cookie;
    Ring_Buffer &midi_rb = *ctx.midi_rb;

    size_t nframes = size / (2 * sizeof(float));
    double fs = format.frame_rate;
    double ts = 1.0 / fs;
    double midi_delta = ctx.midi_delta;
    bool midi_stream_started = ctx.midi_stream_started;

    // maximum interval between midi processing cycles
    constexpr size_t midi_interval_max = 256;

    for (size_t iframe = 0; iframe != nframes;) {
        unsigned segment_nframes = std::min(nframes - iframe, midi_interval_max);

        if (midi_stream_started)
            midi_delta += segment_nframes * ts;
        else
            midi_delta = segment_nframes * ts;

        Midi_Message_Header hdr;
        uint8_t evdata[midi_message_max_size];
        while (midi_rb.peek(hdr) && sizeof(hdr) + hdr.size <= midi_rb.size_used()) {
            double timestamp = hdr.timestamp;
            if (!midi_stream_started) {
                timestamp = 0;
                midi_stream_started = true;
            }
            if (midi_delta < timestamp)
                break;  // not yet
            midi_delta -= timestamp;

            midi_rb.discard(sizeof(hdr));
            midi_rb.get(evdata, hdr.size);
            play_midi(evdata, hdr.size);
        }

        generate_outputs(
            (float *)buffer + 2 * iframe,
            (float *)buffer + 2 * iframe + 1,
            segment_nframes, 2);

        iframe += segment_nframes;
    }

    ctx.midi_delta = midi_delta;
    ctx.midi_stream_started = midi_stream_started;
}

static void generic_midi_event(const uint8_t *data, unsigned size, double timestamp, Audio_Context &ctx)
{
    if (size > midi_message_max_size) {
        ctx.midi_timestamp_accum += timestamp;
        return;
    }
    Ring_Buffer &midi_rb = *ctx.midi_rb;
    Midi_Message_Header hdr;
    hdr.size = size;
    hdr.timestamp = timestamp + ctx.midi_timestamp_accum;

    // wait for buffer space (this is non-RT!)
    while (midi_rb.size_free() < sizeof(hdr) + size) {
        // fprintf(logstream, "MIDI buffer full!\n");
        std::this_thread::sleep_for(stc::microseconds(100));
    }

    midi_rb.put(hdr);
    midi_rb.put(data, size);
    ctx.midi_timestamp_accum = 0;
}

class ADLSoundPlayer : public BSoundPlayer {
public:
    ADLSoundPlayer(Audio_Context &ctx, const media_raw_audio_format &fmt)
        : BSoundPlayer(&fmt, ::program_title.c_str(), &play_buffer, nullptr, &ctx) {}
};

class ADLMidiConsumer : public BMidiLocalConsumer {
public:
    explicit ADLMidiConsumer(Audio_Context &ctx)
        : BMidiLocalConsumer(::program_title.c_str()), ctx_(ctx) {}
    void Data(uchar *data, size_t length, bool atomic, bigtime_t time) override
    {
        double timestamp = (time - last_time_) * 1e-6;
        last_time_ = time;
        generic_midi_event(data, length, timestamp, ctx_);
    }
private:
    Audio_Context &ctx_;
    bigtime_t last_time_ = 0;
};

int audio_main()
{
    Audio_Context ctx;
    Ring_Buffer midi_rb(midi_buffer_size);
    ctx.midi_rb = &midi_rb;

    media_raw_audio_format sound_format;
    sound_format.frame_rate = 48000;
    sound_format.channel_count = 2;
    sound_format.format = media_raw_audio_format::B_AUDIO_FLOAT;
#if B_HOST_IS_LENDIAN
    sound_format.byte_order = B_MEDIA_LITTLE_ENDIAN;
#elif B_HOST_IS_BENDIAN
    sound_format.byte_order = B_MEDIA_BIG_ENDIAN;
#endif
    sound_format.buffer_size = ceil(arg_latency * sound_format.frame_rate);

    ADLSoundPlayer *sound_player = new ADLSoundPlayer(ctx, sound_format);
    if (status_t status = sound_player->InitCheck()) {
        fprintf(logstream, "Cannot create the sound player (status %ld)\n", (long)status);
        return 1;
    }

    ADLMidiConsumer *midi_consumer = new ADLMidiConsumer(ctx);
    if (!midi_consumer->IsValid()) {
        midi_consumer->Release();
        fprintf(logstream, "Cannot create the MIDI consumer\n");
        return 1;
    }

    if (!initialize_player(arg_player_type, sound_format.frame_rate, arg_nchip, arg_bankfile, arg_emulator))
        return 1;

    if (status_t status = sound_player->Start()) {
        fprintf(logstream, "Cannot start sound playback (status %ld)\n", (long)status);
        return 1;
    }
    if (status_t status = midi_consumer->Register()) {
        fprintf(logstream, "Cannot register MIDI consumer (status %ld)\n", (long)status);
        return 1;
    }
    player_ready();

    //
    interface_exec(nullptr, nullptr);

    //
    midi_consumer->Unregister();
    sound_player->Stop();

    //
    midi_consumer->Release();
    delete sound_player;

    return 0;
}

static void usage()
{
    std::string usage_extra;
    usage_extra += "\n          ";
    usage_extra += _("[-L latency-ms]");
    generic_usage("adlhaiku", usage_extra.c_str());
}

std::string get_program_title()
{
    return ::program_title;
}

int main(int argc, char *argv[])
{
    i18n_setup();
    midi_db.init();

    for (int c; (c = generic_getopt(argc, argv, "L:A:M:", usage)) != -1;) {
        switch (c) {
        case 'L': {
            double latency = ::arg_latency = std::stod(optarg) * 1e-3;
            if (latency <= 0) {
                fprintf(stderr, "%s\n", _("Invalid latency."));
                return 1;
            }
            break;
        }
        default:
            usage();
            return 1;
        }
    }

    if (argc != optind)
        return 1;

    if (0) {
        logstream = fopen("log.txt", "w");
        setvbuf(logstream, nullptr, _IOLBF, 0);
    }

    handle_signals();
    return audio_main();
}
