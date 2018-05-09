//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_NSM)
#include "state.h"
#include "state_generated.h"
#include "common.h"

bool save_state(std::vector<uint8_t> &data)
{
    if (!have_active_player())
        return false;

    using namespace fb::state;
    flatbuffers::FlatBufferBuilder builder(1024);

    std::vector<flatbuffers::Offset<Channel_State>> channel_vector;
    channel_vector.reserve(16);
    for (unsigned i = 0; i < 16; ++i) {
        const Program &program = ::channel_map[i];
        auto channel = CreateChannel_State(
            builder, program.gm, (program.bank_msb << 7) | program.bank_lsb);
        channel_vector.push_back(channel);
    }

    std::vector<flatbuffers::Offset<Player_State>> player_vector;
    player_vector.reserve(player_type_count);
    for (unsigned i = 0; i < player_type_count; ++i) {
        Player_Type pt = (Player_Type)i;
        Player &pl = *::player[i];
        auto player = CreatePlayer_State(
            builder,
            CreatePlayer_Id(
                builder,
                builder.CreateString(Player::name(pt)),
                builder.CreateString(pl.emulator_name())),
            builder.CreateString(::player_bank_file[i]));
        player_vector.push_back(player);
    }

    Emulator_Id active_id = ::emulator_ids[::active_emulator_id];
    Player &active_player = *::player[(unsigned)active_id.player];

    auto state = CreateState(
        builder,
        builder.CreateVector(channel_vector),
        builder.CreateVector(player_vector),
        active_player.chip_count(),
        ::player_volume,
        CreatePlayer_Id(
            builder,
            builder.CreateString(Player::name(active_id.player)),
            builder.CreateString(active_player.emulator_name())));
    builder.Finish(state);

    const uint8_t *buffer = builder.GetBufferPointer();
    size_t size = builder.GetSize();
    data.assign(buffer, buffer + size);
    return true;
}

bool load_state(const std::vector<uint8_t> &data)
{
    using namespace fb::state;

    flatbuffers::Verifier verifier(data.data(), data.size());
    if (!VerifyStateBuffer(verifier))
        return false;

    if (!have_active_player())
        return false;
    auto lock = active_player().take_lock();

    bool success = true;

    auto state = GetState(data.data());

    if (state->channel()->size() != 16)
        return false;

    unsigned chip_count = state->chip_count();
    if (chip_count <= 0) {
        chip_count = 1;
        success = false;
    }

    int volume = state->volume();
    if (volume < volume_min || volume > volume_max) {
        volume = 100;
        success = false;
    }
    ::player_volume = volume;

    for (unsigned i = 0; i < 16; ++i) {
        const auto *channel = state->channel()->Get(i);
        Program program;
        program.gm = channel->program() & 0x7f;
        unsigned bank = channel->bank();
        program.bank_lsb = bank & 0x7f;
        program.bank_msb = (bank >> 7) & 0x7f;
        for (unsigned pt = 0; pt < player_type_count; ++pt) {
            Player &pl = *::player[(unsigned)pt];
            pl.rt_bank_change_msb(i, program.bank_msb);
            pl.rt_bank_change_lsb(i, program.bank_lsb);
            pl.rt_program_change(i, program.gm);
        }
        ::channel_map[i] = program;
    }

    for (const auto *player : *state->player()) {
        Player_Type pt = Player::type_by_name(player->id()->player()->c_str());
        if (pt == (Player_Type)-1) {
            success = false;
            continue;
        }
        Player &pl = *::player[(unsigned)pt];
        if (!pl.set_chip_count(chip_count))
            success = false;
        if (const auto *bank_file = player->bank_file()) {
            if (bank_file->size() > 0) {
                if (!pl.load_bank_file(bank_file->c_str()))
                    success = false;
                else
                    ::player_bank_file[(unsigned)pt] = bank_file->str();
            }
            else {
#pragma message("TODO state: load the default bank")
                
                ::player_bank_file[(unsigned)pt] = bank_file->str();
            }
        }
        unsigned emu = Player::emulator_by_name(pt, player->id()->emulator()->c_str());
        if (emu == (unsigned)-1) {
            success = false;
            continue;
        }
        if (!pl.set_emulator(emu))
            success = false;
    }

    Emulator_Id active_id;
    active_id.player = Player::type_by_name(state->active_id()->player()->c_str());
    if (active_id.player != (Player_Type)-1)
        active_id.emulator = Player::emulator_by_name(active_id.player, state->active_id()->emulator()->c_str());

    auto pos = std::find(::emulator_ids.begin(), ::emulator_ids.end(), active_id);
    if (pos == ::emulator_ids.end())
        success = false;
    else
        ::active_emulator_id = std::distance(::emulator_ids.begin(), pos);

    return success;
}

#endif  // defined(ADLJACK_USE_NSM)
