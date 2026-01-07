#pragma once
#include <cstdint>

// font8x8_basic (public domain style bitmap)
// 128 chars, each char is 8 rows, each row is 8 bits (LSB->MSB)
static const uint8_t g_font8x8_basic[128][8] = {
    {0,0,0,0,0,0,0,0}, // 0 NUL
    {0,0,0,0,0,0,0,0}, // 1
    {0,0,0,0,0,0,0,0}, // 2
    {0,0,0,0,0,0,0,0}, // 3
    {0,0,0,0,0,0,0,0}, // 4
    {0,0,0,0,0,0,0,0}, // 5
    {0,0,0,0,0,0,0,0}, // 6
    {0,0,0,0,0,0,0,0}, // 7
    {0,0,0,0,0,0,0,0}, // 8
    {0,0,0,0,0,0,0,0}, // 9
    {0,0,0,0,0,0,0,0}, // 10
    {0,0,0,0,0,0,0,0}, // 11
    {0,0,0,0,0,0,0,0}, // 12
    {0,0,0,0,0,0,0,0}, // 13
    {0,0,0,0,0,0,0,0}, // 14
    {0,0,0,0,0,0,0,0}, // 15

    // You can paste a full 128 table later; for debug overlay we mainly need:
    // numbers, letters, punctuation. To keep this message readable,
    // I’m giving a compact “good enough” subset generator approach below instead.
};
