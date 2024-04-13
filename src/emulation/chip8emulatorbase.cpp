//---------------------------------------------------------------------------------------
// src/emulation/chip8emulatorbase.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------
#include <emulation/chip8cores.hpp>
#include <emulation/chip8emulatorbase.hpp>
#include <emulation/chip8dream.hpp>
#include <emulation/logger.hpp>
#include <emulation/properties.hpp>

namespace emu {

static uint8_t g_chip8VipFont[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x60, 0x20, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0xA0, 0xA0, 0xF0, 0x20, 0x20,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x10, 0x10, 0x10,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xF0, 0x50, 0x70, 0x50, 0xF0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xF0, 0x50, 0x50, 0x50, 0xF0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80   // F
};

static uint8_t  g_chip8EtiFont[] = {
    0xE0, 0xA0, 0xA0, 0xA0, 0xE0,  // 0
    0x20, 0x20, 0x20, 0x20, 0x20,  // 1
    0xE0, 0x20, 0xE0, 0x80, 0xE0,  // 2
    0xE0, 0x20, 0xE0, 0x20, 0xE0,  // 3
    0xA0, 0xA0, 0xE0, 0x20, 0x20,  // 4
    0xE0, 0x80, 0xE0, 0x20, 0xE0,  // 5
    0xE0, 0x80, 0xE0, 0xA0, 0xE0,  // 6
    0xE0, 0x20, 0x20, 0x20, 0x20,  // 7
    0xE0, 0xA0, 0xE0, 0xA0, 0xE0,  // 8
    0xE0, 0xA0, 0xE0, 0x20, 0xE0,  // 9
    0xE0, 0xA0, 0xE0, 0xA0, 0xA0,  // A
    0x80, 0x80, 0xE0, 0xA0, 0xE0,  // B
    0xE0, 0x80, 0x80, 0x80, 0xE0,  // C
    0x20, 0x20, 0xE0, 0xA0, 0xE0,  // D
    0xE0, 0x80, 0xE0, 0x80, 0xE0,  // E
    0xE0, 0x80, 0xC0, 0x80, 0x80,  // F
};

static uint8_t g_chip8DreamFont[] =   {
    0xE0, 0xA0, 0xA0, 0xA0, 0xE0,  // 0
    0x40, 0x40, 0x40, 0x40, 0x40,  // 1
    0xE0, 0x20, 0xE0, 0x80, 0xE0,  // 2
    0xE0, 0x20, 0xE0, 0x20, 0xE0,  // 3
    0x80, 0xA0, 0xA0, 0xE0, 0x20,  // 4
    0xE0, 0x80, 0xE0, 0x20, 0xE0,  // 5
    0xE0, 0x80, 0xE0, 0xA0, 0xE0,  // 6
    0xE0, 0x20, 0x20, 0x20, 0x20,  // 7
    0xE0, 0xA0, 0xE0, 0xA0, 0xE0,  // 8
    0xE0, 0xA0, 0xE0, 0x20, 0xE0,  // 9
    0xE0, 0xA0, 0xE0, 0xA0, 0xA0,  // A
    0xC0, 0xA0, 0xE0, 0xA0, 0xC0,  // B
    0xE0, 0x80, 0x80, 0x80, 0xE0,  // C
    0xC0, 0xA0, 0xA0, 0xA0, 0xC0,  // D
    0xE0, 0x80, 0xE0, 0x80, 0xE0,  // E
    0xE0, 0x80, 0xC0, 0x80, 0x80,  // F
};

static uint8_t g_chip48Font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};

static uint8_t g_ship10BigFont[] = {
    0x3C, 0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0x7E, 0x3C, // big 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, // big 1
    0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF, // big 2
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C, // big 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06, // big 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C, // big 5
    0x3E, 0x7C, 0xE0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C, // big 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C, // big 8
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C  // big 9
};

static uint8_t g_ship11BigFont[] = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, // big 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, // big 1
    0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF, // big 2
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C, // big 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06, // big 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C, // big 5
    0x3E, 0x7C, 0xE0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C, // big 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C, // big 8
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C  // big 9
};

