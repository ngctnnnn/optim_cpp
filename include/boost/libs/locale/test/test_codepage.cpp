//
// Copyright (c) 2009-2011 Artyom Beilis (Tonkikh)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/locale/encoding.hpp>
#include <boost/locale/generator.hpp>
#include <boost/locale/info.hpp>
#include <boost/locale/localization_backend.hpp>
#include <algorithm>
#include <fstream>
#include <vector>

#ifndef BOOST_LOCALE_NO_POSIX_BACKEND
#    ifdef __APPLE__
#        include <xlocale.h>
#    endif
#    include <locale.h>
#endif

#if !defined(BOOST_LOCALE_WITH_ICU) && !defined(BOOST_LOCALE_WITH_ICONV) \
  && (defined(BOOST_WINDOWS) || defined(__CYGWIN__))
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "boostLocale/test/tools.hpp"
#include "boostLocale/test/unit_test.hpp"

bool test_iso;
bool test_iso_8859_8 = true;
bool test_utf;
bool test_sjis;

std::string he_il_8bit;
std::string en_us_8bit;
std::string ja_jp_shiftjis;

template<typename Char>
std::basic_string<Char> read_file(std::basic_istream<Char>& in)
{
    std::basic_string<Char> res;
    Char c;
    while(in.get(c))
        res += c;
    return res;
}

template<typename Char>
void test_ok(const std::string& content, const std::locale& l, std::basic_string<Char> cmp = std::basic_string<Char>())
{
    typedef std::basic_fstream<Char> stream_type;
    if(cmp.empty())
        cmp = to<Char>(content);

    {
        remove_file_on_exit _("testi.txt");
        {
            std::ofstream testi("testi.txt");
            testi << content;
        }
        stream_type testi("testi.txt", stream_type::in);
        testi.imbue(l);
        TEST(read_file<Char>(testi) == cmp);
    }

    {
        remove_file_on_exit _("testo.txt");
        {
            stream_type testo("testo.txt", stream_type::out);
            testo.imbue(l);
            testo << cmp;
        }
        std::ifstream testo("testo.txt");
        TEST(read_file<char>(testo) == content);
    }
}

template<typename Char>
void test_rfail(const std::string& content, const std::locale& l, int pos)
{
    remove_file_on_exit _("testi.txt");
    {
        std::ofstream f("testi.txt");
        f << content;
    }

    typedef std::basic_fstream<Char> stream_type;

    stream_type f("testi.txt", stream_type::in);
    f.imbue(l);
    Char c;
    for(int i = 0; i < pos; i++) {
        f.get(c);
        if(f.fail()) { // failed before as detected errors at forward;
            return;
        }
        TEST(f);
    }
    // if the pos above succeed, at this point
    // it MUST fail
    TEST(f.get(c).fail());
}

template<typename Char>
void test_wfail(const std::string& content, const std::locale& l, int pos)
{
    typedef std::basic_fstream<Char> stream_type;
    remove_file_on_exit _("testo.txt");
    stream_type f("testo.txt", stream_type::out);
    f.imbue(l);
    std::basic_string<Char> out = to<Char>(content);
    int i;
    for(i = 0; i < pos; i++) {
        f << out.at(i);
        f << std::flush;
        TEST(f.good());
    }
    f << out.at(i);
    TEST(f.fail() || (f << std::flush).fail());
}

