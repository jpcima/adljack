//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_CURSES)
#include "tui_fileselect.h"
#include "tui.h"
#include <algorithm>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

struct DIR_deleter { void operator()(DIR *x) { closedir(x); } };
typedef std::unique_ptr<DIR, DIR_deleter> DIR_u;

enum File_Type {
    Regular,
    Directory,
};

struct File_Entry {
    File_Type type;
    std::string name;
};

bool operator<(const File_Entry &a, const File_Entry &b)
{
    if (a.name == "..")
        return true;
    if (b.name == "..")
        return false;
    if (a.type != b.type) {
        if (a.type == File_Type::Directory)
            return true;
        if (b.type == File_Type::Directory)
            return false;
    }
    return a.name < b.name;
}

struct File_Selection_Context {
    WINDOW *win = nullptr;
    File_Selection_Options *opts = nullptr;
    //
    WINDOW_u win_inner;
    std::vector<File_Entry> file_list;
    unsigned file_selection = 0;
};

static void setup_display(File_Selection_Context &ctx);
static void update_display(File_Selection_Context &ctx);
static void update_file_list(File_Selection_Context &ctx);
static std::string visit_file(File_Selection_Context &ctx);

File_Selection_Code fileselect(WINDOW *w, File_Selection_Options &opts)
{
    File_Selection_Context ctx;
    ctx.win = w;
    ctx.opts = &opts;
    ctx.file_list.reserve(256);

    if (opts.directory.empty())
        opts.directory = '/';
    update_file_list(ctx);

    setup_display(ctx);

    bool quit = false;
    while (!quit) {
#if defined(PDCURSES)
        bool resized = is_termresized();
#else
        bool resized = is_term_resized(LINES, COLS);
#endif
        if (resized)
            return File_Selection_Code::Cancel;  // XXX close and let caller handle resize

        update_display(ctx);

        int ch = getch();
        switch (ch) {
        case 'q':
        case 'Q':
        case 3:   // console break
            return File_Selection_Code::Break;
        case KEY_DOWN:
            if (ctx.file_selection + 1 < ctx.file_list.size())
                ++ctx.file_selection;
            break;
        case KEY_UP:
            if (ctx.file_selection > 0)
                --ctx.file_selection;
            break;
        case KEY_NPAGE: {
            unsigned count = std::min<unsigned>
                (10, ctx.file_list.size() - ctx.file_selection - 1);
            ctx.file_selection += count;
            break;
        }
        case KEY_PPAGE: {
            unsigned count = std::min<unsigned>(10, ctx.file_selection);
            ctx.file_selection -= count;
            break;
        }
        case '\r':
        case '\n': {
            std::string path = visit_file(ctx);
            if (!path.empty()) {
                opts.filepath = path;
                return File_Selection_Code::Ok;
            }
            break;
        }
        case 27:  // escape
            quit = true;
            break;
        }
    }

    return File_Selection_Code::Cancel;
}

static void setup_display(File_Selection_Context &ctx)
{
    WINDOW *inner = subwin(stdscr, LINES - 2, COLS - 2, 1, 1);
    if (!inner) return;
    ctx.win_inner.reset(inner);
}

static void update_display(File_Selection_Context &ctx)
{
    WINDOW *inner = ctx.win_inner.get();
    File_Selection_Options &opts = *ctx.opts;

    {
        size_t titlesize = opts.title.size();

        attron(A_BOLD|COLOR_PAIR(Colors_Frame));
        border(' ', ' ', '-', ' ', '-', '-', ' ', ' ');
        attroff(A_BOLD|COLOR_PAIR(Colors_Frame));

        unsigned cols = getcols(stdscr);
        if (cols >= titlesize + 2) {
            unsigned x = (cols - (titlesize + 2)) / 2;
            attron(A_BOLD|COLOR_PAIR(Colors_Frame));
            mvaddch(0, x, '(');
            mvaddch(0, x + titlesize + 1, ')');
            attroff(A_BOLD|COLOR_PAIR(Colors_Frame));
            mvaddstr(0, x + 1, opts.title.c_str());
        }
    }

    wclear(inner);

    unsigned file_selection = ctx.file_selection;
    unsigned file_count = ctx.file_list.size();
    unsigned display_max = getrows(inner);
    unsigned display_offset = file_selection;
    display_offset -= std::min(display_offset, display_max / 2);

    for (unsigned display_nth = 0; display_nth < display_max; ++display_nth) {
        unsigned index = display_nth + display_offset;
        if (index >= file_count)
            break;

        const File_Entry &fe = ctx.file_list[index];
        bool selected = index == file_selection;

        int sel_attr = 0;
        if (selected)
            sel_attr |= COLOR_PAIR(Colors_Select);
        int ent_attr = 0;
        if (fe.type == File_Type::Directory)
            ent_attr |= COLOR_PAIR(Colors_Highlight);

        if (selected)
            wattron(inner, sel_attr);
        else
            wattron(inner, ent_attr);
        mvwaddstr(inner, display_nth, 0, fe.name.c_str());
        if (!selected)
            wattroff(inner, ent_attr);
        if (fe.type == File_Type::Directory)
            waddch(inner, '/');
        if (selected)
            wattroff(inner, sel_attr);
    }
}

void update_file_list(File_Selection_Context &ctx)
{
    File_Selection_Options &opts = *ctx.opts;
    const std::string &directory = opts.directory;

    ctx.file_list.clear();

    File_Entry fe;
    fe.name = "..";
    fe.type = File_Type::Directory;
    ctx.file_list.push_back(fe);

    DIR_u dir(opendir(directory.c_str()));
    if (!dir)
        return;

    while (dirent *ent = readdir(dir.get())) {
        fe.name = ent->d_name;

        if (fe.name == "." || fe.name == "..")
            continue;
        if (!opts.show_hidden_files && fe.name.front() == '.')
            continue;

        struct stat st;
        if (stat((directory + '/' + fe.name).c_str(), &st) == -1)
            continue;

        if (S_ISDIR(st.st_mode))
            fe.type = File_Type::Directory;
        else
            fe.type = File_Type::Regular;
        ctx.file_list.push_back(fe);
    }

    std::sort(ctx.file_list.begin(), ctx.file_list.end());
}

static std::string relative_path(std::string dir, const std::string &entry)
{
#if !defined(_WIN32)
    auto separator = [](char c) -> bool { return c == '/'; };
#else
    auto separator = [](char c) -> bool { return c == '/' || c == '\\'; };
#endif
    while (!dir.empty() && separator(dir.back()))
        dir.pop_back();
    if (entry == ".") {
        if (dir.empty())
            dir = "/";
        return dir;
    }
    else if (entry == "..") {
#if !defined(_WIN32)
        size_t pos = dir.rfind('/');
#else
        size_t pos = dir.find_last_of("/\\");
#endif
        if (pos != std::string::npos)
            dir.resize(pos);
        while (!dir.empty() && separator(dir.back()))
            dir.pop_back();
        if (dir.empty())
            dir = "/";
        return dir;
    } else {
        return dir + '/' + entry;
    }
}

std::string visit_file(File_Selection_Context &ctx)
{
    File_Entry &fe = ctx.file_list.at(ctx.file_selection);
    File_Selection_Options &opts = *ctx.opts;

    std::string relative = relative_path(opts.directory, fe.name);
    if (fe.type == File_Type::Directory) {
        opts.directory = relative;
        ctx.file_selection  = 0;
        update_file_list(ctx);
        return std::string();
    }
    else {
        return relative;
    }
}
#endif
