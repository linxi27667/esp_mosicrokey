#include "game_internal.hpp"
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstring>

namespace mosicrokey {
void RasterPixel(std::vector<uint16_t>& pixels, int x, int y, uint16_t color) {
    if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) pixels[y * kWidth + x] = color;
}
void RasterRect(std::vector<uint16_t>& pixels, int x, int y, int w, int h, uint16_t color) {
    for (int row = std::max(0, y); row < std::min(kHeight, y + h); ++row)
        for (int column = std::max(0, x); column < std::min(kWidth, x + w); ++column) pixels[row * kWidth + column] = color;
}
void RasterLine(std::vector<uint16_t>& pixels, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        RasterPixel(pixels, x0, y0, color); if (x0 == x1 && y0 == y1) break;
        int twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}
void RasterText(std::vector<uint16_t>& pixels, int x, int y, const char* text, uint16_t color) {
    // Original compact 3x5 glyphs for game HUDs. UI uses the Chinese font.
    static constexpr char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:.-!?";
    static constexpr uint16_t glyphs[] = {
        0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf,
        0x2bed,0x6bae,0x7927,0x6b6e,0x79e7,0x79e4,0x796f,0x5bed,0x7497,0x126a,
        0x5bad,0x4927,0x5fed,0x5ffd,0x7b6f,0x7be4,0x7b7b,0x7bad,0x79cf,0x7492,
        0x5b6f,0x5b6a,0x5bfd,0x5aad,0x5a92,0x72a7,0x0410,0x0002,0x01c0,0x2490,0x7282};
    int origin = x;
    for (size_t i = 0; text[i] && i < 96; ++i) {
        if (text[i] == '\n') { x = origin; y += 7; continue; }
        char character = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
        const char* found = std::strchr(alphabet, character);
        uint16_t glyph = found ? glyphs[found - alphabet] : 0;
        for (int row = 0; row < 5; ++row) for (int column = 0; column < 3; ++column)
            if (glyph & (1u << (14 - row * 3 - column))) RasterPixel(pixels, x + column, y + row, color);
        x += 4;
    }
}
}