template<typename Char>
void test_for_char()
{
    boost::locale::generator g;
    if(test_utf) {
        std::cout << "    UTF-8" << std::endl;
        test_ok<Char>("grüße\nn i", g("en_US.UTF-8"));
        test_rfail<Char>("abc\xFF\xFF", g("en_US.UTF-8"), 3);
        std::cout << "    Testing codepoints above 0xFFFF" << std::endl;
        std::cout << "      Single U+2008A" << std::endl;
        test_ok<Char>("\xf0\xa0\x82\x8a", g("en_US.UTF-8")); // U+2008A
        std::cout << "      Single U+2008A within text" << std::endl;
        test_ok<Char>("abc\"\xf0\xa0\x82\x8a\"", g("en_US.UTF-8")); // U+2008A
        std::string one = "\xf0\xa0\x82\x8a";
        std::string res;
        for(unsigned i = 0; i < 1000; i++)
            res += one;
        std::cout << "      U+2008A x 1000" << std::endl;
        test_ok<Char>(res.c_str(), g("en_US.UTF-8")); // U+2008A
    } else {
        std::cout << "    UTF-8 Not supported \n";
    }

    if(test_iso) {
        if(test_iso_8859_8) {
            std::cout << "    ISO8859-8" << std::endl;
            test_ok<Char>("hello \xf9\xec\xe5\xed", g(he_il_8bit), to<Char>("hello שלום"));
        }
        std::cout << "    ISO8859-1" << std::endl;
        test_ok<Char>(to<char>("grüße\nn i"), g(en_us_8bit), to<Char>("grüße\nn i"));
        test_wfail<Char>("grüßen שלום", g(en_us_8bit), 7);
    }

    if(test_sjis) {
        std::cout << "    Shift-JIS" << std::endl;
        test_ok<Char>("\x93\xfa\x96\x7b",
                      g(ja_jp_shiftjis),
                      boost::locale::conv::to_utf<Char>("\xe6\x97\xa5\xe6\x9c\xac", "UTF-8")); // Japan
    }
}
void test_wide_io()
{
    std::cout << "  wchar_t" << std::endl;
    test_for_char<wchar_t>();

#if defined BOOST_LOCALE_ENABLE_CHAR16_T
    std::cout << "  char16_t" << std::endl;
    test_for_char<char16_t>();
#endif
#if defined BOOST_LOCALE_ENABLE_CHAR32_T
    std::cout << "  char32_t" << std::endl;
    test_for_char<char32_t>();
#endif
}

template<typename Char>
void test_pos(std::string source, std::basic_string<Char> target, std::string encoding)
{
    using namespace boost::locale::conv;
    boost::locale::generator g;
    std::locale l = encoding == "ISO8859-8" ? g("he_IL." + encoding) : g("en_US." + encoding);
    TEST(to_utf<Char>(source, encoding) == target);
    TEST(to_utf<Char>(source.c_str(), encoding) == target);
    TEST(to_utf<Char>(source.c_str(), source.c_str() + source.size(), encoding) == target);

    TEST(to_utf<Char>(source, l) == target);
    TEST(to_utf<Char>(source.c_str(), l) == target);
    TEST(to_utf<Char>(source.c_str(), source.c_str() + source.size(), l) == target);

    TEST(from_utf<Char>(target, encoding) == source);
    TEST(from_utf<Char>(target.c_str(), encoding) == source);
    TEST(from_utf<Char>(target.c_str(), target.c_str() + target.size(), encoding) == source);

    TEST(from_utf<Char>(target, l) == source);
    TEST(from_utf<Char>(target.c_str(), l) == source);
    TEST(from_utf<Char>(target.c_str(), target.c_str() + target.size(), l) == source);
}

#define TESTF(X) TEST_THROWS(X, boost::locale::conv::conversion_error)

template<typename Char>
void test_to_neg(std::string source, std::basic_string<Char> target, std::string encoding)
{
    using namespace boost::locale::conv;
    boost::locale::generator g;
    std::locale l = g("en_US." + encoding);

    TEST(to_utf<Char>(source, encoding) == target);
    TEST(to_utf<Char>(source.c_str(), encoding) == target);
    TEST(to_utf<Char>(source.c_str(), source.c_str() + source.size(), encoding) == target);
    TEST(to_utf<Char>(source, l) == target);
    TEST(to_utf<Char>(source.c_str(), l) == target);
    TEST(to_utf<Char>(source.c_str(), source.c_str() + source.size(), l) == target);

    TESTF(to_utf<Char>(source, encoding, stop));
    TESTF(to_utf<Char>(source.c_str(), encoding, stop));
    TESTF(to_utf<Char>(source.c_str(), source.c_str() + source.size(), encoding, stop));
    TESTF(to_utf<Char>(source, l, stop));
    TESTF(to_utf<Char>(source.c_str(), l, stop));
    TESTF(to_utf<Char>(source.c_str(), source.c_str() + source.size(), l, stop));
}

template<typename Char>
void test_from_neg(std::basic_string<Char> source, std::string target, std::string encoding)
{
    using namespace boost::locale::conv;
    boost::locale::generator g;
    std::locale l = g("en_US." + encoding);

    TEST(from_utf<Char>(source, encoding) == target);
    TEST(from_utf<Char>(source.c_str(), encoding) == target);
    TEST(from_utf<Char>(source.c_str(), source.c_str() + source.size(), encoding) == target);
    TEST(from_utf<Char>(source, l) == target);
    TEST(from_utf<Char>(source.c_str(), l) == target);
    TEST(from_utf<Char>(source.c_str(), source.c_str() + source.size(), l) == target);

    TESTF(from_utf<Char>(source, encoding, stop));
    TESTF(from_utf<Char>(source.c_str(), encoding, stop));
    TESTF(from_utf<Char>(source.c_str(), source.c_str() + source.size(), encoding, stop));
    TESTF(from_utf<Char>(source, l, stop));
    TESTF(from_utf<Char>(source.c_str(), l, stop));
    TESTF(from_utf<Char>(source.c_str(), source.c_str() + source.size(), l, stop));
}