static uint8_t g_fishNChipBigFont[] = {
    0x7c, 0xc6, 0xce, 0xde, 0xd6, 0xf6, 0xe6, 0xc6, 0x7c, 0x00, // big 0
    0x10, 0x30, 0xf0, 0x30, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x00, // big 1
    0x78, 0xcc, 0xcc, 0xc,  0x18, 0x30, 0x60, 0xcc, 0xfc, 0x00, // big 2
    0x78, 0xcc, 0x0c, 0x0c, 0x38, 0x0c, 0x0c, 0xcc, 0x78, 0x00, // big 3
    0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x1e, 0x00, // big 4
    0xfc, 0xc0, 0xc0, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0x00, // big 5
    0x38, 0x60, 0xc0, 0xc0, 0xf8, 0xcc, 0xcc, 0xcc, 0x78, 0x00, // big 6
    0xfe, 0xc6, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00, // big 7
    0x78, 0xcc, 0xcc, 0xec, 0x78, 0xdc, 0xcc, 0xcc, 0x78, 0x00, // big 8
    0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x18, 0x18, 0x30, 0x70, 0x00, // big 9
    0x30, 0x78, 0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0x00, // big A
    0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0xfc, 0x00, // big B
    0x3c, 0x66, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x66, 0x3c, 0x00, // big C
    0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00, // big D
    0xfe, 0x62, 0x60, 0x64, 0x7c, 0x64, 0x60, 0x62, 0xfe, 0x00, // big E
    0xfe, 0x66, 0x62, 0x64, 0x7c, 0x64, 0x60, 0x60, 0xf0, 0x00  // big F
};

static uint8_t g_megachip8BigFont[] = {
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, // big 1
    0x3e, 0x7f, 0xc3, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xff, 0xff, // big 2
    0x3c, 0x7e, 0xc3, 0x03, 0x0e, 0x0e, 0x03, 0xc3, 0x7e, 0x3c, // big 3
    0x06, 0x0e, 0x1e, 0x36, 0x66, 0xc6, 0xff, 0xff, 0x06, 0x06, // big 4
    0xff, 0xff, 0xc0, 0xc0, 0xfc, 0xfe, 0x03, 0xc3, 0x7e, 0x3c, // big 5
    0x3e, 0x7c, 0xc0, 0xc0, 0xfc, 0xfe, 0xc3, 0xc3, 0x7e, 0x3c, // big 6
    0xff, 0xff, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
    0x3c, 0x7e, 0xc3, 0xc3, 0x7e, 0x7e, 0xc3, 0xc3, 0x7e, 0x3c, // big 8
    0x3c, 0x7e, 0xc3, 0xc3, 0x7f, 0x3f, 0x03, 0x03, 0x3e, 0x7c, // big 9
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c  // big 0
};


static uint8_t g_octoBigFont[] = {
    0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
    0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
    0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
    0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
    0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0  // F
};

std::pair<const uint8_t*, size_t> Chip8EmulatorBase::smallFontData(Chip8Font font)
{
    switch(font) {
        case C8F5_CHIP48: return {g_chip48Font, sizeof(g_chip48Font)};
        case C8F5_ETI: return {g_chip8EtiFont, sizeof(g_chip8EtiFont)};
        case C8F5_DREAM: return {g_chip8DreamFont, sizeof(g_chip8DreamFont)};
        default: return {g_chip8VipFont, sizeof(g_chip8VipFont)};
    }
}

std::pair<const uint8_t*, size_t> Chip8EmulatorBase::bigFontData(Chip8BigFont font)
{
    switch(font) {
        case C8F10_SCHIP10: return {g_ship10BigFont, sizeof(g_ship10BigFont)};
        case C8F10_MEGACHIP: return {g_megachip8BigFont, sizeof(g_megachip8BigFont)};
        case C8F10_XOCHIP: return {g_octoBigFont, sizeof(g_octoBigFont)};
        default: return {g_ship11BigFont, sizeof(g_ship11BigFont)};
    }
}

