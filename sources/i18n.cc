//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_I18N)
#define ADLJACK_NO_PDC_I18N_MACROS
#include "i18n.h"
#include "i18n_util.h"
#include <locale>
#include <stdio.h>
#include <string.h>

#if defined(PDCURSES) && !defined(PDC_WIDE)

static constexpr Encoding pdc_fontenc = Encoding::CP437;

#if !defined(_WIN32)
static Encoder<Encoding::UTF32, Encoding::Local8> cvt_utf32_from_system;
#else
static Encoder<Encoding::UTF32, Encoding::UTF8> cvt_utf32_from_system;
#endif
static Encoder<pdc_fontenc, Encoding::UTF32> cvt_pdc_from_utf32;

static char char_to_pdc(char32_t ucs4, char defchar = ' ')
{
    std::string result = cvt_pdc_from_utf32.from_string(&ucs4, 1);
    return result.empty() ? defchar : result[0];
}

static std::string string_to_pdc(const char *input, size_t input_size = (size_t)-1)
{
    std::u32string result = cvt_utf32_from_system.from_string(input, input_size);
    switch (pdc_fontenc) {
    default:
        break;
    case Encoding::CP437:
        // substitute ligatures
        for (size_t pos; (pos = result.find(U'œ')) != result.npos;)
            result.replace(pos, 1, U"oe", 2);
        break;
    }
    return cvt_pdc_from_utf32.from_string(result);
}

int pdc_ext_mvprintw(int y, int x, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = move(y, x);
    if (ret == OK)
        ret = pdc_ext_vw_printw(stdscr, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = wmove(w, y, x);
    if (ret == OK)
        ret = pdc_ext_vw_printw(w, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_printw(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = pdc_ext_vw_printw(stdscr, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_vw_printw(WINDOW *w, const char *fmt, va_list ap)
{
    char buf[256];
    if (vsnprintf(buf, 256, fmt, ap) == -1)
        return ERR;
    buf[sizeof(buf) - 1] = '\0';
    std::string lat1 = string_to_pdc(buf, strlen(buf));
    return waddstr(w, lat1.c_str());
}

int pdc_ext_vwprintw(WINDOW *w, const char *fmt, va_list ap)
{
    return pdc_ext_vw_printw(w, fmt, ap);
}

int pdc_ext_wprintw(WINDOW *w, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = pdc_ext_vw_printw(w, fmt, ap);
    va_end(ap);
    return ret;
}

int pdc_ext_mvaddch(int y, int x, char32_t ch)
{
    return mvaddch(y, x, char_to_pdc(ch));
}

int pdc_ext_mvwaddch(WINDOW *w, int y, int x, char32_t ch)
{
    return mvwaddch(w, y, x, char_to_pdc(ch));
}

int pdc_ext_waddch(WINDOW *w, char32_t ch)
{
    return waddch(w, char_to_pdc(ch));
}

int pdc_ext_addch(char32_t ch)
{
    return addch(char_to_pdc(ch));
}

int pdc_ext_addstr(const char *str)
{
    return addstr(string_to_pdc(str).c_str());
}

int pdc_ext_addnstr(const char *str, int n)
{
    return addstr(string_to_pdc(str, n).c_str());
}

int pdc_ext_waddstr(WINDOW *w, const char *str)
{
    return waddstr(w, string_to_pdc(str).c_str());
}

int pdc_ext_waddnstr(WINDOW *w, const char *str, int n)
{
    return waddstr(w, string_to_pdc(str, n).c_str());
}

int pdc_ext_mvaddstr(int y, int x, const char *str)
{
    return mvaddstr(y, x, string_to_pdc(str).c_str());
}

int pdc_ext_mvaddnstr(int y, int x, const char *str, int n)
{
    return mvaddstr(y, x, string_to_pdc(str, n).c_str());
}

int pdc_ext_mvwaddstr(WINDOW *w, int y, int x, const char *str)
{
    return mvwaddstr(w, y, x, string_to_pdc(str).c_str());
}

int pdc_ext_mvwaddnstr(WINDOW *w, int y, int x, const char *str, int n)
{
    return mvwaddstr(w, y, x, string_to_pdc(str, n).c_str());
}
#endif  // defined(PDCURSES) && !defined(PDC_WIDE)

#endif  // defined(ADLJACK_I18N)
