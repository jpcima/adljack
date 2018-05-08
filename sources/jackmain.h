//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <jack/jack.h>
#include <jack/midiport.h>
#include <memory>

struct Audio_Context {
    jack_client_t *client = nullptr;
    jack_port_t *midiport = nullptr;
    jack_port_t *outport[2] = {};
};

struct Jack_Deleter {
    void operator()(jack_client_t *x)
        { jack_client_close(x); }
};
typedef std::unique_ptr<jack_client_t, Jack_Deleter> jack_client_u;
