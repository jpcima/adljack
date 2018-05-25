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
    target_string from_bytes(const uint8_t *input, size_t input_size = (size_t)-1);
    target_string from_string(const source_character *input, size_t input_size = (size_t)-1);
    target_string from_string(const source_string &input);

private:
    static thread_local Iconv_Handle cd_;
};

//------------------------------------------------------------------------------
template <> struct Encoding_Traits<Encoding::Local8> {
    static const char *name() { return ""; }
    typedef char character_type;
};
template <> struct Encoding_Traits<Encoding::UTF8> {
    static const char *name() { return "UTF-8"; }
    typedef char character_type;
};
template <> struct Encoding_Traits<Encoding::UTF32> {
    static const char *name() { return "UTF-32"; }
    typedef char32_t character_type;
};
template <> struct Encoding_Traits<Encoding::CP437> {
    static const char *name() { return "CP437"; }
    typedef char character_type;
};

//------------------------------------------------------------------------------
template <Encoding Target, Encoding Source>
thread_local Iconv_Handle Encoder<Target, Source>::cd_;

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
auto Encoder<Target, Source>::from_bytes(const uint8_t *input, size_t input_size) -> target_string
{
    if (input_size == (size_t)-1) {
        size_t nchars = std::char_traits<source_character>::length((const source_character *)input);
        input_size = nchars * sizeof(source_character);
    }

    target_string result_buf(input_size / sizeof(source_character), '\0');
    target_character *result = &result_buf[0];
    size_t result_size = result_buf.size() * sizeof(target_character);

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

#endif  // defined(ADLJACK_I18N)
