//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_USE_CURSES)
#include "tui_fileselect.h"
#include "tui.h"
#include "i18n.h"
#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#if defined(_WIN32)
#    include <windows.h>
#endif

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

///
static char ascii_to_lower(char x)
{
    return (x >= 'A' && x <= 'Z') ? (x - 'A' + 'a') : x;
}

static int ascii_case_cmp(const char *a, const char *b)
{
    for (;;) {
        char ca = *a++;
        char cb = *b++;
        if (ca == '\0')
            return cb ? -1 : 0;
        if (cb == '\0')
            return +1;
        ca = ascii_to_lower(ca);
        cb = ascii_to_lower(cb);
        if (ca != cb)
            return (ca < cb) ? -1 : +1;
    }
}

///
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
    return ascii_case_cmp(a.name.c_str(), b.name.c_str()) < 0;
}

struct File_Selector::Impl
{
    File_Selection_Options *opts_ = nullptr;
    //
    struct Windows {
        WINDOW *outer_ = nullptr;
        WINDOW_u title_;
        WINDOW_u inner_;
    };
    Windows win;
    std::vector<File_Entry> file_list_;
    unsigned file_selection_ = 0;
    //
    void update_file_list();
    void update_display();
    std::string visit_file();
    std::string visit_file_if_directory();
};

File_Selector::File_Selector(File_Selection_Options &opts)
    : P(new Impl)
{
    P->opts_ = &opts;
    P->file_list_.reserve(256);
    if (opts.directory.empty())
        opts.directory = '/';
    P->update_file_list();
}

File_Selector::~File_Selector()
{
}

void File_Selector::update()
{
    P->update_display();
}

File_Selection_Code File_Selector::key(int key)
{
    switch (key) {
    case KEY_DOWN:
        if (P->file_selection_ + 1 < P->file_list_.size())
            ++P->file_selection_;
        break;
    case KEY_UP:
        if (P->file_selection_ > 0)
            --P->file_selection_;
        break;
    case KEY_NPAGE: {
        unsigned count = std::min<unsigned>
            (10, P->file_list_.size() - P->file_selection_ - 1);
        P->file_selection_ += count;
        break;
    }
    case KEY_PPAGE: {
        unsigned count = std::min<unsigned>(10, P->file_selection_);
        P->file_selection_ -= count;
        break;
    }
    case KEY_ENTER:
    case '\r':
    case '\n': {
        std::string path = P->visit_file();
        if (!path.empty()) {
            P->opts_->filepath = path;
            return File_Selection_Code::Ok;
        }
        break;
    }
    case KEY_LEFT:
    case KEY_BACKSPACE:
    case '\b':
        P->file_selection_ = 0;  // the first entry ".."
        P->visit_file();
        break;
    case KEY_RIGHT:
        P->visit_file_if_directory();
        break;
    case 27:  // escape
        return File_Selection_Code::Cancel;
    }

    return File_Selection_Code::Continue;
}

void File_Selector::setup_display(WINDOW *outer)
{
    P->win = Impl::Windows();
    P->win.outer_ = outer;

    if (!outer)
        return;

    P->win.title_.reset(derwin_s(outer, 1, getcols(outer) - 2, 1, 1));
    P->win.inner_.reset(derwin_s(outer, getrows(outer) - 4, getcols(outer) - 2, 3, 1));
}

