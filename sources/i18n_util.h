//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once
#if defined(ADLJACK_I18N)
#include <iconv.h>
#include <string>
#include <system_error>

struct Iconv_Handle {
    explicit Iconv_Handle(iconv_t cd = (iconv_t)-1) noexcept
        : cd_(cd) {}
    ~Iconv_Handle() noexcept
        { reset(); }
    void reset(iconv_t cd = (iconv_t)-1) noexcept
        { if (*this) iconv_close(cd_); cd_ = cd; }
    iconv_t get() const noexcept
        { return cd_; }
    explicit operator bool() const noexcept
        { return cd_ != (iconv_t)-1; }
    Iconv_Handle(const Iconv_Handle &) = delete;
    Iconv_Handle &operator=(const Iconv_Handle &) = delete;
private:
    iconv_t cd_ = (iconv_t)-1;
};

enum class Encoding {
    Local8,
    UTF8,
    UTF32,
    CP437,
};

template <Encoding e>
struct Encoding_Traits;

template <Encoding Target, Encoding Source>
struct Encoder {
    typedef typename Encoding_Traits<Target>::character_type target_character;
    typedef typename Encoding_Traits<Source>::character_type source_character;
    typedef std::basic_string<target_character> target_string;
    typedef std::basic_string<source_character> source_string;

    iconv_t handle();
    void clear_state();
    target_string from_bytes(const uint8_t *input, size_t input_size = (size_t)-1);
    target_string from_string(const source_character *input, size_t input_size = (size_t)-1);
    target_string from_string(const source_string &input);
    size_t next_character(const uint8_t *input, size_t input_size, target_character *dst);

private:
    Iconv_Handle cd_;
};

//------------------------------------------------------------------------------
#define DEFINE_ENCODING(enc, typ, nam)                  \
    template <> struct Encoding_Traits<Encoding::enc> { \
        static const char *name() { return nam; }       \
        typedef typ character_type;                     \
    }

DEFINE_ENCODING(Local8, char, "");
DEFINE_ENCODING(UTF8, char, "UTF-8");
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
DEFINE_ENCODING(UTF32, char32_t, "UTF-32LE");
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
DEFINE_ENCODING(UTF32, char32_t, "UTF-32BE");
#endif
DEFINE_ENCODING(CP437, char, "CP437");

//------------------------------------------------------------------------------
template <Encoding Target, Encoding Source>
iconv_t Encoder<Target, Source>::handle()
{
    if (!cd_)
        cd_.reset(iconv_open(Encoding_Traits<Target>::name(), Encoding_Traits<Source>::name()));
    if (!cd_)
        throw std::system_error(errno, std::generic_category(), "iconv_open");
    return cd_.get();
}

template <Encoding Target, Encoding Source>
void Encoder<Target, Source>::clear_state()
{
    if (cd_)
        iconv(cd_.get(), nullptr, nullptr, nullptr, nullptr);
}

template <Encoding Target, Encoding Source>
auto Encoder<Target, Source>::from_bytes(const uint8_t *input, size_t input_size) -> target_string
{
    if (input_size == (size_t)-1) {
        size_t nchars = std::char_traits<source_character>::length((const source_character *)input);
        input_size = nchars * sizeof(source_character);
    }

    target_string result_buf(input_size, '\0');
    target_character *result = &result_buf[0];
    size_t result_size = result_buf.size() * sizeof(target_character);

    clear_state();
    iconv_t cvt = handle();
    while (input_size > 0) {
        size_t old_input_size = input_size;
        iconv(cvt, (char **)&input, &input_size, (char **)&result, &result_size);
        if (input_size == old_input_size) {
            ++input;
            --input_size;
        }
    }

    result_buf.resize(result - result_buf.data());
    return result_buf;
}

template <Encoding Target, Encoding Source>
auto Encoder<Target, Source>::from_string(const source_character *input, size_t input_size) -> target_string
{
    if (input_size == (size_t)-1)
        input_size = std::char_traits<source_character>::length(input);
    return from_bytes((const uint8_t *)input, input_size * sizeof(source_character));
}

template <Encoding Target, Encoding Source>
auto Encoder<Target, Source>::from_string(const source_string &input) -> target_string
{
    return from_string(input.data(), input.size());
}

template <Encoding Target, Encoding Source>
size_t Encoder<Target, Source>::next_character(const uint8_t *input, size_t input_size, target_character *dst)
{
    size_t old_input_size = input_size;
    size_t result_size = sizeof(target_character);
    iconv(handle(), (char **)&input, &input_size, (char **)&dst, &result_size);
    return old_input_size - input_size;
}

#endif  // defined(ADLJACK_I18N)
