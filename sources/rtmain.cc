//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "rtmain.h"
#include "common.h"
#include "winmm_dialog.h"
#include <stdio.h>
#if !defined(_WIN32)
#    include <syslog.h>
#endif

static std::string program_title = "ADLrt";

static double arg_latency = 20e-3;  // audio latency, 20ms default

#if defined(ADLJACK_ENABLE_VIRTUALMIDI)
static bool vmidi_init();
static VM_MIDI_PORT_u vmidi_port_setup(Audio_Context &ctx, std::string &name);
#endif

static int process(void *outputbuffer, void *, unsigned nframes, double, RtAudioStreamStatus, void *user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;
    Ring_Buffer &midi_rb = *ctx.midi_rb;
    uint8_t evsize;
    uint8_t evdata[midi_message_max_size];
    while (midi_rb.peek(evsize) && 1u + evsize <= midi_rb.size_used()) {
        midi_rb.discard(1);
        midi_rb.get(evdata, evsize);
        play_midi(evdata, evsize);
    }
    generate_outputs((float *)outputbuffer, (float *)outputbuffer + 1, nframes, 2);
    return 0;
}

static void generic_midi_event(const uint8_t *data, unsigned size, Audio_Context &ctx)
{
    if (size > midi_message_max_size)
        return;
    Ring_Buffer &midi_rb = *ctx.midi_rb;
    if (midi_rb.size_free() >= 1 + size) {
        midi_rb.put<uint8_t>(size);
        midi_rb.put(data, size);
    }
}

static void rtmidi_event(double, std::vector<uint8_t> *message, void *user_data)
{
    Audio_Context &ctx = *(Audio_Context *)user_data;
    generic_midi_event(message->data(), message->size(), ctx);
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

    RtAudio audio_client(RtAudio::Api::UNSPECIFIED);
    ctx.audio_client = &audio_client;
    RtMidiIn midi_client(RtMidi::Api::UNSPECIFIED, "ADLrt", midi_buffer_size);
    ctx.midi_client = &midi_client;

    unsigned num_audio_devices = audio_client.getDeviceCount();
    if (num_audio_devices == 0) {
        fprintf(stderr, "No audio devices are present for output.\n");
        return 1;
    }

    unsigned output_device_id = audio_client.getDefaultOutputDevice();
    RtAudio::DeviceInfo device_info = audio_client.getDeviceInfo(output_device_id);
    unsigned sample_rate = device_info.preferredSampleRate;

    RtAudio::StreamParameters stream_param;
    stream_param.deviceId = output_device_id;
    stream_param.nChannels = 2;

    RtAudio::StreamOptions stream_opts;
    stream_opts.flags = RTAUDIO_ALSA_USE_DEFAULT;
    stream_opts.streamName = "ADLrt";

    double latency = ::arg_latency;
    unsigned buffer_size = ceil(latency * sample_rate);
    fprintf(stderr, "Desired latency %f ms = buffer size %u\n",
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
    fprintf(stderr, "RtAudio client \"%s\" fs=%u bs=%u latency=%f\n",
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
    generic_usage("adlrt", " [-L latency-ms]");
}

std::string get_program_title()
{
    return ::program_title;
}

int main(int argc, char *argv[])
{
    for (int c; (c = generic_getopt(argc, argv, "L:", usage)) != -1;) {
        switch (c) {
        case 'L': {
            double latency = ::arg_latency = std::stod(optarg) * 1e-3;
            if (latency <= 0) {
                fprintf(stderr, "Invalid latency.\n");
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
    generic_midi_event(bytes, length, ctx);
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
