//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "rtmain.h"
#include "insnames.h"
#include "i18n.h"
#include "common.h"
#include "winmm_dialog.h"
#include <stdio.h>
#if defined(ADLJACK_GTK3)
#    include <gtk/gtk.h>
#endif
#if !defined(_WIN32)
#    include <syslog.h>
#endif

static std::string program_title = "ADLrt";

static double arg_latency = 20e-3;  // audio latency, 20ms default
static RtAudio::Api arg_audio_api;
static RtMidi::Api arg_midi_api;

#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
static bool vmidi_init();
static VM_MIDI_PORT_u vmidi_port_setup(Audio_Context &ctx, std::string &name);
#endif

struct Midi_Message_Header {
    uint8_t size;
    double timestamp;
};

static int process(void *outputbuffer, void *, unsigned nframes, double, RtAudioStreamStatus, void *user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;
    Ring_Buffer &midi_rb = *ctx.midi_rb;

    double fs = ctx.sample_rate;
    double ts = 1.0 / fs;
    double midi_delta = ctx.midi_delta;
    bool midi_stream_started = ctx.midi_stream_started;

    // maximum interval between midi processing cycles
    constexpr unsigned midi_interval_max = 256;

    for (unsigned iframe = 0; iframe != nframes;) {
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
            (float *)outputbuffer + 2 * iframe,
            (float *)outputbuffer + 2 * iframe + 1,
            segment_nframes, 2);

        iframe += segment_nframes;
    }

    ctx.midi_delta = midi_delta;
    ctx.midi_stream_started = midi_stream_started;
    return 0;
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

    bool wait_for_buffer_space =
        ctx.midi_client->getCurrentApi() != RtMidi::UNIX_JACK;

    if (wait_for_buffer_space) {
        // wait for buffer space (this is non-RT!)
        while (midi_rb.size_free() < sizeof(hdr) + size) {
            // fprintf(stderr, "MIDI buffer full!\n");
            std::this_thread::sleep_for(stc::microseconds(100));
        }
    }
    else {
        // drop
        if (midi_rb.size_free() < sizeof(hdr) + size) {
            ctx.midi_timestamp_accum += timestamp;
            return;
        }
    }

    midi_rb.put(hdr);
    midi_rb.put(data, size);
    ctx.midi_timestamp_accum = 0;
}

static void rtmidi_event(double timestamp, std::vector<uint8_t> *message, void *user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;
    generic_midi_event(message->data(), message->size(), timestamp, ctx);
}

void audio_error_callback(RtAudioError::Type type, const std::string &text)
{
    if (type == RtAudioError::WARNING) {
        debug_printf("%s", text.c_str());
        return;
    }
    throw RtAudioError(text, type);
}

void midi_error_callback(RtMidiError::Type type, const std::string &text, void *)
{
    if (type == RtMidiError::WARNING) {
        debug_printf("%s", text.c_str());
        return;
    }
    throw RtMidiError(text, type);
}

int audio_main()
{
    Audio_Context ctx;
    Ring_Buffer midi_rb(midi_buffer_size);
    ctx.midi_rb = &midi_rb;

    RtAudio audio_client(::arg_audio_api);
    ctx.audio_client = &audio_client;
    RtMidiIn midi_client(::arg_midi_api, "ADLrt", midi_buffer_size);
    ctx.midi_client = &midi_client;

    unsigned num_audio_devices = audio_client.getDeviceCount();
    if (num_audio_devices == 0) {
        fprintf(stderr, "%s\n", _("No audio devices are present for output."));
        return 1;
    }

    unsigned output_device_id = audio_client.getDefaultOutputDevice();
    RtAudio::DeviceInfo device_info = audio_client.getDeviceInfo(output_device_id);
    unsigned sample_rate = device_info.preferredSampleRate;
    ctx.sample_rate = sample_rate;

    RtAudio::StreamParameters stream_param;
    stream_param.deviceId = output_device_id;
    stream_param.nChannels = 2;

    RtAudio::StreamOptions stream_opts;
    stream_opts.flags = RTAUDIO_ALSA_USE_DEFAULT;
    if (!arg_autoconnect)
        stream_opts.flags |= RTAUDIO_JACK_DONT_CONNECT;
    stream_opts.streamName = "ADLrt";

    double latency = ::arg_latency;
    unsigned buffer_size = ceil(latency * sample_rate);
    fprintf(stderr, _("Desired latency %f ms = buffer size %u\n"),
            latency * 1e3, buffer_size);

    audio_client.openStream(
        &stream_param, nullptr, RTAUDIO_FLOAT32, sample_rate, &buffer_size,
        &process, &ctx, &stream_opts, &audio_error_callback);

    midi_client.setCallback(&rtmidi_event, &ctx);
    midi_client.setErrorCallback(&midi_error_callback);

#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
    VM_MIDI_PORT_u vmidi_port;
    ctx.have_virtualmidi = vmidi_init();
#endif

    std::string midi_port_name;

    switch (midi_client.getCurrentApi()) {
    default:
        midi_port_name = "ADLrt MIDI";
        midi_client.openVirtualPort(midi_port_name.c_str());
        break;
#if defined(_WIN32)
    case RtMidi::WINDOWS_MM: {
        switch(int port = dlg_select_midi_port(ctx)) {
        default:
            midi_client.openPort(port, "ADLrt MIDI");
            midi_port_name = midi_client.getPortName(port);
            break;
        case -1:
            return 1;
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
        case -2:
            vmidi_port = vmidi_port_setup(ctx, midi_port_name);
            if (!vmidi_port)
                return 1;
            ctx.vmidi_port = vmidi_port.get();
            break;
#endif
        }
        break;
    }
#endif
    }

    ::program_title = std::string("ADLrt") + " [" + midi_port_name + "]";

    latency = buffer_size / (double)sample_rate;
    fprintf(stderr, _("RtAudio client \"%s\" fs=%u bs=%u latency=%f\n"),
            device_info.name.c_str(), sample_rate, buffer_size, latency);

    if (!initialize_player(arg_player_type, sample_rate, arg_nchip, arg_bankfile, arg_emulator))
        return 1;

    audio_client.startStream();
    player_ready();

    //
    interface_exec(nullptr, nullptr);

    //
    audio_client.stopStream();
    midi_client.closePort();
#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
    vmidi_port.reset();
#endif

    return 0;
}

