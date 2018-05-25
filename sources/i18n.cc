//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#if defined(ADLJACK_I18N)
#define ADLJACK_NO_PDC_I18N_MACROS
#include "i18n.h"
#include <iconv.h>
#include <locale>
#include <type_traits>
#include <stdio.h>
#include <string.h>

struct Iconv_Deleter {
    void operator()(iconv_t x) const noexcept { iconv_close(x); }
};
typedef std::unique_ptr<
    std::remove_pointer<iconv_t>::type, Iconv_Deleter> iconv_u;

#if defined(PDCURSES) && !defined(PDC_WIDE)

static const char *output_encoding = "CP437";

static iconv_t get_cvt_from_utf8()
{
    static iconv_u cvt;
    if (!cvt)
        cvt.reset(iconv_open(::output_encoding, "UTF-8"));
    return cvt.get();
}

static iconv_t get_cvt_from_utf32()
{
    static iconv_u cvt;
    if (!cvt)
        cvt.reset(iconv_open(::output_encoding, "UTF-32"));
    return cvt.get();
}

static char char_to_pdc(char32_t ucs4, char defchar = ' ')
{
    char result_char = defchar;

    iconv_t cd = get_cvt_from_utf32();
    if (!cd)
        return result_char;

    const char *input = (char *)&ucs4;
    size_t input_size = sizeof(ucs4);
    char *result = &result_char;
    size_t result_size = 1;
    iconv(cd, (char **)&input, &input_size, &result, &result_size);
    return result_char;
}

static std::string string_to_pdc(
    const char *input, size_t input_size = (size_t)-1)
{
    if (input_size == (size_t)-1)
        input_size = strlen(input);

    iconv_t cd = get_cvt_from_utf8();
    if (!cd)
        return std::string();

    std::string result_buf(input_size, '\0');
    char *result = &result_buf[0];
    size_t result_size = result_buf.size();

    while (input_size > 0) {
        size_t old_input_size = input_size;
        iconv(cd, (char **)&input, &input_size, &result, &result_size);
        if (input_size == old_input_size) {
            ++input;
            --input_size;
        }
    }

    result_buf.resize(result - result_buf.data());
    return result_buf;
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
