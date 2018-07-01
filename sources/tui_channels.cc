//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "tui_channels.h"
#include "tui.h"

struct Channel_Monitor::Impl
{
    std::unique_ptr<std::string[]> rowdata;
    std::unique_ptr<std::string[]> rowattr;
    unsigned index = 0;
    unsigned rows = 0;
    //
    bool serial_valid = false;
    unsigned serial = 0;
    //
    struct Windows {
        WINDOW *outer_ = nullptr;
        WINDOW_u inner;
    };
    Windows win;
    //
    void update_display();
};

Channel_Monitor::Channel_Monitor()
    : P(new Impl)
{
}

Channel_Monitor::~Channel_Monitor()
{
}

void Channel_Monitor::setup_display(WINDOW *outer)
{
    P->win = Impl::Windows();
    P->win.outer_ = outer;

    P->rowdata.reset();
    P->rowattr.reset();
    P->index = 0;
    P->rows = 0;

    if (!outer)
        return;

    WINDOW *inner = derwin_s(outer, getrows(outer) - 2, getcols(outer) - 2, 2, 2);
    if (inner) {
        P->win.inner.reset(inner);

        unsigned rows = P->rows = getrows(inner);
        P->rowdata.reset(new std::string[rows]);
        P->rowattr.reset(new std::string[rows]);
    }
}

void Channel_Monitor::update(char *data, unsigned size, unsigned serial)
{
    if (P->serial_valid && P->serial == serial)
        return;

    P->serial_valid = true;
    P->serial = serial;

    unsigned index = P->index;
    unsigned rows = P->rows;
    if (rows > 0) {
        std::string &rowdata = P->rowdata[index];
        std::string &rowattr = P->rowattr[index];
        rowdata.assign(data, size / 2);
        rowattr.assign(data + size / 2, size / 2);
        P->index = (index + 1) % rows;
    }

    P->update_display();
}

int Channel_Monitor::key(int key)
{
    switch (key) {
    case 'c':
    case 'C':
    case 27:  // escape
        return 0;
    }

    return 1;
}

void Channel_Monitor::Impl::update_display()
{
    if (WINDOW *w = win.outer_) {
        wattron(w, A_BOLD|COLOR_PAIR(Colors_Frame));
        wborder(w, ' ', ' ', '-', '-', '-', '-', '-', '-');
        wattroff(w, A_BOLD|COLOR_PAIR(Colors_Frame));
    }

    if (WINDOW *w = win.inner.get()) {
        unsigned index = this->index;
        unsigned rows = this->rows;
        for (unsigned row = 0; row < rows; ++row) {
            wmove(w, row, 0);

            unsigned j = (index + row + rows) % rows;
            const std::string &rowdata = this->rowdata[j];
            const std::string &rowattr = this->rowattr[j];

            unsigned cols1 = getcols(w);
            unsigned cols2 = rowdata.size();

            unsigned pad = 0;
            if (cols2 < cols1)
                pad = (cols1 - cols2) / 2;

            for (unsigned col = 0; col < pad; ++col)
                waddch(w, ' ');
            for (unsigned col = 0; col < cols1 && col < cols2; ++col)
                waddch(w, rowdata[col]);

            wclrtoeol(w);
        }
        wnoutrefresh(w);
    }
}
