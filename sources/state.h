//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_NSM)
#include <vector>
#include <stdint.h>

bool save_state(std::vector<uint8_t> &data);
bool load_state(const std::vector<uint8_t> &data);

#endif  // defined(ADLJACK_USE_NSM)