static void usage()
{
    std::string audio_apis_str;
    std::string midi_apis_str;

    std::vector<RtAudio::Api> audio_apis;
    std::vector<RtMidi::Api> midi_apis;
    RtAudio::getCompiledApi(audio_apis);
    RtMidi::getCompiledApi(midi_apis);

    for (RtAudio::Api api : audio_apis) {
        if (const char *id = audio_api_id(api)) {
            if (!audio_apis_str.empty()) audio_apis_str.append(", ");
            audio_apis_str.append(id);
        }
    }
    for (RtMidi::Api api : midi_apis) {
        if (const char *id = midi_api_id(api)) {
            if (!midi_apis_str.empty()) midi_apis_str.append(", ");
            midi_apis_str.append(id);
        }
    }

    std::string usage_extra;
    usage_extra += "\n          ";
    usage_extra += _("[-L latency-ms]");

    usage_extra += "\n          ";
    usage_extra += _("[-A audio-system]");
    usage_extra +=  ": ";
    usage_extra += audio_apis_str;

    usage_extra += "\n          ";
    usage_extra += _("[-M midi-system]");
    usage_extra +=  ": ";
    usage_extra += midi_apis_str;

    generic_usage("adlrt", usage_extra.c_str());
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
        case 'A': {
            RtAudio::Api audio_api = ::arg_audio_api = find_audio_api(optarg);
            if (!is_compiled_audio_api(audio_api)) {
                fprintf(stderr, _("Invalid audio system '%s'.\n"), optarg);
                return 1;
            }
            break;
        }
        case 'M': {
            RtMidi::Api midi_api = ::arg_midi_api = find_midi_api(optarg);
            if (!is_compiled_midi_api(midi_api)) {
                fprintf(stderr, _("Invalid MIDI system '%s'.\n"), optarg);
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

#ifdef ADLJACK_GTK3
    gtk_init(&argc, &argv);
#endif

#if !defined(_WIN32)
    openlog("ADLrt", 0, LOG_USER);
#endif

    handle_signals();
    return audio_main();
}

#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
static bool vmidi_init()
{
    bool have = virtualMIDI::load();
    fprintf(stderr, "virtualMIDI %ls\n", have ?
            virtualMIDI::GetVersion(nullptr, nullptr, nullptr, nullptr) : L"not found");
    if (have) {
        const WCHAR *driver_version = virtualMIDI::GetDriverVersion(nullptr, nullptr, nullptr, nullptr);
        have = driver_version && *driver_version;
        fprintf(stderr, "virtualMIDI driver %ls\n", have ? driver_version : L"not found");
    }
    return have;
}

static void CALLBACK vmidi_event(VM_MIDI_PORT *, BYTE *bytes, DWORD length, DWORD_PTR user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;

    double timestamp;
    stc::steady_clock::time_point now = stc::steady_clock::now();
    if (!ctx.vmidi_have_last_event) {
        timestamp = 0;
        ctx.vmidi_have_last_event = true;
    }
    else {
        stc::steady_clock::duration d = now - ctx.vmidi_last_event_time;
        timestamp = stc::duration_cast<stc::duration<double>>(d).count();
    }
    ctx.vmidi_last_event_time = now;

    generic_midi_event(bytes, length, timestamp, ctx);
}

static VM_MIDI_PORT_u vmidi_port_setup(Audio_Context &ctx, std::string &name)
{
    VM_MIDI_PORT_u port;
    DWORD err;
    unsigned nth = 0;
    std::wstring nametmp;
    do {
        if (nth == 0)
            nametmp = L"ADLrt MIDI";
        else
            nametmp = L"ADLrt-" + std::to_wstring(nth) + L" MIDI";
        ++nth;
        port.reset(virtualMIDI::CreatePortEx2(
                       nametmp.c_str(), &vmidi_event, (DWORD_PTR)&ctx, 256,
                       TE_VM_FLAGS_PARSE_RX|TE_VM_FLAGS_INSTANTIATE_RX_ONLY));
        if (!port)
            err = GetLastError();
    }
    while (!port && (err == ERROR_ALREADY_EXISTS || err == ERROR_ALIAS_EXISTS));

    if (!port) {
        fprintf(stderr, "Could not create virtualMIDI port.\n");
        return {};
    }

    name.assign(nametmp.begin(), nametmp.end());
    return port;
}
#endif