std::pair<const uint8_t*, size_t> Chip8EmulatorBase::getSmallFontData() const
{
    switch (_options.behaviorBase) {
        case Chip8EmulatorOptions::eCHIP48:
        case Chip8EmulatorOptions::eSCHIP10:
        case Chip8EmulatorOptions::eSCHIP11:
        case Chip8EmulatorOptions::eSCHPC:
        case Chip8EmulatorOptions::eSCHIP_MODERN:
        case Chip8EmulatorOptions::eMEGACHIP:
        case Chip8EmulatorOptions::eXOCHIP:
        case Chip8EmulatorOptions::eCHICUEYI:
            return smallFontData(C8F5_CHIP48);
        case Chip8EmulatorOptions::eCHIP8:
        case Chip8EmulatorOptions::eCHIP10:
        case Chip8EmulatorOptions::eCHIP8VIP:
        default:
            return smallFontData(C8F5_COSMAC);
    }
}
std::pair<const uint8_t*, size_t> Chip8EmulatorBase::getBigFontData() const
{
    switch (_options.behaviorBase) {
        case Chip8EmulatorOptions::eSCHIP10:
            return bigFontData(C8F10_SCHIP10);
        case Chip8EmulatorOptions::eSCHIP11:
        case Chip8EmulatorOptions::eSCHPC:
        case Chip8EmulatorOptions::eSCHIP_MODERN:
        case Chip8EmulatorOptions::eCHICUEYI:
            return bigFontData(C8F10_SCHIP11);
        case Chip8EmulatorOptions::eMEGACHIP:
            return bigFontData(C8F10_MEGACHIP);
        case Chip8EmulatorOptions::eXOCHIP:
            return bigFontData(C8F10_XOCHIP);
        case Chip8EmulatorOptions::eCHIP8:
        case Chip8EmulatorOptions::eCHIP10:
        case Chip8EmulatorOptions::eCHIP48:
        case Chip8EmulatorOptions::eCHIP8VIP:
        default:
            return {nullptr, 0};
    }
}

