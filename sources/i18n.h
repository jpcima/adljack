//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#if defined(ADLJACK_I18N)
#include <string>
#include <libintl.h>
#include <locale.h>

inline const char *_(const char *x)
{
    return gettext(x);
}

inline void i18n_setup()
{
    setlocale(LC_ALL, "");
    bindtextdomain("adljack", ADLJACK_PREFIX "/share/locale/");
    textdomain("adljack");
}

#include <curses.h>

#if defined(PDCURSES) && !defined(PDC_WIDE)
// the basic PDCurses font supports cp437; so a basic solution to support
// accented characters is to convert input text to cp437 encoding.

#if !defined(ADLJACK_NO_PDC_I18N_MACROS)
#define mvprintw pdc_ext_mvprintw
#define mvwprintw pdc_ext_mvwprintw
#define printw pdc_ext_printw
#define vw_printw pdc_ext_vw_printw
#define vwprintw pdc_ext_vwprintw
#define wprintw pdc_ext_wprintw
#define mvaddch pdc_ext_mvaddch
#define mvwaddch pdc_ext_mvwaddch
#define waddch pdc_ext_waddch
#define addch pdc_ext_addch
#define addstr pdc_ext_addstr
#define addnstr pdc_ext_addnstr
#define waddstr pdc_ext_waddstr
#define waddnstr pdc_ext_waddnstr
#define mvaddstr pdc_ext_mvaddstr
#define mvaddnstr pdc_ext_mvaddnstr
#define mvwaddstr pdc_ext_mvwaddstr
#define mvwaddnstr pdc_ext_mvwaddnstr
#endif

int pdc_ext_mvprintw(int, int, const char *, ...);
int pdc_ext_mvwprintw(WINDOW *, int, int, const char *, ...);
int pdc_ext_printw(const char *, ...);
int pdc_ext_vw_printw(WINDOW *, const char *, va_list);
int pdc_ext_vwprintw(WINDOW *, const char *, va_list);
int pdc_ext_wprintw(WINDOW *, const char *, ...);
int pdc_ext_mvaddch(int, int, char32_t);
int pdc_ext_mvwaddch(WINDOW *, int, int, char32_t);
int pdc_ext_addch(char32_t);
int pdc_ext_addstr(const char *);
int pdc_ext_addnstr(const char *, int);
int pdc_ext_waddch(WINDOW *, char32_t);
int pdc_ext_waddstr(WINDOW *, const char *);
int pdc_ext_waddnstr(WINDOW *, const char *, int);
int pdc_ext_mvaddstr(int, int, const char *);
int pdc_ext_mvaddnstr(int, int, const char *, int);
int pdc_ext_mvwaddstr(WINDOW *, int, int, const char *);
int pdc_ext_mvwaddnstr(WINDOW *, int, int, const char *, int);

#endif

#else

inline const char *_(const char *x)
{
    return x;
}

inline void i18n_setup()
{
}

#endif