template<typename Char>
std::basic_string<Char> utf(const char* s)
{
    return to<Char>(s);
}

template<>
std::basic_string<char> utf(const char* s)
{
    return s;
}

template<typename Char>
void test_with_0()
{
    std::string a("abc\0\0 yz\0", 3 + 2 + 3 + 1);
    TEST(boost::locale::conv::from_utf<Char>(boost::locale::conv::to_utf<Char>(a, "UTF-8"), "UTF-8") == a);
    TEST(boost::locale::conv::from_utf<Char>(boost::locale::conv::to_utf<Char>(a, "ISO8859-1"), "ISO8859-1") == a);
}

template<typename Char, int n = sizeof(Char)>
struct utfutf;

template<>
struct utfutf<char, 1> {
    static const char* ok() { return "grüßen"; }
    static const char* bad()
    {
        return "gr\xFF"
               "üßen";
    }
    // split into 2 to make SunCC happy
};

template<>
struct utfutf<wchar_t, 2> {
    static const wchar_t* ok() { return L"\x67\x72\xfc\xdf\x65\x6e"; }
    static const wchar_t* bad()
    {
        static wchar_t buf[256] = L"\x67\x72\xFF\xfc\xFE\xFD\xdf\x65\x6e";
        buf[2] = 0xDC01; // second surrogate must not be
        buf[4] = 0xD801; // First
        buf[5] = 0xD801; // Must be surrogate trail
        return buf;
    }
};
#ifdef BOOST_MSVC
#    pragma warning(push)
#    pragma warning(disable : 4309) // narrowing static_cast warning
#endif
template<>
struct utfutf<wchar_t, 4> {
    static const wchar_t* ok() { return L"\x67\x72\xfc\xdf\x65\x6e"; }
    static const wchar_t* bad()
    {
        static wchar_t buf[256] = L"\x67\x72\xFF\xfc\xdf\x65\x6e";
        buf[2] = static_cast<wchar_t>(0x1000000); // > 10FFFF
        return buf;
    }
};
#ifdef BOOST_MSVC
#    pragma warning(pop)
#endif

template<typename CharOut, typename CharIn>
void test_combinations()
{
    using boost::locale::conv::utf_to_utf;
    typedef utfutf<CharOut> out;
    typedef utfutf<CharIn> in;
    TEST((utf_to_utf<CharOut, CharIn>(in::ok()) == out::ok()));
    TESTF((utf_to_utf<CharOut, CharIn>(in::bad(), boost::locale::conv::stop)));
    TEST((utf_to_utf<CharOut, CharIn>(in::bad()) == out::ok()));
}

void test_all_combinations()
{
    std::cout << "Testing utf_to_utf\n";
    std::cout << "  char<-char" << std::endl;
    test_combinations<char, char>();
    std::cout << "  char<-wchar" << std::endl;
    test_combinations<char, wchar_t>();
    std::cout << "  wchar<-char" << std::endl;
    test_combinations<wchar_t, char>();
    std::cout << "  wchar<-wchar" << std::endl;
    test_combinations<wchar_t, wchar_t>();
}

template<typename Char>
void test_to()
{
    test_pos<Char>(to<char>("grüßen"), utf<Char>("grüßen"), "ISO8859-1");
    if(test_iso_8859_8)
        test_pos<Char>("\xf9\xec\xe5\xed", utf<Char>("שלום"), "ISO8859-8");
    test_pos<Char>("grüßen", utf<Char>("grüßen"), "UTF-8");
    test_pos<Char>("abc\"\xf0\xa0\x82\x8a\"", utf<Char>("abc\"\xf0\xa0\x82\x8a\""), "UTF-8");

    test_to_neg<Char>("g\xFFrüßen", utf<Char>("grüßen"), "UTF-8");
    test_from_neg<Char>(utf<Char>("hello שלום"), "hello ", "ISO8859-1");

    test_with_0<Char>();
}

