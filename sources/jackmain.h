//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#include <jack/jack.h>
#include <jack/midiport.h>
#if defined(ADLJACK_USE_NSM)
#    include <nsm.h>
#endif
#include <memory>

struct Jack_Deleter {
    void operator()(jack_client_t *x)
        { jack_client_close(x); }
};
typedef std::unique_ptr<jack_client_t, Jack_Deleter> jack_client_u;

struct Audio_Context {
    jack_client_u client;
    jack_port_t *midiport = nullptr;
    jack_port_t *outport[2] = {};
#if defined(ADLJACK_USE_NSM)
    nsm_client_t *nsm = nullptr;
#endif
};

#if defined(ADLJACK_USE_NSM)
struct Nsm_Deleter {
    void operator()(nsm_client_t *x) { nsm_free(x); }
};
typedef std::unique_ptr<nsm_client_t, Nsm_Deleter> nsm_client_u;
#endif

enum { session_size_max = 1024 };
