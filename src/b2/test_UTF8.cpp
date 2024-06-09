#include <shared/system.h>
#include <shared/testing.h>
#include <SDL.h>
#include "misc.h"
#include "misc.cpp"

int main(int, char *[]) {
    // Test cases from the Wikipedia page: https://en.wikipedia.org/wiki/UTF-8

    TEST_EQ_SS(GetUTF8StringForCodePoint(0x24), "\x24");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0xa3), "\xc2\xa3");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x418), "\xd0\x98");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x939), "\xe0\xa4\xb9");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x20ac), "\xe2\x82\xac");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0xd55c), "\xed\x95\x9c");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x10348), "\xf0\x90\x8d\x88");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x1096b3), "\xf4\x89\x9a\xb3");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x2825f), "\xf0\xa8\x89\x9f");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x5450), "\xe5\x91\x90");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x35c2), "\xe3\x97\x82");
    TEST_EQ_SS(GetUTF8StringForCodePoint(0x8d8a), "\xe8\xb6\x8a");

    InitUTF8ConvertTables();

    {
        std::string ascii;
        GetBBCASCIIFromISO8859_1(&ascii, {'`', 0xa3});
        TEST_EQ_SS(ascii, "``");
    }

    const std::vector<uint8_t> annoying_chars = {'`', '|', '\\', '{', '[', ']', '}', '^', '_'};
    {
        TEST_EQ_SS(GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_PassThrough, false), "`|\\{[]}^_");
        TEST_EQ_SS(GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_OnlyGBP, false), "\xc2\xa3|\\{[]}^_");
    }

    {
        std::string utf8 = GetUTF8FromBBCASCII(annoying_chars, BBCUTF8ConvertMode_SAA5050, false);
        std::string ascii;

        uint32_t bad_codepoint;
        const uint8_t *bad_char_start;
        int bad_char_len;
        TEST_TRUE(GetBBCASCIIFromUTF8(&ascii, std::vector<uint8_t>(utf8.begin(), utf8.end()), &bad_codepoint, &bad_char_start, &bad_char_len));
        TEST_EQ_SS(ascii, std::string(annoying_chars.begin(), annoying_chars.end()));
    }

    return 0;
}
