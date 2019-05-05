//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "player.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

Player *Player::create(Player_Type pt, unsigned sample_rate)
{
    std::unique_ptr<Player> instance;
    switch (pt) {
    default: assert(false); abort();
    #define PLAYER_CASE(x)                                                  \
        case Player_Type::x: instance.reset(new Generic_Player<Player_Type::x>); break;
    EACH_PLAYER_TYPE(PLAYER_CASE);
    #undef PLAYER_CASE
    }
    if (!instance->init(sample_rate))
        return nullptr;
    return instance.release();
}

const char *Player::name(Player_Type pt)
{
    switch (pt) {
    default: assert(false); abort();
    #define PLAYER_CASE(x)                                                  \
        case Player_Type::x: return Player_Traits<Player_Type::x>::name();
    EACH_PLAYER_TYPE(PLAYER_CASE);
    #undef PLAYER_CASE
    }
}

const char *Player::version(Player_Type pt)
{
    switch (pt) {
    default: assert(false); abort();
    #define PLAYER_CASE(x)                                                  \
        case Player_Type::x: return Player_Traits<Player_Type::x>::version();
    EACH_PLAYER_TYPE(PLAYER_CASE);
    #undef PLAYER_CASE
    }
}

const char *Player::chip_name(Player_Type pt)
{
    switch (pt) {
    default: assert(false); abort();
    #define PLAYER_CASE(x)                                                  \
        case Player_Type::x: return Player_Traits<Player_Type::x>::chip_name();
    EACH_PLAYER_TYPE(PLAYER_CASE);
    #undef PLAYER_CASE
    }
}

double Player::output_gain(Player_Type pt)
{
    switch (pt) {
    default: assert(false); abort();
    #define PLAYER_CASE(x)                                                  \
        case Player_Type::x: return Player_Traits<Player_Type::x>::output_gain;
    EACH_PLAYER_TYPE(PLAYER_CASE);
    #undef PLAYER_CASE
    }
}

auto Player::enumerate_emulators(Player_Type pt) -> std::vector<Emulator>
{
    std::vector<Emulator> emus;
    emus.reserve(32);

    std::unique_ptr<Player> player(create(pt, 44100));
    for (unsigned i = 0; i < 32; ++i) {
        if (player->set_emulator(i)) {
            Emulator emu;
            emu.id = i;
            emu.name = player->emulator_name();
            emus.push_back(emu);
        }
    }

    return emus;
}

unsigned Player::emulator_by_name(Player_Type pt, const char *name)
{
    std::vector<Emulator> emus = enumerate_emulators(pt);
    for (unsigned i = 0, n = emus.size(); i < n; ++i)
        if (!strcmp(emus[i].name, name))
            return i;
    return (unsigned)-1;
}

Player_Type Player::type_by_name(const char *nam)
{
    for (Player_Type pt : all_player_types)
        if (!strcmp(nam, name(pt)))
            return pt;
    return (Player_Type)-1;
}

bool Player::dynamic_set_chip_count(unsigned nchip)
{
    auto lock = take_lock();
    panic();
    return set_chip_count(nchip);
}

bool Player::dynamic_set_emulator(unsigned emulator)
{
    auto lock = take_lock();
    panic();
    return set_emulator(emulator);
}

bool Player::dynamic_load_bank(const char *bankfile)
{
    auto lock = take_lock();
    panic();
    if (!load_bank_file(bankfile))
        return false;
    return true;
}

void Player::dynamic_panic()
{
    auto lock = take_lock();
    panic();
}