#if 0
std::unique_ptr<IChip8Emulator> Chip8EmulatorBase::create(Chip8EmulatorHost& host, Engine engine, Chip8EmulatorOptions& options, IChip8Emulator* iother)
{
    if(engine == eCHIP8TS) {
        if (options.optAllowHires) {
            if (options.optHas16BitAddr) {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, HiresSupport | MultiColor | WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, HiresSupport | MultiColor>>(host, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, HiresSupport | WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, HiresSupport>>(host, options, iother);
                }
            }
            else {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, HiresSupport | MultiColor | WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, HiresSupport | MultiColor>>(host, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, HiresSupport | WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, HiresSupport>>(host, options, iother);
                }
            }
        }
        else {
            if (options.optHas16BitAddr) {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, MultiColor | WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, MultiColor>>(host, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, 0>>(host, options, iother);
                }
            }
            else {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, MultiColor | WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, MultiColor>>(host, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, WrapSprite>>(host, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, 0>>(host, options, iother);
                }
            }
        }
    }
    else if(engine == eCHIP8MPT) {
        return std::make_unique<Chip8EmulatorFP>(host, options, iother);
    }
    else if(engine == eCHIP8VIP) {
        return std::make_unique<Chip8VIP>(host, options, iother);
    }
    else if(engine == eCHIP8DREAM) {
        return std::make_unique<Chip8Dream>(host, options, iother);
    }
    return std::make_unique<Chip8EmulatorVIP>(host, options, iother);
}
#endif

void Chip8EmulatorBase::reset()
{
    //auto& props = Properties::getProperties("Gene");
    //static const uint8_t defaultPalette[16] = {37, 255, 114, 41, 205, 153, 42, 213, 169, 85, 37, 114, 87, 159, 69, 9};
    static const uint8_t defaultPalette[16] = {0, 255, 182, 109, 224, 28, 3, 252, 160, 20, 2, 204, 227, 31, 162, 22};
    //static const uint8_t defaultPalette[16] = {172, 248, 236, 100, 205, 153, 42, 213, 169, 85, 37, 114, 87, 159, 69, 9};
    _cycleCounter = 0;
    _frameCounter = 0;
    _clearCounter = 0;
    _systemTime.reset();
    if(_options.optTraceLog)
        Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, 0}, "--- RESET ---");
    _rI = 0;
    _rPC = _options.startAddress;
    std::memset(_stack.data(), 0, 16 * 2);
    _rSP = 0;
    _rDT = 0;
    _rST = 0;
    std::memset(_rV.data(), 0, 16);
    std::memset(_memory.data(), 0, _memory.size());
    auto [smallFont, smallSize] = getSmallFontData();
    std::memcpy(_memory.data(), smallFont, smallSize);
    auto [bigFont, bigSize] = getBigFontData();
    if(bigSize)
        std::memcpy(_memory.data() + 16*5, bigFont, bigSize);
    std::memcpy(_xxoPalette.data(), defaultPalette, 16);
    std::memset(_xoAudioPattern.data(), 0, 16);
    _xoSilencePattern = true;
    _xoPitch = 64;
    _planes = 0xff;
    _screenAlpha = 0xff;
    _screenRGBA = &_screenRGBA1;
    _workRGBA = &_screenRGBA2;
    _screen.setAll(0);
    _screenRGBA1.setAll(0);
    _screenRGBA2.setAll(0);
    //_host.updatePalette(_xxoPalette);
    _execMode = _host.isHeadless() ? eRUNNING : ePAUSED;
    _cpuState = eNORMAL;
    _errorMessage.clear();
    _isHires = _options.optOnlyHires ? true : false;
    _isInstantDxyn = _options.optInstantDxyn;
    _isMegaChipMode = false;
    _planes = 1;
    _spriteWidth = 0;
    _spriteHeight = 0;
    _collisionColor = 1;
    _sampleLength = 0;
    _sampleStep = 0;
    _mcSamplePos = 0;
    _blendMode = eBLEND_NORMAL;
    _mcPalette.fill(0x00);
    _mcPalette[1] = 0xffffffff;
    _mcPalette[254] = 0xffffffff;
}

int64_t Chip8EmulatorBase::executeFor(int64_t micros)
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return 0;
    }
    if(_options.instructionsPerFrame) {
        auto startTime = _cycleCounter;
        auto microsPerCycle = 1000000.0 / ((int64_t)_options.instructionsPerFrame * _options.frameRate);
        auto endCycles = startTime + int64_t(micros/microsPerCycle);
        auto nextFrame = calcNextFrame();
        while(_execMode != ePAUSED && nextFrame <= endCycles) {
            executeInstructions(nextFrame - _cycleCounter);
            if(_cycleCounter == nextFrame) {
                handleTimer();
                nextFrame += _options.instructionsPerFrame;
            }
        }
        while (_execMode != ePAUSED && _cycleCounter < endCycles) {
            executeInstruction();
        }
        auto excessTime = int64_t((endCycles - _cycleCounter) * microsPerCycle);
        return excessTime;// > 0 ? excessTime : 0;
    }
    else {
        using namespace std::chrono;
        handleTimer();
        auto start = _cycleCounter;
        auto endTime = steady_clock::now() + microseconds(micros > 2000 ? micros * 3 / 4 : 0);
        do {
            executeInstructions(487);
        }
        while(_execMode != ePAUSED && steady_clock::now() < endTime);
        uint32_t actualIPF = _cycleCounter - start;
        _systemTime.setFrequency((_systemTime.getClockFreq() + actualIPF)>>1);
    }
    return 0;
}

void Chip8EmulatorBase::tick(int instructionsPerFrame)
{
    if(!instructionsPerFrame) {
        handleTimer();
        auto start = std::chrono::steady_clock::now();
        do {
            executeInstructions(487);
        }
        while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 12);
    }
    else {
        auto instructionsLeft = calcNextFrame() - _cycleCounter;
        if(instructionsLeft == instructionsPerFrame) {
            handleTimer();
        }
        executeInstructions(instructionsLeft);
    }
}

}  // namespace emu
