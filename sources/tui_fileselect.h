//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#if defined(ADLJACK_USE_CURSES)
#include <curses.h>
#include <string>

struct File_Selection_Options {
    std::string directory;
    std::string filepath;
    std::string title;
    bool show_hidden_files = false;
};

enum class File_Selection_Code {
    Ok,
    Cancel,
    Break,
};

File_Selection_Code fileselect(WINDOW *w, File_Selection_Options &opts);

#endif
