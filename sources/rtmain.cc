//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"
#include <ring_buffer/ring_buffer.h>
#include <RtAudio.h>
#include <RtMidi.h>
#include <stdexcept>
#include <system_error>
#include <stdio.h>
#if defined(_WIN32)
#    include "win_resource.h"
#    include <windows.h>
static INT_PTR CALLBACK winmm_dlgproc(HWND hdlg, unsigned msg, WPARAM wp, LPARAM lp);
#endif

static RtAudio *audio_client;
static RtAudio::DeviceInfo audio_device_info;
static RtMidiIn *midi_client;
static std::string midi_port_name;
static Ring_Buffer *midi_rb;

static int process(void *outputbuffer, void *, unsigned nframes, double, RtAudioStreamStatus, void *)
{
    Ring_Buffer &midi_rb = *::midi_rb;
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

void audio_error_callback(RtAudioError::Type type, const std::string &text)
{
    if (type == RtAudioError::WARNING) {
#if defined(_WIN32)
        OutputDebugStringA(text.c_str());
#else
        // ignore, don't print anything
#endif
        return;
    }
    throw RtAudioError(text, type);
}

void midi_error_callback(RtMidiError::Type type, const std::string &text, void *)
{
    if (type == RtMidiError::WARNING) {
#if defined(_WIN32)
        OutputDebugStringA(text.c_str());
#else
        // ignore, don't print anything
#endif
        return;
    }
    throw RtMidiError(text, type);
}

static void usage()
{
    generic_usage("adlrt", " [-L latency-ms]");
}

std::string get_program_title()
{
    std::string name = "ADLrt";
    const std::string &port_name = ::midi_port_name;
    if (!port_name.empty()) {
        name.push_back(' ');
        name.push_back('[');
        name.append(port_name);
        name.push_back(']');
    }
    return name;
}

int main(int argc, char *argv[])
{
    double latency = 20e-3;  // audio latency, 20ms default

    for (int c; (c = generic_getopt(argc, argv, "L:", usage)) != -1;) {
        switch (c) {
        case 'L':
            latency = std::stod(optarg) * 1e-3;
            if (latency <= 0) {
                fprintf(stderr, "invalid latency\n");
                return 1;
            }
            break;
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
    stream_opts.streamName = "ADLrt";

    unsigned buffer_size = ceil(latency * sample_rate);
    fprintf(stderr, "Desired latency %f ms = buffer size %u\n",
            latency * 1e3, buffer_size);

    audio_client->openStream(
        &stream_param, nullptr, RTAUDIO_FLOAT32, sample_rate, &buffer_size,
        &process, nullptr, &stream_opts, &audio_error_callback);

    ::audio_device_info = device_info;

    RtMidiIn *midi_client = ::midi_client = new RtMidiIn(
        RtMidi::Api::UNSPECIFIED, "ADLrt", midi_buffer_size);
    midi_client->setErrorCallback(&midi_error_callback);

    midi_rb = new Ring_Buffer(midi_buffer_size);

#if defined(_WIN32)
    if (midi_client->getCurrentApi() != RtMidi::WINDOWS_MM) {
#endif
        const char *vport_name = "ADLrt MIDI";
        midi_client->openVirtualPort(vport_name);
        ::midi_port_name = vport_name;
#if defined(_WIN32)
    }
    else {
        INT_PTR port = DialogBox(nullptr, MAKEINTRESOURCE(IDD_DIALOG1), nullptr, &winmm_dlgproc);
        if (port >= 0) {
            midi_client->openPort(port, "ADLrt MIDI");
            ::midi_port_name = midi_client->getPortName(port);
        }
        else
            return 1;
    }
#endif

    midi_client->setCallback(&midi_event);

    latency = buffer_size / (double)sample_rate;
    fprintf(stderr, "RtAudio client \"%s\" fs=%u bs=%u latency=%f\n",
            device_info.name.c_str(), sample_rate, buffer_size, latency);

    initialize_player(arg_player_type, sample_rate, arg_nchip, arg_bankfile, arg_emulator);

    audio_client->startStream();
    player_ready();

    //
    interface_exec();

    return 0;
}

#if defined(_WIN32)
static INT_PTR CALLBACK winmm_dlgproc(HWND hdlg, unsigned msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        RtMidiIn *midi_client = ::midi_client;
        unsigned nports = midi_client->getPortCount();
        HWND hchoice = GetDlgItem(hdlg, IDC_CHOICE);
        for (unsigned i = 0; i < nports; ++i) {
            std::string name = midi_client->getPortName(i);
            SendMessageA(hchoice, CB_ADDSTRING, 0, (LPARAM)name.c_str());
        }
        SendMessage(hchoice, CB_SETCURSEL, 0, 0);
        SetFocus(hchoice);
        return 0;
    }
    case WM_COMMAND: {
        switch (wp) {
            case IDOK: {
                HWND hchoice = GetDlgItem(hdlg, IDC_CHOICE);
                EndDialog(hdlg, SendMessage(hchoice, CB_GETCURSEL, 0, 0));
                break;
            }
            case IDCANCEL:
                EndDialog(hdlg, -1);
                break;
        }
    }
    }
    return FALSE;
}
#endif
