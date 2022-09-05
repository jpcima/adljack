//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#if defined(ADLJACK_USE_CURSES)
#include <curses.h>
#include <memory>

class Player;

class Channel_Monitor {
public:
    Channel_Monitor();
    ~Channel_Monitor();
    void setup_display(WINDOW *outer);
    void setup_player(Player *player);
    void update(char *data, unsigned size, unsigned serial);
    int key(int key);
private:
    struct Impl;
    std::unique_ptr<Impl> P;
};

#endif