void File_Selector::Impl::update_display()
{
    File_Selection_Options &opts = *opts_;

    if (WINDOW *w = win.outer_) {
        const std::string &title = opts.title;
        size_t titlesize = title.size();

        wattron(w, A_BOLD|COLOR_PAIR(Colors_Frame));
        wborder(w, ' ', ' ', '-', '-', '-', '-', '-', '-');
        wattroff(w, A_BOLD|COLOR_PAIR(Colors_Frame));

        unsigned cols = getcols(w);
        if (cols >= titlesize + 2) {
            unsigned x = (cols - (titlesize + 2)) / 2;
            wattron(w, A_BOLD|COLOR_PAIR(Colors_Frame));
            mvwaddch(w, 0, x, '(');
            mvwaddch(w, 0, x + titlesize + 1, ')');
            wattroff(w, A_BOLD|COLOR_PAIR(Colors_Frame));
            mvwaddstr(w, 0, x + 1, title.c_str());
        }
        wnoutrefresh(w);
    }

    if (WINDOW *w = win.title_.get()) {
        wattron(w, A_UNDERLINE);
        mvwprintw(w, 0, 0, _("Directory: %s"), opts.directory.c_str());
        wattroff(w, A_UNDERLINE);
        wclrtoeol(w);
        wnoutrefresh(w);
    }

    if (WINDOW *w = win.inner_.get()) {
        unsigned file_selection = file_selection_;
        unsigned file_count = file_list_.size();
        unsigned display_max = getrows(w);
        unsigned display_offset = file_selection;
        display_offset -= std::min(display_offset, display_max / 2);

        for (unsigned display_nth = 0; display_nth < display_max; ++display_nth) {
            unsigned index = display_nth + display_offset;
            if (index >= file_count)
                break;

            const File_Entry &fe = file_list_[index];
            bool selected = index == file_selection;

            int sel_attr = 0;
            if (selected)
                sel_attr |= COLOR_PAIR(Colors_Select);
            int ent_attr = 0;
            if (fe.type == File_Type::Directory)
                ent_attr |= COLOR_PAIR(Colors_Highlight);

            if (selected)
                wattron(w, sel_attr);
            else
                wattron(w, ent_attr);
            mvwaddstr(w, display_nth, 0, fe.name.c_str());
            if (!selected)
                wattroff(w, ent_attr);
            if (fe.type == File_Type::Directory)
                waddch(w, '/');
            if (selected)
                wattroff(w, sel_attr);

            wclrtoeol(w);
        }

        wclrtobot(w);
        wnoutrefresh(w);
    }
}

static bool is_separator(char c)
{
#if !defined(_WIN32)
    return c == '/';
#else
    return c == '/' || c == '\\';
#endif
}

void File_Selector::Impl::update_file_list()
{
    File_Selection_Options &opts = *opts_;
#if !defined(_WIN32)
    const std::string &directory = opts.directory;
#else
    // needs terminator if it's just the drive letter
    std::string directory = opts.directory;
    if (directory.empty() || !is_separator(directory.back()))
        directory.push_back('/');
#endif

    file_list_.clear();

    File_Entry fe;
    fe.name = "..";
    fe.type = File_Type::Directory;
    file_list_.push_back(fe);

#if defined(_WIN32)
    // root directory special case
    if (directory == "/") {
        DWORD drivemask = GetLogicalDrives();
        for (unsigned i = 0; i < 26; ++i) {
            if (!(drivemask & (1 << i)))
                continue;
            char name[3] = {(char)('A' + i), ':', 0};
            fe.name = name;
            fe.type = File_Type::Directory;
            file_list_.push_back(fe);
        }
        return;
    }
#endif

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
        file_list_.push_back(fe);
    }

    std::sort(file_list_.begin(), file_list_.end());
}

static std::string relative_path(std::string dir, const std::string &entry)
{
    while (!dir.empty() && is_separator(dir.back()))
        dir.pop_back();
    if (entry == ".") {
        if (dir.empty())
            dir = "/";
        return dir;
    }
    else if (entry == "..") {
#if defined(_WIN32)
        if (dir.size() == 2 && dir[1] == ':')
            return "/";  // drive letter special case
#endif
#if !defined(_WIN32)
        size_t pos = dir.rfind('/');
#else
        size_t pos = dir.find_last_of("/\\");
#endif
        if (pos != std::string::npos)
            dir.resize(pos);
        while (!dir.empty() && is_separator(dir.back()))
            dir.pop_back();
        if (dir.empty())
            dir = "/";
        return dir;
    }
#if defined(_WIN32)
    else if (dir.empty()) {
        return entry;  // drive letter special case
    }
#endif
    else {
        return dir + '/' + entry;
    }
}

std::string File_Selector::Impl::visit_file()
{
    File_Entry &fe = file_list_.at(file_selection_);
    File_Selection_Options &opts = *opts_;

    std::string relative = relative_path(opts.directory, fe.name);
    if (fe.type == File_Type::Directory) {
        opts.directory = relative;
        file_selection_  = 0;
        update_file_list();
        return std::string();
    }
    else {
        return relative;
    }
}

std::string File_Selector::Impl::visit_file_if_directory()
{
    File_Entry &fe = file_list_.at(file_selection_);
    if (fe.type != File_Type::Directory)
        return std::string();
    return visit_file();
}
#endif