void test_convert(const char* enc, const char* utf, const char* name)
{
    TEST_EQ(boost::locale::conv::to_utf<char>(enc, name), utf);
    TEST_EQ(boost::locale::conv::to_utf<wchar_t>(enc, name), boost::locale::conv::utf_to_utf<wchar_t>(utf));
#ifdef BOOST_LOCALE_ENABLE_CHAR16_T
    TEST_EQ(boost::locale::conv::to_utf<char16_t>(enc, name), boost::locale::conv::utf_to_utf<char16_t>(utf));
#endif
#ifdef BOOST_LOCALE_ENABLE_CHAR32_T
    TEST_EQ(boost::locale::conv::to_utf<char32_t>(enc, name), boost::locale::conv::utf_to_utf<char32_t>(utf));
#endif
}

void test_simple_conversions()
{
    std::cout << "- Testing Latin1 conversion\n";
    {
        using namespace boost::locale::conv;
        const std::string utf8_string = "A-Za-z0-9grüße";
        const std::string latin1_string = to<char>(utf8_string);
        const std::wstring wide_string = to<wchar_t>(utf8_string);

        TEST_EQ(to_utf<char>(latin1_string, "Latin1"), utf8_string);
        TEST_EQ(to_utf<wchar_t>(latin1_string, "Latin1"), wide_string);
        TEST_EQ(from_utf(utf8_string, "Latin1"), latin1_string);
        TEST_EQ(from_utf(wide_string, "Latin1"), latin1_string);
        TEST_EQ(utf_to_utf<char>(wide_string), utf8_string);
        TEST_EQ(utf_to_utf<wchar_t>(utf8_string), wide_string);
    }

    namespace blc = boost::locale::conv;
    std::cout << "- Testing correct invalid bytes skipping\n";
    try {
        std::cout << "-- ISO-8859-8" << std::endl;
        test_convert("\xFB", "", "ISO-8859-8");
        test_convert("\xFB-", "-", "ISO-8859-8");
        test_convert("test \xE0\xE1\xFB", "test \xd7\x90\xd7\x91", "ISO-8859-8");
        test_convert("test \xE0\xE1\xFB-", "test \xd7\x90\xd7\x91-", "ISO-8859-8");
    } catch(const blc::invalid_charset_error&) {
        std::cout << "--- not supported\n"; // LCOV_EXCL_LINE
    }
    try {
        std::cout << "-- cp932" << std::endl;
        test_convert("\x83\xF8", "", "cp932");
        test_convert("\x83\xF8-", "-", "cp932");
        test_convert("test\xE0\xA0 \x83\xF8", "test\xe7\x87\xbf ", "cp932");
        test_convert("test\xE0\xA0 \x83\xF8-", "test\xe7\x87\xbf -", "cp932");
    } catch(const blc::invalid_charset_error&) {
        std::cout << "--- not supported\n"; // LCOV_EXCL_LINE
    }
    try {
        // Testing a codepage which may be an issue on Windows, see issue #121
        std::cout << "-- iso-2022-jp" << std::endl;
        test_convert("\x1b$BE_5(\x1b(B", "冬季", "iso-2022-jp");
    } catch(const blc::invalid_charset_error&) {
        std::cout << "--- not supported\n"; // LCOV_EXCL_LINE
    }
}

void test_win_codepages();

