//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "common.h"

ADL_MIDIPlayer *player;
int16_t *buffer;
DcFilter dcfilter[2];

void play_midi(const uint8_t *msg, unsigned len)
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

void generate_outputs(float *left, float *right, unsigned nframes, unsigned stride)
{
    ADL_MIDIPlayer &player = *::player;

    int16_t *pcm = ::buffer;
    adl_generate(&player, 2 * nframes, pcm);

    DcFilter &dclf = dcfilter[0];
    DcFilter &dcrf = dcfilter[1];

    for (unsigned i = 0; i < nframes; ++i) {
        constexpr double outputgain = 1.0; // 3.5;
        left[i * stride] = dclf.process(pcm[2 * i] * (outputgain / 32768));
        right[i * stride] = dcrf.process(pcm[2 * i + 1] * (outputgain / 32768));
    }
}