void test_main(int /*argc*/, char** /*argv*/)
{
    // Sanity check to<char>
    TEST(to<char>("grüßen")
         == "gr\xFC\xDF"
            "en");
    TEST_THROWS(to<char>("€"), std::runtime_error);
    test_win_codepages();

    std::vector<std::string> backends;
#ifdef BOOST_LOCALE_WITH_ICU
    backends.push_back("icu");
#endif
#ifndef BOOST_LOCALE_NO_STD_BACKEND
    backends.push_back("std");
#endif
#ifndef BOOST_LOCALE_NO_WINAPI_BACKEND
    backends.push_back("winapi");
#endif
#ifndef BOOST_LOCALE_NO_POSIX_BACKEND
    backends.push_back("posix");
#endif

#if !defined(BOOST_LOCALE_WITH_ICU) && !defined(BOOST_LOCALE_WITH_ICONV) \
  && (defined(BOOST_WINDOWS) || defined(__CYGWIN__))
    test_iso_8859_8 = IsValidCodePage(28598) != 0;
#endif

    test_simple_conversions();

    for(const std::string& backendName : backends) {
        boost::locale::localization_backend_manager tmp_backend = boost::locale::localization_backend_manager::global();
        tmp_backend.select(backendName);
        boost::locale::localization_backend_manager::global(tmp_backend);

        if(backendName == "std") {
            en_us_8bit = get_std_name("en_US.ISO8859-1");
            he_il_8bit = get_std_name("he_IL.ISO8859-8");
            ja_jp_shiftjis = get_std_name("ja_JP.SJIS");
            if(!ja_jp_shiftjis.empty() && !test_std_supports_SJIS_codecvt(ja_jp_shiftjis)) {
                std::cout << "Warning: detected unproper support of " << ja_jp_shiftjis << " locale, disabling it"
                          << std::endl;
                ja_jp_shiftjis = "";
            }
        } else {
            en_us_8bit = "en_US.ISO8859-1";
            he_il_8bit = "he_IL.ISO8859-8";
            ja_jp_shiftjis = "ja_JP.SJIS";
        }

        std::cout << "Testing for backend " << backendName << std::endl;

        test_iso = true;
        if(backendName == "std" && (he_il_8bit.empty() || en_us_8bit.empty())) {
            std::cout << "no ISO locales available, passing" << std::endl;
            test_iso = false;
        }
        test_sjis = true;
        if(backendName == "std" && ja_jp_shiftjis.empty()) {
            test_sjis = false;
        }
        if(backendName == "winapi") {
            test_iso = false;
            test_sjis = false;
        }
        test_utf = true;
#ifndef BOOST_LOCALE_NO_POSIX_BACKEND
        if(backendName == "posix") {
            {
                locale_holder l(newlocale(LC_ALL_MASK, he_il_8bit.c_str(), 0));
                if(!l)
                    test_iso = false;
            }
            {
                locale_holder l(newlocale(LC_ALL_MASK, en_us_8bit.c_str(), 0));
                if(!l)
                    test_iso = false;
            }
            {
                locale_holder l(newlocale(LC_ALL_MASK, "en_US.UTF-8", 0));
                if(!l)
                    test_utf = false;
            }
#    ifdef BOOST_LOCALE_WITH_ICONV
            {
                locale_holder l(newlocale(LC_ALL_MASK, ja_jp_shiftjis.c_str(), 0));
                if(!l)
                    test_sjis = false;
            }
#    else
            test_sjis = false;
#    endif
        }
#endif

        if(backendName == "std" && (get_std_name("en_US.UTF-8").empty() || get_std_name("he_IL.UTF-8").empty())) {
            test_utf = false;
        }

        std::cout << "Testing wide I/O" << std::endl;
        test_wide_io();
        std::cout << "Testing charset to/from UTF conversion functions\n";
        std::cout << "  char" << std::endl;
        test_to<char>();
        std::cout << "  wchar_t" << std::endl;
        test_to<wchar_t>();
#ifdef BOOST_LOCALE_ENABLE_CHAR16_T
        if(backendName == "icu" || backendName == "std") {
            std::cout << "  char16_t" << std::endl;
            test_to<char16_t>();
        }
#endif
#ifdef BOOST_LOCALE_ENABLE_CHAR32_T
        if(backendName == "icu" || backendName == "std") {
            std::cout << "  char32_t" << std::endl;
            test_to<char32_t>();
        }
#endif

        test_all_combinations();
    }
}

// Internal tests, keep those out of the above scope

#include "../src/boost/locale/encoding/conv.hpp"
#include "../src/boost/locale/encoding/win_codepages.hpp"

void test_win_codepages()
{
    using namespace boost::locale::conv::impl;

    constexpr size_t n = sizeof(all_windows_encodings) / sizeof(all_windows_encodings[0]);
    for(const windows_encoding *it = all_windows_encodings, *end = all_windows_encodings + n; it != end; ++it) {
        TEST_EQ(normalize_encoding(it->name), it->name); // Must be normalized
        auto is_same_win_codepage = [&it](const boost::locale::conv::impl::windows_encoding& rhs) -> bool {
            return it->codepage == rhs.codepage && std::strcmp(it->name, rhs.name) == 0;
        };
        const auto* it2 = std::find_if(it + 1, end, is_same_win_codepage);
        TEST(it2 == end);
        if(it2 != end)
            std::cerr << "Duplicate entry: " << it->name << ':' << it->codepage << '\n';
    }
    const auto cmp = [](const boost::locale::conv::impl::windows_encoding& rhs,
                        const boost::locale::conv::impl::windows_encoding& lhs) -> bool { return rhs < lhs.name; };
    const auto* it = std::is_sorted_until(all_windows_encodings, all_windows_encodings + n, cmp);
    TEST(it == all_windows_encodings + n);
    if(it != all_windows_encodings + n)
        std::cerr << "First wrongly sorted element: " << it->name << '\n';
}

// boostinspect:noascii
