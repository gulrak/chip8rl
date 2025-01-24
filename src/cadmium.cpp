//---------------------------------------------------------------------------------------
// src/cadmium.cpp
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
#include <raylib.h>
#include <rlguipp/rlguipp.hpp>
#include <stylemanager.hpp>
#include "configuration.hpp"

#include <raymath.h>

#include <date/date.h>
#include <fmt/format.h>
#include <stdendian/stdendian.h>
#include <about.hpp>
#include <chiplet/chip8decompiler.hpp>
#include <chiplet/utility.hpp>
#include <circularbuffer.hpp>
#include <debugger.hpp>
#include <emuhostex.hpp>
#include <emulation/c8bfile.hpp>
//#include <emulation/chip8generic.hpp>
#include <emulation/coreregistry.hpp>
//#include <emulation/dream6800.hpp>
#include <c8db/database.hpp>
#include <emulation/time.hpp>
#include <emulation/timecontrol.hpp>
#include <ghc/cli.hpp>
#include <logview.hpp>
#include <nlohmann/json.hpp>
#include <resourcemanager.hpp>
#include <systemtools.hpp>
#include <texturescaler.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <mutex>
#include <new>
#include <regex>
#include <thread>

#include "emulation/chip8strict.hpp"

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
extern "C" void openFileCallbackC(const char* str) __attribute__((used));
static std::function<void(const std::string&)> openFileCallback;
void openFileCallbackC(const char* str)
{
    if (openFileCallback)
        openFileCallback(str);
}
#ifdef WEB_WITH_FETCHING
#include <emscripten/fetch.h>
extern "C" void loadBinaryCallbackC(emscripten_fetch_t *fetch) __attribute__((used));
extern "C" void downloadFailedCallbackC(emscripten_fetch_t *fetch) __attribute__((used));
static std::function<void(std::string, const uint8_t*, size_t)> loadBinaryCallback;
void loadBinaryCallbackC(emscripten_fetch_t *fetch)
{
    if (loadBinaryCallback) {
        std::string_view url{fetch->url};
        auto pos = url.rfind('/');
        loadBinaryCallback(std::string(pos != std::string_view::npos ? url.substr(pos + 1) : url), reinterpret_cast<const unsigned char*>(fetch->data), fetch->numBytes);
    }
    emscripten_fetch_close(fetch);
}
void downloadFailedCallbackC(emscripten_fetch_t *fetch)
{
    emscripten_fetch_close(fetch);
}
#endif
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#if __clang__
#pragma GCC diagnostic ignored "-Wenum-compare-conditional"
#endif
#endif  // __GNUC__

extern "C" {
#include <octo_emulator.h>
}

#pragma GCC diagnostic pop

#include <fstream>

#endif

#include <editor.hpp>


#define CHIP8_STYLE_PROPS_COUNT 16
static const GuiStyleProp chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
    {0, 0, 0x2f7486ff},   // DEFAULT_BORDER_COLOR_NORMAL
    {0, 1, 0x024658ff},   // DEFAULT_BASE_COLOR_NORMAL
    {0, 2, 0x51bfd3ff},   // DEFAULT_TEXT_COLOR_NORMAL
      {0, 3, (int)0x82cde0ff},   // B! DEFAULT_BORDER_COLOR_FOCUSED
      {0, 4, 0x3299b4ff},   // A! DEFAULT_BASE_COLOR_FOCUSED
//    {0, 5, (int)0xb6e1eaff},   // DEFAULT_TEXT_COLOR_FOCUSED
    {0, 5, (int)0xeff8ffff},   // DEFAULT_TEXT_COLOR_FOCUSED
      {0, 6, (int)0x82cde0ff},   // B! DEFAULT_BORDER_COLOR_PRESSED
      {0, 7, 0x3299b4ff},   // A! DEFAULT_BASE_COLOR_PRESSED
    {0, 8, (int)0xeff8ffff},   // DEFAULT_TEXT_COLOR_PRESSED
    {0, 9, 0x134b5aff},   // DEFAULT_BORDER_COLOR_DISABLED
    {0, 10, 0x0e273aff},  // DEFAULT_BASE_COLOR_DISABLED
    {0, 11, 0x17505fff},  // DEFAULT_TEXT_COLOR_DISABLED
    {0, 16, 0x0000000e},  // DEFAULT_TEXT_SIZE
    {0, 17, 0x00000000},  // DEFAULT_TEXT_SPACING
    {0, 18, (int)0x81c0d0ff},  // DEFAULT_LINE_COLOR
    {0, 19, 0x00222bff},  // DEFAULT_BACKGROUND_COLOR
};

struct FontCharInfo {
    uint16_t codepoint;
    uint8_t data[5];
};
static FontCharInfo fontRom[] = {
    {32,0,0,0,0,0},
    {33,0,0,95,0,0},
    {34,0,7,0,7,0},
    {35,20,62,20,62,20},
    {36,36,42,127,42,18},
    {37,35,19,8,100,98},
    {38,54,73,85,34,80},
    {39,0,0,11,7,0},
    {40,0,28,34,65,0},
    {41,0,65,34,28,0},
    {42,42,28,127,28,42},
    {43,8,8,62,8,8},
    {44,0,0,176,112,0},
    {45,8,8,8,8,8},
    {46,0,96,96,0,0},
    {47,32,16,8,4,2},
    {48,62,65,65,62,0},
    {49,0,2,127,0,0},
    {50,98,81,73,73,70},
    {51,65,65,73,77,51},
    {52,15,8,8,127,8},
    {53,71,69,69,69,57},
    {54,60,74,73,73,48},
    {55,97,17,9,5,3},
    {56,54,73,73,73,54},
    {57,6,73,73,41,30},
    {58,0,54,54,0,0},
    {59,0,182,118,0,0},
    {60,8,20,34,65,0},
    {61,20,20,20,20,20},
    {62,0,65,34,20,8},
    {63,2,1,81,9,6},
    {64,62,65,93,85,94},
    {65,126,9,9,9,126},
    {66,127,73,73,73,54},
    {67,62,65,65,65,34},
    {68,127,65,65,65,62},
    {69,127,73,73,73,65},
    {70,127,9,9,9,1},
    {71,62,65,73,73,122},
    {72,127,8,8,8,127},
    {73,0,65,127,65,0},
    {74,32,64,64,64,63},
    {75,127,8,20,34,65},
    {76,127,64,64,64,64},
    {77,127,2,12,2,127},
    {78,127,2,4,8,127},
    {79,62,65,65,65,62},
    {80,127,9,9,9,6},
    {81,62,65,81,33,94},
    {82,127,9,25,41,70},
    {83,38,73,73,73,50},
    {84,1,1,127,1,1},
    {85,63,64,64,64,63},
    {86,31,32,64,32,31},
    {87,127,32,24,32,127},
    {88,99,20,8,20,99},
    {89,7,8,112,8,7},
    {90,97,81,73,69,67},
    {91,0,127,65,65,0},
    {92,2,4,8,16,32},
    {93,0,65,65,127,0},
    {94,4,2,1,2,4},
    {95,128,128,128,128,128},
    {96,0,7,11,0,0},
    {97,112,84,84,120,64},
    {98,64,127,68,68,60},
    {99,0,56,68,68,72},
    {100,56,68,68,127,64},
    {101,0,56,84,84,72},
    {102,0,8,124,10,2},
    {103,0,140,146,146,126},
    {104,0,127,4,4,120},
    {105,0,0,122,0,0},
    {106,0,64,128,116,0},
    {107,0,126,16,40,68},
    {108,0,2,126,64,0},
    {109,124,4,124,4,120},
    {110,0,124,4,4,120},
    {111,0,56,68,68,56},
    {112,0,252,36,36,24},
    {113,24,36,36,252,128},
    {114,0,124,8,4,4},
    {115,0,72,84,84,36},
    {116,0,4,62,68,32},
    {117,60,64,64,124,64},
    {118,12,48,64,48,12},
    {119,60,64,48,64,60},
    {120,68,36,56,72,68},
    {121,0,28,32,160,252},
    {122,64,100,84,76,4},
    {123,0,8,54,65,65},
    {124,0,0,119,0,0},
    {125,0,65,65,54,8},
    {126,2,1,2,2,1},
    {127,85,42,85,42,85},
    {160,0,0,0,0,0},
    {161,0,0,125,0,0},
    {162,56,68,254,68,40},
    {163,72,126,73,73,66},
    {164,93,34,34,34,93},
    {165,41,42,124,42,41},
    {166,0,0,119,0,0},
    {167,74,85,85,85,41},
    {168,0,3,0,3,0},
    {169,62,73,85,85,62},
    {170,92,85,85,94,80},
    {171,16,40,84,40,68},
    {172,8,8,8,8,56},
    {173,0,8,8,8,0},
    {174,62,93,77,89,62},
    {175,1,1,1,1,1},
    {176,6,9,9,6,0},
    {177,68,68,95,68,68},
    {178,9,12,10,9,0},
    {179,17,21,23,9,0},
    {180,0,4,2,1,0},
    {181,252,64,64,60,64},
    {182,6,127,1,127,1},
    {183,0,24,24,0,0},
    {184,0,128,128,64,0},
    {185,2,31,0,0,0},
    {186,38,41,41,38,0},
    {187,68,40,84,40,16},
    {188,34,23,104,244,66},
    {189,34,23,168,212,162},
    {190,41,19,109,244,66},
    {191,32,64,69,72,48},
    {192,120,21,22,20,120},
    {193,120,20,22,21,120},
    {194,120,22,21,22,120},
    {195,122,21,22,22,121},
    {196,120,21,20,21,120},
    {197,122,21,21,21,122},
    {198,126,9,127,73,73},
    {199,30,161,225,33,18},
    {200,124,85,86,84,68},
    {201,124,84,86,85,68},
    {202,124,86,85,86,68},
    {203,124,85,84,85,68},
    {204,0,68,125,70,0},
    {205,0,70,125,68,0},
    {206,0,70,125,70,0},
    {207,0,68,125,70,0},
    {208,8,127,73,65,62},
    {209,126,9,18,34,125},
    {210,56,69,70,68,56},
    {211,56,68,70,69,56},
    {212,56,70,69,70,56},
    {213,58,69,70,70,57},
    {214,56,69,68,69,56},
    {215,0,40,16,40,0},
    {216,94,33,93,66,61},
    {217,60,65,66,64,60},
    {218,60,64,66,65,60},
    {219,60,66,65,66,60},
    {220,60,65,64,65,60},
    {222,12,16,98,17,12},
    {222,127,20,20,20,8},
    {223,126,1,73,78,48},
    {224,112,85,86,120,64},
    {225,112,86,85,120,64},
    {226,112,86,85,122,64},
    {227,114,85,86,122,65},
    {228,112,85,84,121,64},
    {229,114,85,85,122,64},
    {230,116,84,124,84,88},
    {231,0,28,162,98,36},
    {232,0,56,85,86,72},
    {233,0,56,86,85,72},
    {234,0,58,85,86,72},
    {235,0,57,84,84,73},
    {236,0,1,122,0,0},
    {237,0,0,122,1,0},
    {238,0,2,121,2,0},
    {239,0,1,120,1,0},
    {240,53,73,74,77,56},
    {241,2,125,6,6,121},
    {242,0,56,69,70,56},
    {243,0,56,70,69,56},
    {244,0,58,69,70,56},
    {245,2,57,70,70,57},
    {246,0,57,68,68,57},
    {247,8,8,42,8,8},
    {248,0,120,116,76,60},
    {249,60,65,66,124,64},
    {250,60,66,65,124,64},
    {251,62,65,66,124,64},
    {252,61,64,64,125,64},
    {253,0,28,34,161,252},
    {254,254,40,68,68,56},
    {255,0,29,32,160,253},
    {7680,30,69,165,69,30},
    {10240,0,0,0,0,0},
    {10495,85,85,0,85,85},
    {57376,0,0,0,0,0},
    {57377,0,0,92,0,0},
    {57378,0,12,0,12,0},
    {57379,40,124,40,124,40},
    {57380,72,84,124,84,36},
    {57381,76,44,16,104,100},
    {57382,40,84,88,32,80},
    {57383,0,0,44,28,0},
    {57384,0,56,68,0,0},
    {57385,0,0,68,56,0},
    {57386,40,16,124,16,40},
    {57387,16,16,124,16,16},
    {57388,0,0,176,112,0},
    {57389,16,16,16,16,16},
    {57390,0,96,96,0,0},
    {57391,64,32,16,8,4},
    {57392,56,68,68,56,0},
    {57393,0,8,124,0,0},
    {57394,100,84,84,84,72},
    {57395,68,68,84,92,36},
    {57396,28,16,16,124,16},
    {57397,92,84,84,84,36},
    {57398,56,84,84,84,32},
    {57399,4,4,100,20,12},
    {57400,40,84,84,84,40},
    {57401,8,84,84,84,56},
    {57402,0,108,108,0,0},
    {57403,0,168,104,0,0},
    {57404,8,20,34,0,0},
    {57405,40,40,40,40,40},
    {57406,0,0,68,40,16},
    {57407,0,4,84,20,12},
    {57408,56,68,92,84,88},
    {57409,120,20,20,20,120},
    {57410,124,84,84,84,40},
    {57411,56,68,68,68,40},
    {57412,124,68,68,68,56},
    {57413,124,84,84,84,68},
    {57414,124,20,20,20,4},
    {57415,56,68,84,84,116},
    {57416,124,16,16,16,124},
    {57417,0,68,124,68,0},
    {57418,32,64,64,64,60},
    {57419,124,16,16,40,68},
    {57420,124,64,64,64,64},
    {57421,124,8,48,8,124},
    {57422,124,8,16,32,124},
    {57423,56,68,68,68,56},
    {57424,124,20,20,20,8},
    {57425,56,68,84,36,88},
    {57426,124,20,20,52,72},
    {57427,72,84,84,84,36},
    {57428,4,4,124,4,4},
    {57429,60,64,64,64,60},
    {57430,28,32,64,32,28},
    {57431,124,32,24,32,124},
    {57432,68,40,16,40,68},
    {57433,12,16,96,16,12},
    {57434,68,100,84,76,68},
    {57435,0,124,68,68,0},
    {57436,4,8,16,32,64},
    {57437,0,68,68,124,0},
    {57438,16,8,4,8,16},
    {57439,128,128,128,128,128},
    {57440,0,7,11,0,0},
    {57441,120,20,20,20,120},
    {57442,124,84,84,84,40},
    {57443,56,68,68,68,40},
    {57444,124,68,68,68,56},
    {57445,124,84,84,84,68},
    {57446,124,20,20,20,4},
    {57447,56,68,84,84,116},
    {57448,124,16,16,16,124},
    {57449,0,68,124,68,0},
    {57450,32,64,64,64,60},
    {57451,124,16,16,40,68},
    {57452,124,64,64,64,64},
    {57453,124,8,48,8,124},
    {57454,124,8,16,32,124},
    {57455,56,68,68,68,56},
    {57456,124,20,20,20,8},
    {57457,56,68,84,36,88},
    {57458,124,20,20,52,72},
    {57459,72,84,84,84,36},
    {57460,4,4,124,4,4},
    {57461,60,64,64,64,60},
    {57462,28,32,64,32,28},
    {57463,124,32,24,32,124},
    {57464,68,40,16,40,68},
    {57465,12,16,96,16,12},
    {57466,68,100,84,76,68},
    {57467,0,16,108,68,68},
    {57468,0,0,108,0,0},
    {57469,0,68,68,108,16},
    {57470,8,4,8,8,4},
    {57471,84,40,84,40,84},
    {65103,64,128,64,192,64},
    {65533,126,251,173,243,126}
};

#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
#include <jsct/JsClipboardTricks.h>
#if 0
EM_ASYNC_JS(void, copyClip, (const char* str), {
    document.getElementById("clipping").focus();
    const rtn = await navigator.clipboard.writeText(UTF8ToString(str));
    document.getElementById("canvas").focus();
});

EM_ASYNC_JS(char*, pasteClip, (), {
    document.getElementById("clipping").focus();
    const str = await navigator.clipboard.readText();
    document.getElementById("canvas").focus();
    const size = lengthBytesUTF8(str) + 1;
    const rtn = _malloc(size);
    stringToUTF8(str, rtn, size);
    return rtn;
});
#endif
#else
static std::string webClip;
#endif
#endif

std::string GetClipboardTextX()
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    return JsClipboard_getClipText();//pasteClip();
#else
    return webClip;
#endif
#else
    return GetClipboardText();
#endif
}

void SetClipboardTextX(const std::string& text)
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    //copyClip(text.c_str());
    JsClipboard_SetClipboardText(text.c_str());
#else
    webClip = text;
#endif
#else
    SetClipboardText(text.c_str());
#endif
}

bool isClipboardPaste()
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    return JsClipboard_hasClipText();
#else
    return false;
#endif
#else
    return false;
#endif
}

inline bool getFontPixel(uint32_t c, unsigned x, unsigned y)
{
    if (c > 0xffff) c = '?';
    const FontCharInfo* info = &fontRom['?' - ' '];
    for(const auto& fci : fontRom) {
        if(fci.codepoint == c) {
            info = &fci;
            break;
        }
    }
    uint8_t data = info->data[x];
    return (data & (1u << y)) != 0;
}

void drawChar(Image& image, uint32_t c, int xPos, int yPos, Color col)
{
    for (auto y = 0; y < 8; ++y) {
        for (auto x = 0; x < 5; ++x) {
            if (getFontPixel(c, x, y)) {
                ImageDrawPixel(&image, xPos + x, yPos + y, col);
            }
        }
    }
}

void CenterWindow(int width, int height)
{
    auto monitor = GetCurrentMonitor();
    SetWindowPosition((GetMonitorWidth(monitor) - width) / 2, (GetMonitorHeight(monitor) - height) / 2);
}

void LogHandler(int msgType, const char *text, va_list args)
{
    static char buffer[4096];
#ifndef PLATFORM_WEB
    static std::ofstream ofs((fs::path(dataPath())/"logfile.txt").string().c_str());
    ofs << date::format("[%FT%TZ]", std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
    switch (msgType)
    {
        case LOG_INFO: ofs << "[INFO] : "; break;
        case LOG_ERROR: ofs << "[ERROR]: "; break;
        case LOG_WARNING: ofs << "[WARN] : "; break;
        case LOG_DEBUG: ofs << "[DEBUG]: "; break;
        default: break;
    }
#endif
    vsnprintf(buffer, 4095, text, args);
#ifndef PLATFORM_WEB
    ofs << buffer << std::endl;
#endif
    emu::Logger::log(emu::Logger::eHOST, 0, {0,0}, buffer);
}

std::atomic_uint8_t g_soundTimer{0};
std::atomic_int g_frameBoost{1};

class Cadmium : public emu::EmuHostEx
{
public:
    using ExecMode = emu::IChip8Emulator::ExecMode;
    using CpuState = emu::IChip8Emulator::CpuState;
    enum MemFlags { eNONE = 0, eBREAKPOINT = 1, eWATCHPOINT = 2 };
    enum MainView { eVIDEO, eDEBUGGER, eEDITOR, eTRACELOG, eSETTINGS, eROM_SELECTOR, eROM_EXPORT, eLIBRARY };
    enum EmulationMode { eCOSMAC_VIP_CHIP8, eGENERIC_CHIP8 };
    enum VideoRenderMode { eFAST, eHIRES };
    enum FileBrowserMode { eLOAD, eSAVE, eWEB_SAVE };
    static constexpr int MIN_SCREEN_WIDTH = 640; // 512;
    static constexpr int MIN_SCREEN_HEIGHT = 480; // 192*2+36;
    explicit Cadmium(CadmiumConfiguration& cfg, emu::Properties& props)
        : emu::EmuHostEx(cfg)
        , _audioBuffer(44100)
        , _screenWidth(MIN_SCREEN_WIDTH)
        , _screenHeight(MIN_SCREEN_HEIGHT)
#ifndef PLATFORM_WEB
        , _editor(_threadPool)
#endif
    {
        SetTraceLogCallback(LogHandler);

    #ifdef RESIZABLE_GUI
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    #else
        // SetConfigFlags(FLAG_VSYNC_HINT);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        _windowInvisible = true;
        if (cfg.scaleMode) {
            _scaleMode = cfg.scaleMode;
        }
        if (!_scaleMode) {
            _scaleMode = (GetMonitorWidth(_currentMonitor) > 1680 || GetWindowScaleDPI().x > 1.0f) ? 2 : 1;
        }
    #endif

        InitWindow(_screenWidth * _scaleMode, _screenHeight * _scaleMode, "Cadmium - A CHIP-8 variant environment");
        SetMouseScale(1.0f/_scaleMode, 1.0f/_scaleMode);
        auto winPos = GetWindowPosition();
        if (cfg.windowPosX != 0xFFFF) {
            winPos.x = cfg.windowPosX;
        }
        if (cfg.windowPosY != 0xFFFF) {
            winPos.y = cfg.windowPosY;
        }
        SetWindowPosition(winPos.x, winPos.y);
        _currentMonitor = GetCurrentMonitor();
#ifdef RESIZABLE_GUI
        if(GetMonitorWidth(_curentMonitor) > 1680 || GetWindowScaleDPI().x > 1.0f) {
            SetWindowSize(_screenWidth * 2, _screenHeight * 2);
            CenterWindow(_screenWidth * 2, _screenHeight * 2);
        }
#else
        auto scale2d = GetWindowScaleDPI();
        auto monitorResolutionX = GetMonitorWidth(_currentMonitor);
        auto monitorResolutionY = GetMonitorHeight(_currentMonitor);
        TRACELOG(LOG_INFO, fmt::format("WindowScaleDPI: {}x{}", scale2d.x, scale2d.y).c_str());
#endif
        _textureScaler = std::make_unique<TextureScaler>(MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
        _textureScaler->setOutputSize(_screenWidth * _scaleMode, _screenHeight * _scaleMode);

        SetExitKey(0);

        _instance = this;
        InitAudioDevice();
        SetAudioStreamBufferSizeDefault(1470);
        _audioStream = LoadAudioStream(44100, 16, 1);
        SetAudioStreamCallback(_audioStream, Cadmium::audioInputCallback);
        PlayAudioStream(_audioStream);
        SetTargetFPS(60);

        //_renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
        //if (GetWindowScaleDPI().x != std::trunc(GetWindowScaleDPI().x))
        //    SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_BILINEAR);
        //else
        //    SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);

#ifdef PLATFORM_WEB
        const char* scanLineShader = R"(
#version 100

precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// NOTE: Add here your custom variables

float offset = 0.0;
float frequency = 450.0/3.0;

uniform float time;

void main()
{
/*
    // Scanlines method 1
    float tval = 0; //time
    vec2 uv = 0.5 + (fragTexCoord - 0.5)*(0.9 + 0.01*sin(0.5*tval));

    vec4 color = texture2D(texture0, fragTexCoord);

    color = clamp(color*0.5 + 0.5*color*color*1.2, 0.0, 1.0);
    color *= 0.5 + 0.5*16.0*uv.x*uv.y*(1.0 - uv.x)*(1.0 - uv.y);
    color *= vec4(0.8, 1.0, 0.7, 1);
    color *= 0.9 + 0.1*sin(10.0*tval + uv.y*1000.0);
    color *= 0.97 + 0.03*sin(110.0*tval);

    fragColor = color;
*/
    // Scanlines method 2
    float globalPos = (fragTexCoord.y + offset) * frequency;
    float wavePos = cos((fract(globalPos) - 0.5)*3.14);

    vec4 color = texture2D(texture0, fragTexCoord);

    gl_FragColor = mix(vec4(0.0, 0.3, 0.0, 0.0), color, wavePos);
}
)";
#else
        const char* scanLineShader = R"(
#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables
vec2 res=vec2(128.0/1.0,128.0/1.0);
float hardScan=-10.0;
float hardPix=-2.0;
vec2 warp=vec2(1.0/64.0,1.0/24.0);
float shape=3.0;
float overdrive=1.25;

float ToLinear1(float c){return(c<=0.04045)?c/12.92:pow((c+0.055)/1.055,2.4);}
vec3 ToLinear(vec3 c){return vec3(ToLinear1(c.r),ToLinear1(c.g),ToLinear1(c.b));}

// Linear to sRGB.
// Assuing using sRGB typed textures this should not be needed.
float ToSrgb1(float c){return(c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);}
vec3 ToSrgb(vec3 c){return vec3(ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));}

// Testing only, something to help generate a dark signal for bloom test.
// Set to zero, or remove Test() if using this shader.
#if 1
 vec3 Test(vec3 c){return c*(1.0/64.0)+c*c;}
#else
 vec3 Test(vec3 c){return c;}
#endif

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
vec3 Fetch(vec2 pos,vec2 off){
  pos=floor(pos*res+off)/res;
  if(max(abs(pos.x-0.5),abs(pos.y-0.5))>0.5)return vec3(0.0,0.0,0.0);
    return Test(ToLinear(texture(texture0,vec2(pos.x,pos.y),-16.0).rgb));}

// Distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos){pos=pos*res;return -((pos-floor(pos))-vec2(0.5));}

// Try different filter kernels.
float Gaus(float pos,float scale){return exp2(scale*pow(abs(pos),shape));}

// 3-tap Gaussian filter along horz line.
vec3 Horz3(vec2 pos,float off){
  vec3 b=Fetch(pos,vec2(-1.0,off));
  vec3 c=Fetch(pos,vec2( 0.0,off));
  vec3 d=Fetch(pos,vec2( 1.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wb=Gaus(dst-1.0,scale);
  float wc=Gaus(dst+0.0,scale);
  float wd=Gaus(dst+1.0,scale);
  // Return filtered sample.
  return (b*wb+c*wc+d*wd)/(wb+wc+wd);}

// 5-tap Gaussian filter along horz line.
vec3 Horz5(vec2 pos,float off){
  vec3 a=Fetch(pos,vec2(-2.0,off));
  vec3 b=Fetch(pos,vec2(-1.0,off));
  vec3 c=Fetch(pos,vec2( 0.0,off));
  vec3 d=Fetch(pos,vec2( 1.0,off));
  vec3 e=Fetch(pos,vec2( 2.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wa=Gaus(dst-2.0,scale);
  float wb=Gaus(dst-1.0,scale);
  float wc=Gaus(dst+0.0,scale);
  float wd=Gaus(dst+1.0,scale);
  float we=Gaus(dst+2.0,scale);
  // Return filtered sample.
  return (a*wa+b*wb+c*wc+d*wd+e*we)/(wa+wb+wc+wd+we);}

// 7-tap Gaussian filter along horz line.
vec3 Horz7(vec2 pos,float off){
  vec3 a=Fetch(pos,vec2(-3.0,off));
  vec3 b=Fetch(pos,vec2(-2.0,off));
  vec3 c=Fetch(pos,vec2(-1.0,off));
  vec3 d=Fetch(pos,vec2( 0.0,off));
  vec3 e=Fetch(pos,vec2( 1.0,off));
  vec3 f=Fetch(pos,vec2( 2.0,off));
  vec3 g=Fetch(pos,vec2( 3.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wa=Gaus(dst-3.0,scale);
  float wb=Gaus(dst-2.0,scale);
  float wc=Gaus(dst-1.0,scale);
  float wd=Gaus(dst+0.0,scale);
  float we=Gaus(dst+1.0,scale);
  float wf=Gaus(dst+2.0,scale);
  float wg=Gaus(dst+3.0,scale);
  // Return filtered sample.
  return (a*wa+b*wb+c*wc+d*wd+e*we+f*wf+g*wg)/(wa+wb+wc+wd+we+wf+wg);}

// Return scanline weight.
float Scan(vec2 pos,float off){
  float dst=Dist(pos).y;
  return Gaus(dst+off,hardScan);}

// Allow nearest three lines to effect pixel.
vec3 Tri(vec2 pos){
  vec3 a=Horz5(pos,-2.0);
  vec3 b=Horz7(pos,-1.0);
  vec3 c=Horz7(pos, 0.0);
  vec3 d=Horz7(pos, 1.0);
  vec3 e=Horz5(pos, 2.0);
  float wa=Scan(pos,-2.0);
  float wb=Scan(pos,-1.0);
  float wc=Scan(pos, 0.0);
  float wd=Scan(pos, 1.0);
  float we=Scan(pos, 2.0);
  return (a*wa+b*wb+c*wc+d*wd+e*we)*overdrive;}

// Distortion of scanlines, and end of screen alpha.
vec2 Warp(vec2 pos){
  pos=pos*2.0-1.0;
  pos*=vec2(1.0+(pos.y*pos.y)*warp.x,1.0+(pos.x*pos.x)*warp.y);
  return pos*0.5+0.5;}

// Draw dividing bars.
float Bar(float pos,float bar){pos-=bar;return pos*pos<4.0?0.0:1.0;}

// Entry.
void main(){
  vec2 pos=Warp(fragTexCoord.xy/res.xy);
  finalColor.rgb=Tri(pos) * 1.5;//*Mask(fragTexCoord.xy);
  finalColor.a=1.0;
  finalColor.rgb=ToSrgb(finalColor.rgb);
}

/*
// NOTE: Render size values must be passed from code
const float renderWidth = 800;
const float renderHeight = 2048;
float offset = 0.0;

uniform float time;

void main()
{
    float frequency = renderHeight/3.0;
*/
/*
    // Scanlines method 1
    float tval = 0; //time
    vec2 uv = 0.5 + (fragTexCoord - 0.5)*(0.9 + 0.01*sin(0.5*tval));

    vec4 color = texture(texture0, fragTexCoord);

    color = clamp(color*0.5 + 0.5*color*color*1.2, 0.0, 1.0);
    color *= 0.5 + 0.5*16.0*uv.x*uv.y*(1.0 - uv.x)*(1.0 - uv.y);
    color *= vec4(0.8, 1.0, 0.7, 1);
    color *= 0.9 + 0.1*sin(10.0*tval + uv.y*1000.0);
    color *= 0.97 + 0.03*sin(110.0*tval);

    fragColor = color;
*/
/*
    // Scanlines method 2
    float globalPos = (fragTexCoord.y + offset) * frequency;
    float wavePos = cos((fract(globalPos) - 0.5)*3.14);

    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);

    finalColor = mix(vec4(0.0, 0.3, 0.0, 0.0), texelColor, wavePos);
}*/
)";
#endif

        _scanLineShader = LoadShaderFromMemory(NULL, scanLineShader);

//        _styleManager.setDefaultTheme();
        _styleManager.addTheme("dark", 235.0f, 0.1, false);

        _microFont = LoadImage("micro-font.png");
        generateFont();
        if(props) {
            if (props.palette().empty())
                setPalette(_defaultPalette);
            else
                setPalette(props.palette());
        }
        else
            _mainView = eSETTINGS;
        updateEmulatorOptions(props);
        Cadmium::whenEmuChanged(*_chipEmu);
        _debugger.updateCore(_chipEmu.get());
        _screen = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
        _screenTexture = LoadTextureFromImage(_screen);
        _crt = GenImageColor(256,512,BLACK);
        _crtTexture = LoadTextureFromImage(_crt);
        _screenShot = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
        _screenShotTexture = LoadTextureFromImage(_screen);
        SetTextureFilter(_crtTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(_screenShotTexture, TEXTURE_FILTER_POINT);
        _titleImage = LoadImage("cadmium-title.png");
        _keyboardOverlay = LoadRenderTexture(40,40);
        _chipEmu->reset();
        std::string versionStr(CADMIUM_VERSION);
        drawMicroText(_titleImage, "v" CADMIUM_VERSION, 91u - std::strlen("v" CADMIUM_VERSION)*4, 6, WHITE);
        if(!versionStr.empty() && (versionStr.back() & 1))
            drawMicroText(_titleImage, "WIP", 38, 53, WHITE);
        std::string buildDate = __DATE__;
        auto dateText = buildDate.substr(0, 3);
        bool shortDate = (buildDate[4] == ' ');
        drawMicroText(_titleImage, buildDate.substr(9), 83, 53, WHITE);
        drawMicroText(_titleImage, buildDate.substr(4,2), 75, 52, WHITE);
        drawMicroText(_titleImage, buildDate.substr(0,3), shortDate ? 67 : 63, 53, WHITE);
        //ImageColorReplace(&titleImage, {0,0,0,255}, {0x00,0x22,0x2b,0xff});
        ImageColorReplace(&_titleImage, {0,0,0,255}, {0x1a,0x1c,0x2c,0xff});
        ImageColorReplace(&_titleImage, {255,255,255,255}, {0x51,0xbf,0xd3,0xff});
        _icon = GenImageColor(64,64,{0,0,0,0});
        ImageDraw(&_icon, _titleImage, {34,2,60,60}, {2,2,60,60}, WHITE);
#ifndef __APPLE__
        SetWindowIcon(_icon);
#endif
        _titleTexture = LoadTextureFromImage(_titleImage);
        if(_currentDirectory.empty())
            _currentDirectory = _librarian.currentDirectory();
        else
            _librarian.fetchDir(_currentDirectory);

        updateResolution();

        _styleManager.setTheme(0);

        // SWEETIE-16:
        // {0x1a1c2c, 0xf4f4f4, 0x94b0c2, 0x333c57, 0xef7d57, 0xa7f070, 0x3b5dc9, 0xffcd75, 0xb13e53, 0x38b764, 0x29366f, 0x566c86, 0x41a6f6, 0x73eff7, 0x5d275d, 0x257179}
        // PICO-8:
        // {0x000000, 0xfff1e8, 0xc2c3c7, 0x5f574f, 0xff004d, 0x00e436, 0x29adff, 0xffec27, 0xab5236, 0x008751, 0x1d2b53, 0xffa300, 0xff77a8, 0xffccaa, 0x7e2553, 0x83769c}
        // C64:
        // {0x000000, 0xffffff, 0xadadad, 0x626262, 0xa1683c, 0x9ae29b, 0x887ecb, 0xc9d487, 0x9f4e44, 0x5cab5e, 0x50459b, 0x6d5412, 0xcb7e75, 0x6abfc6, 0xa057a3, 0x898989}
        // Intellivision:
        // {0x0c0005, 0xfffcff, 0xa7a8a8, 0x3c5800, 0xff3e00, 0x6ccd30, 0x002dff, 0xfaea27, 0xffa600, 0x00a720, 0xbd95ff, 0xc9d464, 0xff3276, 0x5acbff, 0xc81a7d, 0x00780f}
        // CGA
        // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff5555, 0x55ff55, 0x5555ff, 0xffff55, 0xaa0000, 0x00aa00, 0x0000aa, 0xaa5500, 0xff55ff, 0x55ffff, 0xaa00aa, 0x00aaaa}
        // Silicon-8 1.0
        // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x880000, 0x008800, 0x000088, 0x888800, 0xff00ff, 0x00ffff, 0x880088, 0x008888}
        // Macintosh II
        // {0x000000, 0xffffff, 0xb9b9b9, 0x454545, 0xdc0000, 0x00a800, 0x0000ca, 0xffff00, 0xff6500, 0x006500, 0x360097, 0x976536, 0xff0097, 0x0097ff, 0x653600, 0x868686}
        // IBM PCjr
        // {0x1c2536, 0xced9ed, 0x81899e, 0x030625, 0xe85685, 0x2cc64e, 0x0000e8, 0xa7c251, 0x9f2441, 0x077c35, 0x0e59f0, 0x4b7432, 0xc137ff, 0x0bc3a9, 0x6b03ca, 0x028566}
        // Daylight-16
        // {0x272223, 0xf2d3ac, 0xe7a76c, 0x6a422c, 0xb55b39, 0xb19e3f, 0x7a6977, 0xf8c65c, 0x996336, 0x606b31, 0x513a3d, 0xd58b39, 0xc28462, 0xb5c69a, 0x905b54, 0x878c87}
        // Soul of the Sea
        // {0x01141a, 0xcfbc95, 0x93a399, 0x2f4845, 0x92503f, 0x949576, 0x425961, 0x81784d, 0x703a28, 0x7a7e67, 0x203633, 0x605f33, 0x56452b, 0x467e73, 0x403521, 0x51675a}
/*
        setPalette({
            0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff,
            0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff,
            0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff,
            0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff
        });
*/
#ifdef PLATFORM_WEB
        JsClipboard_AddJsHook();
#else
        _cfgPath = (fs::path(dataPath())/"config.json").string();
        _volume = _volumeSlider = _cfg.volume;
        _styleManager.updateStyle(_cfg.guiHue, _cfg.guiSat, false);
#endif
        if(_volume > 1.0f)
            _volume = _volumeSlider = 1.0f;
        SetMasterVolume(_volume);
    }

    ~Cadmium() override
    {
        if(!_cfgPath.empty()) {
            _cfg.workingDirectory = _currentDirectory;
            _cfg.guiHue = _styleManager.getGuiHue();
            _cfg.guiSat = _styleManager.getGuiSaturation();
            saveConfig();
        }
        gui::UnloadGui();
#ifndef PLATFORM_WEB
        if (_database)
            _database.reset();
#endif
        _textureScaler.release();
        UnloadFont(_font);
        UnloadImage(_fontImage);
        UnloadImage(_microFont);
        //UnloadRenderTexture(_renderTexture);
        UnloadRenderTexture(_keyboardOverlay);
        UnloadImage(_titleImage);
        UnloadTexture(_titleTexture);
        UnloadTexture(_screenShotTexture);
        UnloadTexture(_crtTexture);
        UnloadTexture(_screenTexture);
        UnloadAudioStream(_audioStream);
        CloseAudioDevice();
        UnloadImage(_screenShot);
        UnloadImage(_crt);
        UnloadImage(_screen);
        UnloadImage(_icon);
        _instance = nullptr;
        CloseWindow();
    }

    void updateResolution()
    {
        static unsigned counter = 0;
        if (++counter > 120) {
            counter = 0;
            _currentMonitor = GetCurrentMonitor(); // surprisingly expensive call
        }
        if (_windowInvisible) {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
            _windowInvisible = false;
        }
        if (!_scaleMode || GetMonitorWidth(_currentMonitor) <= _screenWidth * _scaleMode) {
            _scaleMode = 1;
        }

#ifdef RESIZABLE_GUI
        static int resizeCount = 0;
        if (IsWindowResized()) {
            int width{0}, height{0};
            resizeCount++;
#if defined(PLATFORM_WEB)
            double devicePixelRatio = emscripten_get_device_pixel_ratio();
            width = GetScreenWidth() * devicePixelRatio;
            height = GetScreenHeight() * devicePixelRatio;
#else
            // TODO: glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
#endif
            TraceLog(LOG_INFO, "Window resized: %dx%d, fb: %dx%d", GetScreenWidth(), GetScreenHeight(), width, height);
        }

        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        SetMouseScale(1.0f/screenScale, 1.0f/screenScale);

        auto width = std::max(GetScreenWidth(), _screenWidth) / (_scaleBy2 ? 2 : 1);
        auto height = std::max(GetScreenHeight(), _screenHeight) / (_scaleBy2 ? 2 : 1);

        if(GetScreenWidth() < width || GetScreenHeight() < height)
            SetWindowSize(width, height);
        if(width != _screenWidth || height != _screenHeight) {
            UnloadRenderTexture(_renderTexture);
            _screenWidth = width;
            _screenHeight = height;
            _renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
            SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
        }
#else
        if(_screenHeight < MIN_SCREEN_HEIGHT ||_screenWidth < MIN_SCREEN_WIDTH || GetScreenWidth() != _screenWidth * _scaleMode) {
            //UnloadRenderTexture(_renderTexture);
            _screenWidth = MIN_SCREEN_WIDTH;
            _screenHeight = MIN_SCREEN_HEIGHT;
            //_renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
            //SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
            Vector2 scale2d{1.0f, 1.0f};// GetWindowScaleDPI();
            SetWindowSize(_screenWidth * _scaleMode / scale2d.x, _screenHeight * _scaleMode / scale2d.y);
            SetMouseScale(1.0f/_scaleMode*scale2d.x, 1.0f/_scaleMode*scale2d.y);
            _textureScaler->setOutputSize(_screenWidth * _scaleMode / scale2d.x, _screenHeight * _scaleMode / scale2d.y);
        }
#endif
    }

    bool isHeadless() const override { return false; }

    void drawMicroText(Image& dest, const std::string& text, int x, int y, Color tint) const
    {
        for(auto c : text) {
            if (static_cast<uint8_t>(c) < 128)
                ImageDraw(&dest, _microFont, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
            x += 4;
        }
    }

    void drawMicroText2(Image& dest, const std::string& text, int x, int y, Color tint) const
    {
        for(auto c : text) {
            if (static_cast<uint8_t>(c) < 128) {
                for (int yy = 0; yy < 6; ++yy) {
                    for (int xx = 0; xx < 4; ++xx) {
                        if (GetImageColor(_microFont, (c%32)*4.0f + xx, (c/32)*6.0f + yy).r > 128) {
                            ImageDrawPixel(&dest, (float)x + xx, (float)y + yy, tint);
                        }
                    }
                }
                //ImageDraw(&dest, _microFont, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
            }
            x += 4;
        }
    }

    static Cadmium* instance() { return _instance; }

    static void audioInputCallback(void *buffer, unsigned int frames)
    {
        if (auto* inst = Cadmium::instance()) {
            inst->renderAudio(static_cast<int16_t*>(buffer), frames);
        }
    }

    void renderAudio(int16_t *samples, unsigned int frames)
    {
        std::scoped_lock lock(_audioMutex);
        _audioCallbackAvgFrames = _audioCallbackAvgFrames ? (_audioCallbackAvgFrames + frames)/2u : frames;
        if(_chipEmu) {
            if(_chipEmu->execMode() == emu::GenericCpu::eRUNNING) {
                const auto len = _audioBuffer.read(samples, frames);
                if(!len) {
                    while(frames--) {
                        *samples++ = 0;
                    }
                    return;
                }
                frames -= len;
                samples += len;
                if(frames) {
                    _chipEmu->renderAudio(samples, frames, 44100);
                    frames = 0;
                }
            }
        }
        while(frames--) {
            *samples++ = 0;
        }
    }

    void pushAudio(int frames)
    {
        static int16_t sampleBuffer[44100];
        if(_chipEmu->execMode() == emu::IChip8Emulator::eRUNNING) {
            //if(_audioBuffer.dataAvailable() < _audioCallbackAvgFrames) ++frames;
            if(frames > _audioBuffer.spaceAvailable()) frames = _audioBuffer.spaceAvailable();
            _chipEmu->renderAudio(sampleBuffer, frames, 44100);
            _audioBuffer.write(sampleBuffer, frames);
        }
    }

    void vblank() override
    {
        if(_chipEmu)
            pushAudio(44100 / _chipEmu->frameRate());
    }

    int getKeyPressed() override
    {
#if 1
        static uint32_t instruction = 0;
        static int waitKeyUp = 0;
        static int keyId = 0;
        auto now = GetTime();
        for(int i = 0; i < 16; ++i)
            _keyScanTime[i] = now;
        if(waitKeyUp && instruction == _chipEmu->focussedExecutionUnit()->getPC()) {
            if(IsKeyUp(waitKeyUp)) {
                waitKeyUp = 0;
                instruction = 0;
                return keyId;
            }
            return -1;
        }
        waitKeyUp = 0;
        auto key = GetKeyPressed();
        if (!gui::IsSysKeyDown() && key) {
            for (int i = 0; i < 16; ++i) {
                if (key == _keyMapping[i]) {
                    instruction = _chipEmu->focussedExecutionUnit()->getPC();
                    waitKeyUp = key;
                    keyId = i + 1;
                    return 0;
                }
            }
        }
        return waitKeyUp ? -1 : 0;
#else
        //static uint32_t instruction = 0;
        //static int waitKeyUp = 0;
        //static int keyId = 0;
        auto now = GetTime();
        int keyId = -1;
        for(int i = 0; i < 16; ++i) {
            _keyScanTime[i] = now;
            if(IsKeyReleased(_keyMapping[i]))
                keyId = i;
        }
        return keyId;
        /*
        if(waitKeyUp && instruction == _chipEmu->getPC()) {
            if(IsKeyUp(waitKeyUp)) {
                waitKeyUp = 0;
                instruction = 0;
                return keyId;
            }
            return -1;
        }
        waitKeyUp = 0;
        auto key = GetKeyPressed();
        if (!gui::IsSysKeyDown() && key) {
            for (int i = 0; i < 16; ++i) {
                if (key == _keyMapping[i]) {
                    instruction = _chipEmu->getPC();
                    waitKeyUp = key;
                    keyId = i + 1;
                    return 0;
                }
            }
        }
        return waitKeyUp ? -1 : 0;
        */
#endif
    }

    bool isKeyDown(uint8_t key) override
    {
        _keyScanTime[key & 0xF] = GetTime();
        return !gui::IsSysKeyDown() && IsKeyDown(_keyMapping[key & 0xF]);
    }

    const std::array<bool, 16>& getKeyStates() const override
    {
        return _keyMatrix;
    }

    void updateKeyboardOverlay()
    {
        static const char* keys = "1\0" "2\0" "3\0" "C\0" "4\0" "5\0" "6\0" "D\0" "7\0" "8\0" "9\0" "E\0" "A\0" "0\0" "B\0" "F\0";
        BeginTextureMode(_keyboardOverlay);
        ClearBackground({0,0,0,0});
        auto now = GetTime();
        for(int i = 0; i < 4; ++i) {
            for(int j = 0; j < 4; ++j) {
                DrawRectangleRec({j*10.0f, i*10.0f, 9.0f, 9.0f}, now - _keyScanTime[_keyPosition[i*4+j]] < 0.2 ? WHITE : GRAY);
                if(IsKeyDown(_keyMapping[_keyPosition[i*4+j]])) {
                    DrawRectangleLines(j*10.0f, i*10.0f, 9.0f, 9.0f, BLUE);
                }
                DrawTextEx(_font, &keys[i*8+j*2], {j*10.0f + 2.0f, i*10.0f + 1.0f}, 8.0f, 0, BLACK);
            }
        }
        EndTextureMode();
    }

    static Vector3 rgbToXyz(Color c)
    {
        float x, y, z, r, g, b;

        r = c.r / 255.0f; g = c.g / 255.0f; b = c.b / 255.0f;

        if (r > 0.04045f)
            r = std::pow(((r + 0.055f) / 1.055f), 2.4f);
        else r /= 12.92;

        if (g > 0.04045f)
            g = std::pow(((g + 0.055f) / 1.055f), 2.4f);
        else g /= 12.92;

        if (b > 0.04045f)
            b = std::pow(((b + 0.055f) / 1.055f), 2.4f);
        else b /= 12.92f;

        r *= 100; g *= 100; b *= 100;

        x = r * 0.4124f + g * 0.3576f + b * 0.1805f;
        y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
        z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

        return {x, y, z};
    }

    static Vector3 xyzToCIELAB(Vector3 c)
    {
        float x, y, z, l, a, b;
        const float refX = 95.047f, refY = 100.0f, refZ = 108.883f;

        x = c.x / refX; y = c.y / refY; z = c.z / refZ;

        if (x > 0.008856f)
            x = powf(x, 1 / 3.0f);
        else x = (7.787f * x) + (16.0f / 116.0f);

        if (y > 0.008856f)
            y = powf(y, 1 / 3.0);
        else y = (7.787f * y) + (16.0f / 116.0f);

        if (z > 0.008856f)
            z = powf(z, 1 / 3.0);
        else z = (7.787f * z) + (16.0f / 116.0f);

        l = 116 * y - 16;
        a = 500 * (x - y);
        b = 200 * (y - z);

        return {l, a, b};
    }

    static float getColorDeltaE(Color c1, Color c2)
    {
        Vector3 xyzC1 = rgbToXyz(c1), xyzC2 = rgbToXyz(c2);
        Vector3 labC1 = xyzToCIELAB(xyzC1), labC2 = xyzToCIELAB(xyzC2);
        return Vector3Distance(labC1, labC2);;
    }

    static uint32_t rgb332To888(uint8_t c)
    {
        static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xff};
        static uint8_t b2[] = {0, 0x60, 0xA0, 0xff};
        return (b3[(c & 0xe0) >> 5] << 16) | (b3[(c & 0x1c) >> 2] << 8) | (b2[c & 3]);
    }

    void updatePalette(const std::array<uint8_t,16>& palette) override
    {
        /*
         * TODO: Fix this
        if(!_customPalette) {
            for (int i = 0; i < palette.size(); ++i) {
                _colorPalette[i] = ((rgb332To888(palette[i]) << 8) | 0xff);
            }
            _updateScreen = true;
        }*/
    }

    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override
    {
        setPalette(palette, offset);
    }

    void generateFont()
    {
        int imageWidth = 256;
        int imageHeight = 256;
        _fontImage = GenImageColor(imageWidth, imageHeight, {0, 0, 0, 0});
        int glyphCount = 0;
        std::vector<Rectangle> rectangles;
        std::vector<GlyphInfo> glyphs;
        float glyphX = 0, glyphY = 0;
        for(const auto& fci : fontRom) {
            auto c = fci.codepoint;
            if (glyphX + 6 > imageWidth) {
                glyphX = 0;
                glyphY += 8;
            }
            drawChar(_fontImage, c, glyphX, glyphY, WHITE);
            rectangles.push_back(Rectangle{glyphX, glyphY, 6.0f, 8.0f});
            glyphs.push_back(GlyphInfo{c, 0, 0, 6});
            glyphX += 6;
            ++glyphCount;
        }
        float badgeX = 0;
        float badgeY = glyphX < 1 ? glyphY : glyphY + 8;
        int badgeCount = 0;
        struct BadgeInfo {
            std::string text;
            Color badgeColor, textColor;
        };
        std::vector<BadgeInfo> badges;
        auto textColor = DARKGRAY;
        for(const auto& [name, info] : _cores) {
            //std::cout << toOptionName(name) << std::endl;
            //coresAvailable += fmt::format("        {} - {}\n", toOptionName(name), info->description);
            //presetsDescription += fmt::format("        {}:\n", info->description);
            for(size_t i = 0; i < info->numberOfVariants(); ++i) {
                std::string presetName;
                if(info->prefix().empty())
                    presetName = toOptionName(info->variantName(i));
                else
                    presetName = toOptionName(info->prefix() + '-' + info->variantName(i));
                badges.push_back(BadgeInfo{presetName, {0x00, 0xE0, 0x00, 0xFF}, textColor});
            }
        }
        badges.push_back(BadgeInfo{"GENERIC-CHIP-8", {0xE0, 0xC0, 0x00, 0xFF}, textColor});
        badges.push_back(BadgeInfo{"???", {0xE0, 0x40, 0x40, 0xFF}, LIGHTGRAY});
        badges.push_back(BadgeInfo{"NEW", {0x00, 0xC0, 0xE0, 0xFF}, textColor});
        for(uint16_t i = 0; i < badges.size(); ++i) {
            auto& [label, badgeCol, textCol] = badges[i];
            float width = label.length() * 4 + 3;
            if (badgeX + width > imageWidth) {
                badgeX = 0;
                badgeY += 8;
            }
            ImageDrawRectangle(&_fontImage, badgeX, badgeY+1, width, 5,badgeCol);
            ImageDrawRectangle(&_fontImage, badgeX+1, badgeY, width-2, 7, badgeCol);
            drawMicroText2(_fontImage, toUpper(label), badgeX + 2, badgeY + 1, textCol);
            rectangles.push_back(Rectangle{badgeX, badgeY, width, 7});
            glyphs.push_back(GlyphInfo{0xE100 + i, 0, 0, (int)width + 1});
            std::string badgeUtf8;
            fs::detail::appendUTF8(badgeUtf8, 0xE100 + i);
            _badges[toLower(label)] = badgeUtf8;
            badgeX += width + 1;
        }
#if !defined(NDEBUG) // && defined(EXPORT_FONT)
        ExportImage(_fontImage, "Test.png");
        {
            std::ofstream fos("font.txt");
            for (uint8_t c = 32; c < 128; ++c) {
                fos << "char: " << fmt::format("0x{:04x}", c) << " " << c << std::endl;
                for(int y = 0; y < 8; ++y) {
                    for(int x = 0; x < 5; ++x) {
                        fos << (getFontPixel(c, x, y) ? "#" : "-");
                    }
                    fos << "-" << std::endl;
                }
            }
            fos << std::endl;
        }
#endif
        _font.baseSize = 8;
        _font.glyphCount = glyphs.size();
        _font.texture = LoadTextureFromImage(_fontImage);
        _font.recs = static_cast<Rectangle*>(std::calloc(_font.glyphCount, sizeof(Rectangle)));
        _font.glyphs = static_cast<GlyphInfo*>(std::calloc(_font.glyphCount, sizeof(GlyphInfo)));
        std::ranges::copy(rectangles, _font.recs);
        std::ranges::copy(glyphs, _font.glyphs);
        GuiSetFont(_font);
    }

    bool screenChanged() const
    {
        return _updateScreen;
    }

    int getFrameBoost() const { return _chipEmu->supportsFrameBoost() && _frameBoost > 0 ? _frameBoost : 1; }

    void updateScreen() override
    {
        auto* pixel = (uint32_t*)_screen.data;
        if(pixel) {
            const auto* screen = _chipEmu->getScreen();
            if (screen) {
                if (!_renderCrt) {
                    screen->convert(pixel, _screen.width, 255, nullptr);
                    UpdateTexture(_screenTexture, _screen.data);
                }
                else {
                }
            }
            else {
                // TraceLog(LOG_INFO, "Updating MC8 screen!");
                const auto* screenRgb = _chipEmu->getScreenRGBA();
                if(screenRgb) {
                    screenRgb->convert(pixel, _screen.width, _chipEmu->getScreenAlpha(), _chipEmu->getWorkRGBA());
                    UpdateTexture(_screenTexture, _screen.data);
                }
            }
        }
    }

    static void updateAndDrawFrame(void* self)
    {
        static_cast<Cadmium*>(self)->updateAndDraw();
    }

    void updateAndDraw()
    {
        static auto lastFrameTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(16);
        auto now = std::chrono::steady_clock::now();
        double deltaTC = std::chrono::duration<double>(now - lastFrameTime).count();
        lastFrameTime = now;
        float deltaT = GetFrameTime();

        updateResolution();

        _librarian.update(*_properties); // allows librarian to complete background tasks

        if (IsFileDropped()) {
            auto files = LoadDroppedFiles();
            if (files.count > 0) {
                //TraceLog(LOG_INFO, "About to load one of %d dropped files.", (int)files.count);
                loadRom(files.paths[0], LoadOptions::None);
            }
            UnloadDroppedFiles(files);
        }

        if(_mainView == eEDITOR) {
            _editor.update();
            if(!_editor.compiler().isError() && _editor.compiler().sha1() != _romSha1) {
                _romImage.assign(_editor.compiler().code(), _editor.compiler().code() + _editor.compiler().codeSize());
                _romSha1 = _editor.compiler().sha1();
                _debugger.updateOctoBreakpoints(_editor.compiler());
                reloadRom();
            }
        }

        for(uint8_t key = 0; key < 16; ++key) {
            _keyMatrix[key] = IsKeyDown(_keyMapping[key & 0xF]);
        }

        if(_chipEmu->execMode() != ExecMode::ePAUSED) {
            _partialFrameTime += GetFrameTime()*1000 * _chipEmu->frameRate();
            if(_partialFrameTime > 10000) {
                _fps.reset();
                _partialFrameTime = 1000;
            }
            if(_partialFrameTime >= 1000) {
                while (_partialFrameTime >= 1000) {
                    _partialFrameTime -= 1000;
                    for(int i = 0; i < getFrameBoost(); ++i) {
                        _chipEmu->executeFrame();
                        for(auto& unit : *_chipEmu) {
                            if(unit.isBreakpointTriggered())
                                _mainView = eDEBUGGER;
                        }
                    }
                    _fps.add(GetTime()*1000);
                }
            }
            if(_chipEmu->needsScreenUpdate())
                updateScreen();
            if(_showKeyMap)
                updateKeyboardOverlay();
        }

        static Stopwatch guiRenderTime;
        guiRenderTime.start();
        _screenOverlay = {0,0,0,0};
        BeginTextureMode(_textureScaler->getRenderTexture());
        drawGui();
        EndTextureMode();
        _textureScaler->updateIntermediateTexture();
        guiRenderTime.stop();
        _avgGuiRenderTime = guiRenderTime.getElapsedAvgString();

        BeginDrawing();
        {
            ClearBackground(CADMIUM_VERSION_DECIMAL & 1 ? Color{16,0,0,255} : BLACK);
            //ClearBackground(BLACK);
#ifdef RESIZABLE_GUI
            Vector2 guiOffset = {(GetScreenWidth() - _screenWidth*screenScale)/2.0f, (GetScreenHeight() - _screenHeight*screenScale)/2.0f};
            if(guiOffset.x < 0) guiOffset.x = 0;
            if(guiOffset.y < 0) guiOffset.y = 0;
            if (_scaleBy2) {
                drawScreen({_screenOverlay.x * 2, _screenOverlay.y * 2, _screenOverlay.width * 2, _screenOverlay.height * 2}, _screenScale);
                DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_renderTexture.texture.width * 2, (float)_renderTexture.texture.height * 2},
                               (Vector2){0, 0}, 0.0f, WHITE);
            }
            else {
                drawScreen(_screenOverlay, _screenScale);
                DrawTextureRec(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Vector2){0, 0}, WHITE);
            }
            //DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){guiOffset.x, guiOffset.y, (float)_renderTexture.texture.width * screenScale, (float)_renderTexture.texture.height * screenScale},
            //               (Vector2){0, 0}, 0.0f, WHITE);
#else
            if (_videoRenderMode == eHIRES && _screenOverlay.width > 0 && _screenOverlay.height > 0) {
                drawScreen({_screenOverlay.x * _scaleMode, _screenOverlay.y * _scaleMode, _screenOverlay.width * _scaleMode, _screenOverlay.height * _scaleMode}, _screenScale);
            }
            _textureScaler->draw(0, 0);
            //DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_renderTexture.texture.width * _scaleMode / scale2d.x, (float)_renderTexture.texture.height * _scaleMode / scale2d.y},
            //               (Vector2){0, 0}, 0.0f, WHITE);
#endif
#if 0
            int width{0}, height{0};
#if defined(PLATFORM_WEB)
            double devicePixelRatio = emscripten_get_device_pixel_ratio();
            width = GetScreenWidth() * devicePixelRatio;
            height = GetScreenHeight() * devicePixelRatio;
#else
            glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
#endif
            //TraceLog(LOG_INFO, "Window resized: %dx%d, fb: %dx%d", GetScreenWidth(), GetScreenHeight(), width, height);
            DrawText(TextFormat("Window resized: %dx%d, fb: %dx%d, rzc: %d", GetScreenWidth(), GetScreenHeight(), width, height, resizeCount), 10,30,10,GREEN);
#endif
            // DrawText(TextFormat("Res: %dx%d", GetMonitorWidth(_currentMonitor), GetMonitorHeight(_currentMonitor)), 10, 30, 10, GREEN);
            // DrawFPS(10,45);
        }
        EndDrawing();
    }


    void drawScreen(Rectangle dest, int gridScale)
    {
        const Color gridLineCol{40,40,40,255};
        int scrWidth = _chipEmu->getCurrentScreenWidth();
        //int scrHeight = crt ? 385 : (_chipEmu->isGenericEmulation() ? _chipEmu->getCurrentScreenHeight() : 128);
        int scrHeight = _chipEmu->getCurrentScreenHeight();
        auto videoScaleX = dest.width / scrWidth;
        auto videoScaleY = _chipEmu->getScreen() && _chipEmu->getScreen()->ratio() ? videoScaleX / _chipEmu->getScreen()->ratio() : videoScaleX;
        auto videoX = (dest.width - _chipEmu->getCurrentScreenWidth() * videoScaleX) / 2 + dest.x;
        auto videoY = (dest.height - _chipEmu->getCurrentScreenHeight() * videoScaleY) / 2 + dest.y;
        if(_chipEmu->getMaxScreenWidth() > 128)
            DrawRectangleRec(dest, {0,0,0,255});
        else
            DrawRectangleRec(dest, {0,12,24,255});

        //BeginShaderMode(_scanLineShader);
        DrawTexturePro(_screenTexture, {0, 0, (float)scrWidth, (float)scrHeight}, {videoX, videoY, scrWidth * videoScaleX, scrHeight * videoScaleY}, {0, 0}, 0, WHITE);
        //EndShaderMode();

        if (_grid) {
            for (short x = 0; x < scrWidth; ++x) {
                DrawRectangle(videoX + x * gridScale, videoY, 1, scrHeight * videoScaleY, gridLineCol);
            }
            if(_chipEmu->isGenericEmulation()) {
                for (short y = 0; y < scrHeight; ++y) {
                    DrawRectangle(videoX, videoY + y * gridScale, scrWidth * videoScaleX, 1, gridLineCol);
                }
            }
        }
        if(_showKeyMap) {
            DrawTexturePro(_keyboardOverlay.texture, {0, 0, 40, -40}, {videoX + scrWidth * videoScaleX - 40.0f, videoY + scrHeight * videoScaleY - 40.0f, 40.0f, 40.0f}, {0, 0}, 0.0f, {255, 255, 255, 128});
        }
        if(GetTime() < 5 && _romImage.empty()) {
            auto scale = dest.width / 128;
            auto offsetX = (dest.width - 60*scale) / 2;
            auto offsetY = (dest.height - 60*scale) / 2;
            DrawTexturePro(_titleTexture, {34, 2, 60, 60}, {dest.x + offsetX, dest.y + offsetY, 60*scale, 60*scale}, {0, 0}, 0, {255,255,255,uint8_t(GetTime()>4 ? 255.0f*(4.0f-GetTime()) : 255.0f)});
        }
#if 0
        rlSetBlendFactors(1, 0, 0x8006);
        rlSetBlendMode(RL_BLEND_CUSTOM);
        DrawRectangle(25,25,75,75, {0,0,0,0});
        rlSetBlendMode(RL_BLEND_ALPHA);
#endif
    }

    static bool iconButton(int iconId, bool isPressed = false, Color color = {3, 127, 161}, Color foreground = {0x51, 0xbf, 0xd3, 0xff})
    {
        using namespace gui;
        StyleManager::Scope guard;
        auto fg = guard.getStyle(Style::TEXT_COLOR_NORMAL);
        auto bg = guard.getStyle(Style::BASE_COLOR_NORMAL);
        if(isPressed) {
            guard.setStyle(Style::BASE_COLOR_NORMAL, fg);
            guard.setStyle(Style::TEXT_COLOR_NORMAL, bg);
        }
        //guard.setStyle(Style::TEXT_COLOR_NORMAL, foreground);
        gui::SetNextWidth(20);
        auto result = gui::Button(GuiIconText(iconId, ""));
        return result;
    }

    const std::vector<std::pair<uint32_t,std::string>>& disassembleNLinesBackwardsGeneric(uint32_t addr, int n)
    {
        static std::vector<std::pair<uint32_t,std::string>> disassembly;
        auto* rcb = dynamic_cast<emu::Chip8RealCoreBase*>(_chipEmu.get());
        if(rcb) {
            n *= 4;
            uint32_t start = n > addr ? 0 : addr - n;
            disassembly.clear();
            bool inIf = false;
            while (start < addr) {
                int bytes = 0;
                auto instruction = rcb->getBackendCpu().disassembleInstructionWithBytes(start, &bytes);
                disassembly.emplace_back(start, instruction);
                start += bytes;
            }
        }
        return disassembly;
    }

    void drawGui()
    {
        using namespace gui;
        ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
        Rectangle video;
        int gridScale = 4;
        static int64_t lastInstructionCount = 0;
        static int64_t lastFrameCount = 0;

        static std::chrono::steady_clock::time_point volumeClick{};

#ifdef RESIZABLE_GUI
        //auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        Vector2 mouseOffset = {-(GetScreenWidth() - _screenWidth * screenScale) / 2.0f, -(GetScreenHeight() - _screenHeight * screenScale) / 2.0f};
        if (mouseOffset.x > 0)
            mouseOffset.x = 0;
        if (mouseOffset.y > 0)
            mouseOffset.y = 0;
        BeginGui({}, &_renderTexture, mouseOffset, {(float)screenScale, (float)screenScale});
//        BeginGui({}, &_renderTexture, {0, 0}, {_scaleBy2 ? 2.0f : 1.0f, _scaleBy2 ? 2.0f : 1.0f});
#else
        BeginGui({}, &_textureScaler->getRenderTexture(), {0, 0}, {static_cast<float>(_scaleMode), static_cast<float>(_scaleMode)});
#endif
        {
            SetStyle(STATUSBAR, TEXT_PADDING, 4);
            SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
            SetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 0);
            SetStyle(SPINNER, TEXT_PADDING, 4);

            SetRowHeight(16);
            SetSpacing(0);
            auto instructionsThisUpdate = _chipEmu->cycles() - lastInstructionCount;
            auto framesThisUpdate = _chipEmu->frames() - lastFrameCount;
            if (_chipEmu->execMode() == emu::GenericCpu::eRUNNING) {
                _ipfAverage.add(instructionsThisUpdate);
                _frameTimeAverage_us.add(GetFrameTime() * 1000000);
                _frameDelta.add(framesThisUpdate);
            }
            auto ipfAvg = _ipfAverage.get();
            auto ftAvg_us = _frameTimeAverage_us.get();
            auto fdAvg = _frameDelta.get();
            auto ips = instructionsThisUpdate / GetFrameTime();

            auto ipsAvg = float(ipfAvg) * 1000000 / ftAvg_us;
            if (_mainView == eEDITOR) {
                StatusBar({{0.55f, fmt::format("UI:{}", _avgGuiRenderTime).c_str()}, {0.15f, fmt::format("{} byte", _editor.compiler().codeSize()).c_str()}, {0.1f, fmt::format("{}:{}", _editor.line(), _editor.column()).c_str()}, {0.15f, _variantName.c_str()}});
            }
            else if (_chipEmu->coreState() == emu::IEmulationCore::ECS_ERROR) {
                StatusBar({{0.55f, fmt::format("{}, UI:{}", _chipEmu->errorMessage(), _avgGuiRenderTime).c_str()}, {0.15f, formatUnit(ipsAvg, "IPS").c_str()}, {0.1f, formatUnit(_fps.getFps(), "FPS").c_str()}, {0.15f, _variantName.c_str()}});
            }
            else if (getFrameBoost() > 1) {
                StatusBar({{0.5f, fmt::format("Cycles: {}, UI:{}", _chipEmu->cycles(), _avgGuiRenderTime).c_str()}, {0.2f, formatUnit(ipsAvg, "IPS").c_str()}, {0.1f, formatUnit(_fps.getFps() * getFrameBoost(), "eFPS").c_str()}, {0.15f, _variantName.c_str()}});
            }
            else {
                if (_chipEmu->cycles() != _chipEmu->machineCycles()) {
                    StatusBar({{0.55f, fmt::format("Cycles: {}/{} [{}], UI:{}", _chipEmu->cycles(), _chipEmu->machineCycles(), _chipEmu->frames(), _avgGuiRenderTime).c_str()},
                               {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                               {0.1f, formatUnit(_fps.getFps(), "FPS").c_str()},
                               {0.15f, _variantName.c_str()}});
                }
                else {
                    StatusBar({{0.55f, fmt::format("Cycles: {} [{}], UI:{}", _chipEmu->cycles(), _chipEmu->frames(), _avgGuiRenderTime).c_str()},
                               {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                               //{0.15f, formatUnit((double)getFrameBoost() * GetFPS(), "FPS").c_str()},
                               {0.1f, formatUnit(_fps.getFps(), "FPS").c_str()},
                               {0.15f, _variantName.c_str()}});
                }
            }
            lastInstructionCount = _chipEmu->cycles();
            lastFrameCount = _chipEmu->frames();
            BeginColumns();
            {
                SetRowHeight(20);
                SetSpacing(0);
                SetNextWidth(20);
                static bool menuOpen = false;
                static bool aboutOpen = false;
                static Vector2 aboutScroll{};
                if (Button(GuiIconText(ICON_BURGER_MENU, "")))
                    menuOpen = true;
                if (menuOpen || (IsSysKeyDown() && (IsKeyDown(KEY_N) || IsKeyDown(KEY_O) || IsKeyDown(KEY_S) || IsKeyDown(KEY_K) || IsKeyDown(KEY_Q)))) {
#ifndef PLATFORM_WEB
                    Rectangle menuRect = {1, GetCurrentPos().y + 20, 110, 84};
#else
                    Rectangle menuRect = {1, GetCurrentPos().y + 20, 110, 69};
#endif
                    BeginPopup(menuRect, &menuOpen);
                    SetRowHeight(12);
                    Space(3);
                    if (LabelButton(" About Cadmium..."))
                        aboutOpen = true, aboutScroll = {0, 0}, menuOpen = false;
                    Space(3);
                    if (LabelButton(" New...  [^N]") || (IsSysKeyDown() && IsKeyPressed(KEY_N))) {
                        _mainView = eEDITOR;
                        menuOpen = false;
                        _editor.setText(": main\n    jump main");
                        _romName = "unnamed.8o";
                        _editor.setFilename("");
                        for(auto& unit : *_chipEmu) {
                            unit.removeAllBreakpoints();
                        }
                    }
                    if (LabelButton(" Open... [^O]") || (IsSysKeyDown() && IsKeyPressed(KEY_O))) {
#ifdef PLATFORM_WEB
                        loadFileWeb();
#else
                        _mainView = eROM_SELECTOR;
                        _librarian.fetchDir(_currentDirectory);
#endif
                        menuOpen = false;
                    }
                    if (LabelButton(" Save... [^S]") || (IsSysKeyDown() && IsKeyPressed(KEY_S))) {
                        _mainView = eROM_EXPORT;
#ifndef PLATFORM_WEB
                        _librarian.fetchDir(_currentDirectory);
#endif
                        menuOpen = false;
                    }
                    if (LabelButton(" Key Map [^M]") || (IsSysKeyDown() && IsKeyPressed(KEY_K))) {
                        _showKeyMap = !_showKeyMap;
                        menuOpen = false;
                    }
#ifndef PLATFORM_WEB
                    Space(3);
                    if (LabelButton(" Quit    [^Q]") || (IsSysKeyDown() && IsKeyPressed(KEY_Q)))
                        menuOpen = false, _shouldClose = true;
#endif
                    EndPopup();
                    if (IsKeyPressed(KEY_ESCAPE) || (IsMouseButtonPressed(0) && !CheckCollisionPointRec(GetMousePosition(), menuRect)))
                        menuOpen = false;
                }
                if (aboutOpen) {
                    aboutOpen = !BeginWindowBox({-1, -1, 460, 300}, "About Cadmium", &aboutOpen, WindowBoxFlags(WBF_MOVABLE | WBF_MODAL));
                    SetStyle(DEFAULT, BORDER_WIDTH, 0);
                    static size_t newlines = std::count_if(aboutText.begin(), aboutText.end(), [](char c) { return c ==        '\n'; });
                    BeginScrollPanel(-1, {0, 0, 445, newlines * 10.0f + 100}, &aboutScroll);
                    SetRowHeight(10);
                    DrawTextureRec(_titleTexture, {34, 2, 60, 60}, {aboutScroll.x + 8.0f, aboutScroll.y + 31.0f}, WHITE);
                    auto styleColor = GetStyle(LABEL, TEXT_COLOR_NORMAL);
                    SetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
                    Label("           Cadmium v" CADMIUM_VERSION);
                    SetStyle(LABEL, TEXT_COLOR_NORMAL, styleColor);
                    Space(4);
                    Label("           (c) 2022 by Steffen 'Gulrak' Schümann");
                    if (LabelButton("           https://github.com/gulrak/cadmium")) {
                        OpenURL("https://github.com/gulrak/cadmium");
                    }
                    Space(8);
                    std::istringstream iss(aboutText);
                    for (std::string line; std::getline(iss, line);) {
                        auto trimmedLine = trim(line);
                        if (startsWith(trimmedLine, "http")) {
                            if (LabelButton(line.c_str()))
                                OpenURL(trimmedLine.c_str());
                        }
                        else if (startsWith(line, "# ")) {
                            SetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
                            Label(line.substr(2).c_str());
                            SetStyle(LABEL, TEXT_COLOR_NORMAL, styleColor);
                        }
                        else
                            Label(line.c_str());
                    }
                    EndScrollPanel();
                    SetStyle(DEFAULT, BORDER_WIDTH, 1);
                    EndWindowBox();
                    if (IsKeyPressed(KEY_ESCAPE))
                        aboutOpen = false;
                }
                SetNextWidth(20);
                if (iconButton(ICON_ROM, _mainView == eROM_SELECTOR)) {
#ifdef PLATFORM_WEB
                    loadFileWeb();
#else
                    _mainView = eROM_SELECTOR;
                    _librarian.fetchDir(_currentDirectory);
#endif
                }
#ifndef PLATFORM_WEB
                if (iconButton(ICON_NOTEBOOK, _mainView == eLIBRARY)) {
                    _mainView = eLIBRARY;
                }
#endif
                SetNextWidth(130);
                SetStyle(TEXTBOX, BORDER_WIDTH, 1);
                TextBox(_romName, 4095);

                bool chip8Control = _debugger.isControllingChip8();
                Color controlBack = {3, 127, 161};
                Color controlColor = Color{0x51, 0xbf, 0xd3, 0xff};  // chip8Control ? Color{0x51, 0xbf, 0xd3, 0xff} : Color{0x51, 0xff, 0xbf, 0xff};
                if (iconButton(ICON_PLAYER_PAUSE, _chipEmu->execMode() == ExecMode::ePAUSED /*, controlBack, controlColor*/) || ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && IsKeyPressed(KEY_F5))) {
                    _chipEmu->focussedExecutionUnit()->setExecMode(ExecMode::ePAUSED);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eVIDEO;
                    }
                }
                SetTooltip("PAUSE [Shift+F5]");
                if (iconButton(ICON_PLAYER_PLAY, _chipEmu->execMode() == ExecMode::eRUNNING /*, controlBack, controlColor*/) || (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT) && IsKeyPressed(KEY_F5))) {
                    _debugger.setExecMode(ExecMode::eRUNNING);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = _lastRunView;
                    }
                }
                SetTooltip("RUN [F5]");
                if (!_debugger.supportsStepOver())
                    GuiDisable();
                if (iconButton(ICON_STEP_OVER, _chipEmu->execMode() == ExecMode::eSTEPOVER /*, controlBack, controlColor*/) || (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT) && IsKeyPressed(KEY_F8))) {
                    _debugger.setExecMode(ExecMode::eSTEPOVER);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                GuiEnable();
                SetTooltip("STEP OVER [F8]");
                if (iconButton(ICON_STEP_INTO, _chipEmu->execMode() == ExecMode::eSTEP /*, controlBack, controlColor*/) || (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT) && IsKeyPressed(KEY_F7))) {
                    _debugger.setExecMode(ExecMode::eSTEP);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                SetTooltip("STEP INTO [F7]");
                if (!_debugger.supportsStepOver())
                    GuiDisable();
                if (iconButton(ICON_STEP_OUT, _chipEmu->execMode() == ExecMode::eSTEPOUT /*, controlBack, controlColor*/) || ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && IsKeyPressed(KEY_F7))) {
                    _debugger.setExecMode(ExecMode::eSTEPOUT);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                GuiEnable();
                SetTooltip("STEP OUT [Shift+F7]");
                if (iconButton(ICON_RESTART)) {
                    reloadRom(true);
                    resetStats();
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = _lastRunView;
                    }
                }
                SetTooltip("RESTART");
                int buttonsRight = 8;
                ++buttonsRight;
                int avail = _screenWidth - GetCurrentPos().x;
//#ifdef RESIZABLE_GUI
//                --buttonsRight;
//                avail += 10;
//#endif
                auto spacePos = GetCurrentPos();
                auto spaceWidth = avail - buttonsRight * 20;
                Space(spaceWidth);
                if (_chipEmu->getMaxScreenWidth() > 128)
                    GuiDisable();
                if (iconButton(ICON_BOX_GRID, _grid))
                    _grid = !_grid;
                GuiEnable();
                SetTooltip("TOGGLE GRID");
                Space(10);
                if (iconButton(ICON_ZOOM_ALL, _mainView == eVIDEO))
                    _mainView = eVIDEO;
                SetTooltip("FULL VIDEO");
                if (iconButton(ICON_CPU, _mainView == eDEBUGGER))
                    _mainView = eDEBUGGER;
                SetTooltip("DEBUGGER");
                if (iconButton(ICON_FILETYPE_TEXT, _mainView == eEDITOR))
                    _mainView = eEDITOR, _chipEmu->setExecMode(ExecMode::ePAUSED);
                SetTooltip("EDITOR");
                if (iconButton(ICON_PRINTER, _mainView == eTRACELOG))
                    _mainView = eTRACELOG;
                SetTooltip("TRACE-LOG");
                if (iconButton(ICON_GEAR, _mainView == eSETTINGS))
                    _mainView = eSETTINGS;
                SetTooltip("SETTINGS");
                if (iconButton(ICON_AUDIO, false))
                    volumeClick = std::chrono::steady_clock::now();
                SetTooltip("VOLUME");

                static Vector2 versionSize = MeasureTextEx(GuiGetFont(), "v" CADMIUM_VERSION, 8, 0);
                DrawTextEx(GuiGetFont(), "v" CADMIUM_VERSION, {spacePos.x + (spaceWidth - versionSize.x) / 2, spacePos.y + 6}, 8, 0, WHITE);
                Space(10);
                if (iconButton(ICON_HIDPI, _scaleMode != 1))
                    _scaleMode = _scaleMode >= 3 ? 1 : _scaleMode + 1;
                SetTooltip("TOGGLE ZOOM    ");
            }
            EndColumns();

            switch (_mainView) {
                case eDEBUGGER: {
                    _lastView = _lastRunView = _mainView;
                    _debugger.render(_font, [this](Rectangle video, int scale) {
                        _screenOverlay = video;
                        _screenScale = scale;
                        if (_videoRenderMode == eHIRES) {
                            rlSetBlendFactors(1, 0, 0x8006);
                            rlSetBlendMode(RL_BLEND_CUSTOM);
                            DrawRectangleRec(_screenOverlay, {0,0,0,255});
                            rlSetBlendMode(RL_BLEND_ALPHA);
                        }
                    });
                    if (_videoRenderMode == eFAST) {
                        drawScreen(_screenOverlay, _screenScale);
                    }
                    break;
                }
                case eVIDEO: {
                    _lastView = _lastRunView = _mainView;
                    gridScale = _screenWidth / _chipEmu->getCurrentScreenWidth();
                    _screenOverlay = {0, 20, (float)_screenWidth, (float)_screenHeight - 36};
                    _screenScale = gridScale;
                    if (_videoRenderMode == eHIRES) {
                        rlSetBlendFactors(1, 0, 0x8006);
                        rlSetBlendMode(RL_BLEND_CUSTOM);
                        DrawRectangleRec(_screenOverlay, {0,0,0,255});
                        rlSetBlendMode(RL_BLEND_ALPHA);
                    }
                    else {
                        drawScreen(_screenOverlay, _screenScale);
                    }
                    break;
                }
                case eEDITOR:
                    if (_lastView != eEDITOR)
                        _editor.setFocus();
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Editor", {1, 1});
                    {
                        auto rect = GetContentAvailable();
                        _editor.draw(_font, {rect.x, rect.y - 1, rect.width, rect.height});
                    }
                    EndPanel();
                    End();
                    break;
                case eTRACELOG: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Trace-Log", {1, 1});
                    {
                        auto rect = GetContentAvailable();
                        _logView.draw(_font, {rect.x, rect.y - 1, rect.width, rect.height});
                    }
                    EndPanel();
                    End();
                    break;
                }
                case eSETTINGS: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Settings");
                    {
                        static int activeTab = 0;
                        BeginTabView(&activeTab);
                        if (BeginTab("Emulation", {5, 0})) {
                            renderEmulationSettings();
                            EndTab();
                        }
                        if (BeginTab("Appearance", {5, 0})) {
                            _styleManager.renderAppearanceEditor();
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            EndTab();
                        }
                        if (BeginTab("Misc", {5, 0})) {
                            Space(3);
                            Label("Config Directory:");
                            GuiDisable();
                            TextBox(_cfgPath, 4096);
                            GuiEnable();
                            Label("CHIP-8 Library Directories:");
                            if (TextBox(_databaseDirectory, 4096)) {
                                saveConfig();
                            }
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            EndTab();
                        }
                        EndTabView();
                    }
                    EndPanel();
                    End();
                    break;
                }
#ifndef PLATFORM_WEB
                case eROM_SELECTOR: {
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Load/Import ROM or Octo Source");
                    {
                        renderFileBrowser(eLOAD);
                    }
                    EndPanel();
                    End();
                    if (IsKeyPressed(KEY_ESCAPE))
                        _mainView = _lastView;
                    break;
                }
#else
                case eROM_SELECTOR:
                    break;
#endif  // !PLATFORM_WEB
                case eROM_EXPORT: {
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Save/Export ROM or Source");
                    {
#ifdef PLATFORM_WEB
                        renderFileBrowser(eWEB_SAVE);
#else
                        renderFileBrowser(eSAVE);
#endif
                    }
                    EndPanel();
                    End();
                    if (IsKeyPressed(KEY_ESCAPE))
                        _mainView = _lastView;
                    break;
                }
#ifndef PLATFORM_WEB
                case eLIBRARY: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Library / Research");
                    if (_database && _database->render(_font)) {
                        auto program = _database->getSelectedProgram();
                        if (program) {
                            loadBinary(program->name, program->data, program->properties, true);
                            reloadRom(true);
                            _lastRunView = _mainView = eDEBUGGER;
                        }
                    }
                    Space(_screenHeight - GetCurrentPos().y - 20 - 1);
                    EndPanel();
                    End();
                    break;
                }
#endif
            }

            if (_colorSelectOpen) {
                _colorSelectOpen = !BeginWindowBox({-1, -1, 200, 250}, "Select Color", &_colorSelectOpen, WindowBoxFlags(WBF_MOVABLE | WBF_MODAL));
                uint32_t prevCol = *_selectedColor;
                *_selectedColor = ColorToInt(ColorPicker(GetColor(*_selectedColor)));
                if (*_selectedColor != prevCol) {
                    _colorText = fmt::format("{:06x}", *_selectedColor >> 8);
                }
                Space(5);
                BeginColumns();
                SetNextWidth(40);
                Label("Color:");
                SetNextWidth(60);
                if (TextBox(_colorText, 7)) {
                    *_selectedColor = (std::strtoul(_colorText.c_str(), nullptr, 16) << 8) + 255;
                }
                EndColumns();
                Space(5);
                BeginColumns();
                Space(30);
                SetNextWidth(60);
                if (Button("Ok")) {
                    _defaultPalette = _colorPalette;
                    _selectedColor = nullptr;
                    _colorSelectOpen = false;
                }
                SetNextWidth(60);
                if (Button("Cancel") || IsKeyPressed(KEY_ESCAPE)) {
                    *_selectedColor = _previousColor;
                    _selectedColor = nullptr;
                    _colorSelectOpen = false;
                }
                EndColumns();
                EndWindowBox();
            }
            if (IsKeyDown(KEY_ESCAPE))
                volumeClick = std::chrono::steady_clock::time_point{};
            if (volumeClick != std::chrono::steady_clock::time_point{}) {
                if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - volumeClick).count() < 2) {
                    Rectangle bounds{430.0, 21.0f, 80.0f, 14.0f};
                    DrawRectangleRec({bounds.x - 56, bounds.y - 2, bounds.width + 58, bounds.height + 4}, {0, 0, 0, 128});
                    GuiSliderBar(bounds, "Volume: ", "", &_volumeSlider, 0.0001f, 1.0f);
                    if (_volumeSlider != _volume)
                        SetMasterVolume(_volumeSlider);
                    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
                        volumeClick = std::chrono::steady_clock::now();
                    }
                }
                else {
                    if (_volumeSlider != _volume) {
                        _volume = _volumeSlider;
                        _cfg.volume = _volume;
                        saveConfig();
                    }
                }
            }
            EndGui();
        }
        static auto lastExecMode = _chipEmu->execMode();
        if (_chipEmu->execMode() == ExecMode::eRUNNING || (_chipEmu->execMode() != ExecMode::ePAUSED && lastExecMode == ExecMode::ePAUSED)) {
            _debugger.captureStates();
        }
        lastExecMode = _chipEmu->execMode();
    }

    enum PropertyAlign { PA_LEFT, PA_RIGHT };

    static int editProperty(emu::Property& prop, bool forceUpdate, PropertyAlign pa = PA_RIGHT)
    {
        auto prevTextAlignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
        if(pa == PA_RIGHT) {
            gui::BeginColumns();
            gui::SetSpacing(4);
            gui::SetNextWidth(90);
            gui::SetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_RIGHT);
            gui::Label(fmt::format("{}", prop.getName()).c_str());
            gui::SetStyle(LABEL, TEXT_ALIGNMENT, prevTextAlignment);
        }
        //gui::SetNextWidth(150);
        if (prop.access() != emu::eWritable)
            GuiDisable();
        const auto rc = std::visit(emu::visitor{
                                [](std::nullptr_t) -> int { gui::Label(""); return 0; },
                                [pa, &prop](bool& val) -> int { val = gui::CheckBox(pa == PA_RIGHT ? "" : prop.getName().c_str(), val); return val ? 1 : 0; },
                                [pa, &prop](emu::Property::Integer& val) -> int { gui::Spinner(pa == PA_RIGHT ? "" : prop.getName().c_str(), &val.intValue, val.minValue, val.maxValue); return val.intValue; },
                                [](std::string& val) -> int {
                                    auto prevTextAlignment = GuiGetStyle(TEXTBOX, TEXT_ALIGNMENT);
                                    gui::SetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                                    gui::TextBox(val, 4096);
                                    gui::SetStyle(TEXTBOX, TEXT_ALIGNMENT, prevTextAlignment);
                                    return 0;
                                },
                                [&forceUpdate](emu::Property::Combo& val) -> int {
                                    if (gui::DropdownBox(val.rgCombo.c_str(), &val.index))
                                        forceUpdate = true;
                                    return val.index;
                                }},
                   prop.getValue());
        if (prop.access() != emu::eWritable)
            GuiEnable();
        if(pa == PA_RIGHT) {
            gui::EndColumns();
        }
        return rc;
    }

    void editPropertyCheckBox(std::string_view key, bool forceUpdate)
    {
        if(_properties->containsFuzzy(key)) {
            editProperty(_properties->at(key), forceUpdate);
        }
        else {
            static bool dummyBool = false;
            GuiDisable();
            gui::CheckBox(key.data(), dummyBool);
            GuiEnable();
        }
    }

    int editPropertySpinner(std::string_view key, bool forceUpdate, int defaultValue = 0)
    {
        if(_properties->containsFuzzy(key)) {
            return editProperty(_properties->at(key), forceUpdate, PA_LEFT);
        }
        static int dummyInt = defaultValue;
        GuiDisable();
        gui::Spinner(key.data(), &dummyInt, defaultValue, defaultValue);
        GuiEnable();
        return -1;
    }

    void renderEmulationSettings()
    {
        using namespace gui;
        auto oldProps = *_properties;
        bool forceUpdate = false;
        auto& props = *_properties;
        BeginColumns();
        SetNextWidth(0.42f);
        BeginGroupBox("CHIP-8 variant / Core");
        Space(5);
        SetSpacing(2);
        if(DropdownBox(_cores.getCoresCombo().c_str(), &_behaviorSel)) {
            _subBehaviorSel = 0;
            auto preset = _cores[_behaviorSel].variantProperties(0);
            _frameBoost = 1;
            updateEmulatorOptions(preset);
            props = *_properties;
        }
        if(DropdownBox(_cores[_behaviorSel].variantsCombo.c_str(), &_subBehaviorSel)) {
            auto preset = _cores[_behaviorSel].variantProperties(_subBehaviorSel);
            _frameBoost = 1;
            updateEmulatorOptions(preset);
            props = *_properties;
        }
        if(_properties->containsFuzzy("Trace-log")) {
            editProperty(_properties->at("Trace-log"), forceUpdate, PA_LEFT);
        }
        else {
            static bool dummyTrace = false;
            GuiDisable();
            CheckBox("Trace-Log", dummyTrace);
            GuiEnable();
        }
        EndGroupBox();
        //Space(10);
        BeginGroupBox("Emulation Speed");
        Space(5);
        SetIndent(150);
        //SetRowHeight(20);
        SetSpacing(2);
        auto ipf = editPropertySpinner("Instructions per frame", forceUpdate);
        editPropertySpinner("Frame rate", forceUpdate, _chipEmu->frameRate());
        if(ipf != 0) {
            Spinner("Frame boost", &_frameBoost, 1, 1000);
        }
        else {
            static int _fb1{1};
            GuiDisable();
            Spinner("Frame boost", &_fb1, 1, 1000);
            GuiEnable();
        }
        g_frameBoost = getFrameBoost();
        EndGroupBox();
        EndColumns();
        Space(5);
        const int quirksHeight = 181;
#if 0
        if(_chipEmu->isGenericEmulation() && _options.behaviorBase != emu::Chip8EmulatorOptions::eCHIP8TE) {
            BeginGroupBox("Quirks");
            auto startY = GetCurrentPos().y;
            Space(5);
            BeginColumns();
            SetNextWidth(GetContentAvailable().width/2);
            Begin();
            _options.optJustShiftVx = CheckBox("8xy6/8xyE just shift VX", _options.optJustShiftVx);
            _options.optDontResetVf = CheckBox("8xy1/8xy2/8xy3 don't reset VF", _options.optDontResetVf);
            bool oldInc = !(_options.optLoadStoreIncIByX | _options.optLoadStoreDontIncI);
            bool newInc = CheckBox("Fx55/Fx65 increment I by X + 1", oldInc);
            if(newInc != oldInc) {
                _options.optLoadStoreIncIByX = !newInc;
                _options.optLoadStoreDontIncI = false;
            }
            oldInc = _options.optLoadStoreIncIByX;
            _options.optLoadStoreIncIByX = CheckBox("Fx55/Fx65 increment I only by X", _options.optLoadStoreIncIByX);
            if(_options.optLoadStoreIncIByX != oldInc) {
                _options.optLoadStoreDontIncI = false;
            }
            oldInc = _options.optLoadStoreDontIncI;
            _options.optLoadStoreDontIncI = CheckBox("Fx55/Fx65 don't increment I", _options.optLoadStoreDontIncI);
            if(_options.optLoadStoreDontIncI != oldInc) {
                _options.optLoadStoreIncIByX = false;
            }
            _options.optJump0Bxnn = CheckBox("Bxnn/jump0 uses Vx", _options.optJump0Bxnn);
            _options.optCyclicStack = CheckBox("Cyclic stack", _options.optCyclicStack);
            _options.optXOChipSound = CheckBox("XO-CHIP sound engine", _options.optXOChipSound);
            _options.optAllowColors = CheckBox("Multicolor support", _options.optAllowColors);
            _options.optHas16BitAddr = CheckBox("Has 16 bit addresses", _options.optHas16BitAddr);
            End();
            Begin();
            _options.optWrapSprites = CheckBox("Wrap sprite pixels", _options.optWrapSprites);
            _options.optInstantDxyn = CheckBox("Dxyn doesn't wait for vsync", _options.optInstantDxyn);
            bool oldLoresWidth = _options.optLoresDxy0Is8x16;
            _options.optLoresDxy0Is8x16 = CheckBox("Lores Dxy0 draws 8 pixel width", _options.optLoresDxy0Is8x16);
            if(!oldLoresWidth && _options.optLoresDxy0Is8x16)
                _options.optLoresDxy0Is16x16 = false;
            oldLoresWidth = _options.optLoresDxy0Is16x16;
            _options.optLoresDxy0Is16x16 = CheckBox("Lores Dxy0 draws 16 pixel width", _options.optLoresDxy0Is16x16);
            if(!oldLoresWidth && _options.optLoresDxy0Is16x16)
                _options.optLoresDxy0Is8x16 = false;
            bool oldVal = _options.optSC11Collision;
            _options.optSC11Collision = CheckBox("Dxyn uses SCHIP1.1 collision", _options.optSC11Collision);
            if(!oldVal && _options.optSC11Collision) {
                _options.optAllowHires = true;
            }
            _options.optSCLoresDrawing = CheckBox("HP SuperChip lores drawing", _options.optSCLoresDrawing);
            _options.optHalfPixelScroll = CheckBox("Half pixel scrolling", _options.optHalfPixelScroll);
            bool oldAllowHires = _options.optAllowHires;
            _options.optAllowHires = CheckBox("128x64 hires support", _options.optAllowHires);
            if(!_options.optAllowHires && oldAllowHires) {
                _options.optOnlyHires = false;
                _options.optSC11Collision = false;
            }
            bool oldOnlyHires = _options.optOnlyHires;
            _options.optOnlyHires = CheckBox("Only 128x64 mode", _options.optOnlyHires);
            if(_options.optOnlyHires && !oldOnlyHires)
                _options.optAllowHires = true;
            _options.optModeChangeClear = CheckBox("Mode change clear", _options.optModeChangeClear);
            End();
            EndColumns();
            auto used = GetCurrentPos().y - startY;
            Space(quirksHeight - used - 2);
            EndGroupBox();
            Space(4);
        }
        //else if(!_chipEmu->isGenericEmulation()) {
#endif
        BeginGroupBox("System Configuration");
        auto startY = GetCurrentPos().y;
        auto colWidth1 = GetContentAvailable().width / 2 - 1;
        auto colWidth2 = GetContentAvailable().width - colWidth1 - 1;
        auto colHeight = GetContentAvailable().height;
        auto rowCount = 0;
        Space(5);
        BeginColumns();
        SetSpacing(2);
        SetNextWidth(colWidth1);
        Begin();
        SetSpacing(2);
        for(size_t i = 0; i < props.numProperties(); ++i) {
            auto& prop = props[i];
            if(prop.getName().empty()) {
                auto used = GetCurrentPos().y - startY;
                Space(quirksHeight - used - 4);
                End();
                Begin();
                SetSpacing(2);
            }
            else if(prop.access() != emu::eInvisible && !fuzzyAnyOf(prop.getName(), {"TraceLog", "InstructionsPerFrame", "FrameRate"})) {
                if(props.numProperties() > 20 && std::holds_alternative<bool>(prop.getValue())) {
                    editProperty(prop, forceUpdate, PA_LEFT);
                }
                else {
                    editProperty(prop, forceUpdate);
                }
                ++rowCount;
            }
        }
        auto* changedProp = props.changedProperty(_propsMemento);
        if(changedProp) {
            // on change...
            if(_chipEmu->updateProperties(props, *changedProp)) {
                // core asks for a cold start...
                updateEmulatorOptions(props);
            }
        }
        auto used = GetCurrentPos().y - startY;
        Space(quirksHeight - used - 4);
        End();
        EndColumns();
        EndGroupBox();

        Space(14);
        {
            StyleManager::Scope guard;
            BeginColumns();
            auto pos = GetCurrentPos();
            pos.x = std::ceil(pos.x);
            pos.y = std::ceil(pos.y);
            SetNextWidth(52.0f + 16*18);
            Label("Colors:");
#if 0 // TODO: fix this
            for (int i = 0; i < 16; ++i) {
                bool hover =  CheckCollisionPointRec(GetMousePosition(), {pos.x + 52 + i * 18, pos.y, 16, 16});
                DrawRectangle(pos.x + 52 + i * 18, pos.y, 16, 16, GetColor(guard.getStyle(hover ? Style::BORDER_COLOR_FOCUSED : Style::BORDER_COLOR_NORMAL)));
                DrawRectangle(pos.x + 52 + i * 18 + 1, pos.y + 1 , 14, 14, GetColor(guard.getStyle(Style::BACKGROUND_COLOR)));
                DrawRectangle(pos.x + 52 + i * 18 + 2, pos.y + 2 , 12, 12, GetColor(_colorPalette[i]));
                if(!GuiIsLocked() && IsMouseButtonReleased(0) && hover) {
                    _selectedColor = &_colorPalette[i];
                    _previousColor = _colorPalette[i];
                    _colorText = fmt::format("{:06x}", _colorPalette[i]>>8);
                    _colorSelectOpen = true;
                }
            }
            static std::vector<uint32_t> prevPalette(_colorPalette.begin(), _colorPalette.end());
            if(std::memcmp(prevPalette.data(), _colorPalette.data(), 16*sizeof(uint32_t)) != 0) {
                setPalette({_colorPalette.begin(), _colorPalette.end()});
                prevPalette.assign(_colorPalette.begin(), _colorPalette.end());
            }
#endif
            static int sel = 5;
            if(DropdownBox("Cadmium;Silicon-8;Pico-8;Octo Classic;LCD;Custom", &sel, true)) {
                switch(sel) {
                    case 0:
                        setPalette({
                            0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff,
                            0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff,
                            0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff,
                            0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff
                        });
                        _defaultPalette = _colorPalette;
                        sel = 5;
                        break;
                    case 1:
                        setPalette({
                            0x000000ff, 0xffffffff, 0xaaaaaaff, 0x555555ff,
                            0xff0000ff, 0x00ff00ff, 0x0000ffff, 0xffff00ff,
                            0x880000ff, 0x008800ff, 0x000088ff, 0x888800ff,
                            0xff00ffff, 0x00ffffff, 0x880088ff, 0x008888ff
                        });
                        _defaultPalette = _colorPalette;
                        sel = 5;
                        break;
                    case 2:
                        setPalette({
                            0x000000ff, 0xfff1e8ff, 0xc2c3c7ff, 0x5f574fff,
                            0xef7d57ff, 0x00e436ff, 0x29adffff, 0xffec27ff,
                            0xab5236ff, 0x008751ff, 0x1d2b53ff, 0xffa300ff,
                            0xff77a8ff, 0xffccaaff, 0x7e2553ff, 0x83769cff
                        });
                        _defaultPalette = _colorPalette;
                        sel = 5;
                        break;
                    case 3:
                        setPalette({
                            0x996600ff, 0xFFCC00ff, 0xFF6600ff, 0x662200ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
                        });
                        _defaultPalette = _colorPalette;
                        sel = 5;
                        break;
                    case 4:
                        setPalette({
                            0xf2fff2ff, 0x5b8c7cff, 0xadd9bcff, 0x0d1a1aff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
                        });
                        _defaultPalette = _colorPalette;
                        sel = 5;
                        break;
                    default:
                        break;
                }
                // SWEETIE-16:
                // {0x1a1c2c, 0xf4f4f4, 0x94b0c2, 0x333c57,  0xef7d57, 0xa7f070, 0x3b5dc9, 0xffcd75,  0xb13e53, 0x38b764, 0x29366f, 0x566c86,  0x41a6f6, 0x73eff7, 0x5d275d, 0x257179}
                // PICO-8:
                // {0x000000, 0xfff1e8, 0xc2c3c7, 0x5f574f,  0xff004d, 0x00e436, 0x29adff, 0xffec27,  0xab5236, 0x008751, 0x1d2b53, 0xffa300,  0xff77a8, 0xffccaa, 0x7e2553, 0x83769c}
                // C64:
                // {0x000000, 0xffffff, 0xadadad, 0x626262, 0xa1683c, 0x9ae29b, 0x887ecb, 0xc9d487, 0x9f4e44, 0x5cab5e, 0x50459b, 0x6d5412, 0xcb7e75, 0x6abfc6, 0xa057a3, 0x898989}
                // Intellivision:
                // {0x0c0005, 0xfffcff, 0xa7a8a8, 0x3c5800, 0xff3e00, 0x6ccd30, 0x002dff, 0xfaea27, 0xffa600, 0x00a720, 0xbd95ff, 0xc9d464, 0xff3276, 0x5acbff, 0xc81a7d, 0x00780f}
                // CGA
                // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff5555, 0x55ff55, 0x5555ff, 0xffff55, 0xaa0000, 0x00aa00, 0x0000aa, 0xaa5500, 0xff55ff, 0x55ffff, 0xaa00aa, 0x00aaaa}
                // Silicon-8 1.0
                // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x880000, 0x008800, 0x000088, 0x888800, 0xff00ff, 0x00ffff, 0x880088, 0x008888}
                // Macintosh II
                // {0x000000, 0xffffff, 0xb9b9b9, 0x454545, 0xdc0000, 0x00a800, 0x0000ca, 0xffff00, 0xff6500, 0x006500, 0x360097, 0x976536, 0xff0097, 0x0097ff, 0x653600, 0x868686}
                // IBM PCjr
                // {0x1c2536, 0xced9ed, 0x81899e, 0x030625, 0xe85685, 0x2cc64e, 0x0000e8, 0xa7c251, 0x9f2441, 0x077c35, 0x0e59f0, 0x4b7432, 0xc137ff, 0x0bc3a9, 0x6b03ca, 0x028566}
                // Daylight-16
                // {0x272223, 0xf2d3ac, 0xe7a76c, 0x6a422c, 0xb55b39, 0xb19e3f, 0x7a6977, 0xf8c65c, 0x996336, 0x606b31, 0x513a3d, 0xd58b39, 0xc28462, 0xb5c69a, 0x905b54, 0x878c87}
                // Soul of the Sea
                // {0x01141a, 0xcfbc95, 0x93a399, 0x2f4845, 0x92503f, 0x949576, 0x425961, 0x81784d, 0x703a28, 0x7a7e67, 0x203633, 0x605f33, 0x56452b, 0x467e73, 0x403521, 0x51675a}

            }
            EndColumns();
        }
        Space(8);
        BeginColumns();
        Space(100);
        SetNextWidth(0.21f);
        bool romRemembered = _cfg.romConfigs.contains(_romSha1);
        if((romRemembered && *_properties == _cfg.romConfigs[_romSha1]) || (_romIsWellKnown && *_properties == _romWellKnownProperties)) {
            GuiDisable();
        }
        if (Button(!romRemembered ? "Remember for ROM" : "Update for ROM")) {
            _cfg.romConfigs[_romSha1] = *_properties;
            saveConfig();
        }
        GuiEnable();
        if(!romRemembered)
            GuiDisable();
        SetNextWidth(0.21f);
        if(Button("Forget ROM")) {
            _cfg.romConfigs.erase(_romSha1);
            saveConfig();
        }
        GuiEnable();
        EndColumns();
        auto pos = GetCurrentPos();
        Space(_screenHeight - pos.y - 20 - 1);
        //SetIndent(110);
        //Label("(C) 2022 by Steffen '@gulrak' Schümann");

    }

    void renderFileBrowser(FileBrowserMode mode)
    {
        using namespace gui;
        static Vector2 scroll{0,0};
        static Librarian::Info selectedInfo;
        SetRowHeight(16);
        auto area = GetContentAvailable();
#ifdef PLATFORM_WEB
        Space(area.height - 54);
#else
        if(TextBox(_currentDirectory, 4096)) {
            _librarian.fetchDir(_currentDirectory);
            _currentDirectory = _librarian.currentDirectory();
        }
        Space(1);
        auto tabPos = GetCurrentPos();
        BeginTableView(area.height - 135, 4, &scroll);
        for(int i = 0; i < _librarian.numEntries(); ++i) {
            const auto& info = _librarian.getInfo(i);
            auto rowCol = Color{0,0,0,0};
            if(info.analyzed) {
                //if(info.type == Librarian::Info::eROM_FILE)
                //    rowCol = Color{0,128,128,32}; //ColorAlpha(GetColor(GetStyle(DEFAULT, BASE_COLOR_NORMAL)), 64);
            }
            else {
                rowCol = Color{0,128,0,10};
            }
            auto pos = GetCurrentPos();
            bool hover = false; //CheckCollisionPointRec(GetMousePosition(), {5, pos.y + scroll.y + tabPos.y, area.width, 16.0f});
            TableNextRow(16, hover ? _styleManager.getStyleColor(Style::BASE_COLOR_NORMAL) : rowCol);
            if(TableNextColumn(24)) {
                int icon = ICON_HELP2;
                switch (info.type) {
                    case Librarian::Info::eDIRECTORY: icon = ICON_FOLDER_OPEN; break;
                    case Librarian::Info::eROM_FILE: icon = ICON_ROM; break;
                    case Librarian::Info::eOCTO_SOURCE: icon = ICON_FILETYPE_TEXT; break;
                    default: icon = ICON_FILE_DELETE; break;
                }
                auto oldFG = gui::GetStyle(LABEL, TEXT_COLOR_NORMAL);
                {
                    StyleManager::Scope guard;
                    if (info.type == Librarian::Info::eROM_FILE)
                        guard.setStyle(Style::TEXT_COLOR_NORMAL, info.isKnown ? GREEN : YELLOW);
                    //    gui::SetStyle(LABEL, TEXT_COLOR_NORMAL, info.isKnown ? ColorToInt(GREEN) : ColorToInt(YELLOW));
                    Label(GuiIconText(icon, ""));
                    //gui::SetStyle(LABEL, TEXT_COLOR_NORMAL, oldFG);
                }
            }
            if(TableNextColumn(.66f)) {
                if(info.filePath.size() > 50 ? LabelButton(info.filePath.substr(0,50).c_str()) : LabelButton(info.filePath.c_str())) {
                    if(info.type == Librarian::Info::eDIRECTORY) {
                        if(info.filePath != "..") {
                            _librarian.intoDir(info.filePath);
                            _currentDirectory = _librarian.currentDirectory();
                            if(mode == eLOAD)
                                _currentFileName = "";
                        }
                        else {
                            _librarian.parentDir();
                            _currentDirectory = _librarian.currentDirectory();
                            if(mode == eLOAD)
                                _currentFileName = "";
                        }
                        selectedInfo.analyzed = false;
                        selectedInfo.isKnown = false;
                        break;
                    }
                    else if(info.type == Librarian::Info::eOCTO_SOURCE) {
                        selectedInfo = info;
                        _currentFileName = info.filePath;
                    }
                    else if(info.type == Librarian::Info::eROM_FILE) {
                        selectedInfo = info;
                        _currentFileName = info.filePath;
                    }
                }
            }
            if(TableNextColumn(.145f))
                Label(info.type == Librarian::Info::eDIRECTORY ? "" : fmt::format("{:>8s}", formatUnit(info.fileSize, "")).c_str());
            if(TableNextColumn(.13f) && info.filePath != "..")
                Label(date::format("%F", date::floor<std::chrono::seconds>(info.changeDate)).c_str());
        }
        EndTableView();
#endif
        Space(1);
        BeginColumns();
        SetNextWidth(25);
        Label("File:");
        TextBox(_currentFileName, 4096);
        EndColumns();
        Space(2);
        switch(mode) {
            case eLOAD: {
                auto infoPos = GetCurrentPos();
                Label(fmt::format("SHA1:  {}", selectedInfo.analyzed ? selectedInfo.sha1sum.to_hex() : "").c_str());
                if(!selectedInfo.analyzed || selectedInfo.isKnown) {
                    Label(fmt::format("Type:  {}", selectedInfo.analyzed ?selectedInfo.variant : "").c_str());
                }
                else {
                    Label(fmt::format("Type:  {} (estimated)", selectedInfo.minimumOpcodeProfile()).c_str());
                }
                if(selectedInfo.analyzed) {
                    if(_screenShotSha1 != selectedInfo.sha1sum) {
                        // TODO: Fix this
                        _screenshotData = {}; //_librarian.genScreenshot(selectedInfo, _defaultPalette);
                        _screenShotSha1 = selectedInfo.sha1sum;
                        if(_screenshotData.width && _screenshotData.pixel.size() == _screenshotData.width * _screenshotData.height) {
                            auto* image = (uint32_t*)_screenShot.data;
                            for(int y = 0; y < _screenshotData.height; ++y) {
                                for(int x = 0; x < _screenshotData.width; ++x) {
                                    image[y * _screenShot.width + x] = _screenshotData.pixel[y * _screenshotData.width + x];
                                }
                            }
                            UpdateTexture(_screenShotTexture, _screenShot.data);
                        }
                    }
                    if(_screenShotSha1 == selectedInfo.sha1sum && _screenshotData.width) {
                        DrawTexturePro(_screenShotTexture, {0, 0, (float)_screenshotData.width, (float)_screenshotData.height}, {300, infoPos.y + 2, 192, 96}, {0,0}, 0, WHITE);
                        DrawRectangleLinesEx({299, infoPos.y + 1, 194, 98}, 1, GetColor(GetStyle(DEFAULT, BORDER_COLOR_NORMAL)));
                    }
                }
                Space(3);
                BeginColumns();
                Space(32);
                SetNextWidth(80);
                if(!selectedInfo.analyzed) GuiDisable();
                if(Button("Load") && selectedInfo.analyzed) {
                    auto mainView = _mainView;
                    loadRom(_librarian.fullPath(selectedInfo.filePath).c_str(), LoadOptions::None);
                    if(_mainView == mainView)
                        _mainView = _lastView;
                }
                SetNextWidth(110);
                if(Button("Load w/o Config") && selectedInfo.analyzed) {
                    _chipEmu->reset();
                    loadRom(_librarian.fullPath(selectedInfo.filePath).c_str(), LoadOptions::DontChangeOptions);
                    _mainView = _lastView;
                }
                GuiEnable();
                EndColumns();
                break;
            }
            case eWEB_SAVE:
            case eSAVE: {
                BeginColumns();
                SetNextWidth(100);
                Label("Select file type:");
                static int activeType = 0;
                SetNextWidth(70);
                activeType = ToggleGroup("ROM File;Source Code", activeType);
                EndColumns();
                Space(3);
                SetNextWidth(80);
                SetIndent(32);
                if(_currentFileName.empty() && ((activeType == 0 && _romImage.empty()) || (activeType == 1 && _editor.getText().empty()))) GuiDisable();
                if(Button("Save") && !_currentFileName.empty()) {
                    // TODO: Fix this
                    std::string romExtension = ".ch8";
                    if (activeType == 0 && fs::path(_currentFileName).extension() != romExtension) {
                        if (fs::path(_currentFileName).has_extension())
                            _currentFileName = fs::path(_currentFileName).replace_extension(romExtension).string();
                        else
                            _currentFileName += romExtension;
                    }
                    else if (activeType == 1 && fs::path(_currentFileName).extension() != ".8o") {
                        if (fs::path(_currentFileName).has_extension())
                            _currentFileName = fs::path(_currentFileName).replace_extension(".8o").string();
                        else
                            _currentFileName += ".8o";
                    }
#ifdef PLATFORM_WEB
                    auto targetFile = _currentFileName;
#else
                    auto targetFile = _librarian.fullPath(_currentFileName);
#endif
                    if(activeType == 0) {
                        writeFile(targetFile, (const char*)_romImage.data(), _romImage.size());
                    }
                    else {
                        writeFile(targetFile, _editor.getText().data(), _editor.getText().size());
                    }
#ifdef PLATFORM_WEB
                    // can only use path-less filenames
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", targetFile.c_str(), targetFile.c_str()));
#endif
                    _mainView = _lastView;
                }
                GuiEnable();
                break;
            }
        }
        BeginColumns();
        EndColumns();
        auto pos = GetCurrentPos();
        Space(_screenHeight - pos.y - 20 - 1);
    }

#ifdef PLATFORM_WEB
    void loadFileWeb()
    {
        //-------------------------------------------------------------------------------
        // This file upload dialog is heavily inspired by MIT licensed code from
        // https://github.com/daid/SeriousProton2 - specifically:
        //
        // https://github.com/daid/SeriousProton2/blob/f13f32336360230788054822183049a0153c0c07/src/io/fileSelectionDialog.cpp#L67-L102
        //
        // All Rights Reserved.
        //
        // Permission is hereby granted, free of charge, to any person obtaining a copy
        // of this software and associated documentation files (the "Software"), to deal
        // in the Software without restriction, including without limitation the rights
        // to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
        // copies of the Software, and to permit persons to whom the Software is
        // furnished to do so, subject to the following conditions:
        //
        // The above copyright notice and this permission notice shall be included in
        // all copies or substantial portions of the Software.
        //
        // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        // OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
        // THE SOFTWARE.
        //-------------------------------------------------------------------------------
        openFileCallback = [&](const std::string& filename)
        {
            loadRom(filename.c_str(), LoadOptions::None);
        };
        EM_ASM({
            if (typeof(open_file_element) == "undefined")
            {
                open_file_element = document.createElement('input');
                open_file_element.type = "file";
                open_file_element.style.display = "none";
                document.body.appendChild(open_file_element);
                open_file_element.addEventListener("change", function() {
                    var filename = "/upload/" + this.files[0].name;
                    var name = this.files[0].name;
                    this.files[0].arrayBuffer().then(function(buffer) {
                         try { FS.unlink(filename); } catch (exception) { }
                         FS.createDataFile("/upload/", name, new Uint8Array(buffer), true, true, true);
                         var stack = Module.stackSave();
                         var name_ptr = Module.stackAlloc(filename.length * 4 + 1);
                         stringToUTF8(filename, name_ptr, filename.length * 4 + 1);
                         Module._openFileCallbackC(name_ptr);
                         stackRestore(stack);
                        });
                    }, false
                );
                FS.mkdir("/upload");
            }
            open_file_element.value = "";
            open_file_element.accept = '.ch8,.ch10,.hc8,.sc8,.xo8,.c8b,.8o,.gif,.bin,.ram';
            open_file_element.click();
        });
    }
#endif

    void saveConfig()
    {
#ifndef PLATFORM_WEB
        if(!_cfgPath.empty()) {
            auto opt = _properties;
            std::vector<std::string> pal(16, "");
            // TODO. Fix this
            //for(size_t i = 0; i < 16; ++i) {
            //    pal[i] = fmt::format("#{:06x}", _defaultPalette[i] >> 8);
            //}
            // opt.advanced["palette"] = pal;
            _cfg.emuProperties = *_properties;
            _cfg.workingDirectory = _currentDirectory;
            _cfg.libraryPath = _databaseDirectory;
            _cfg.windowPosX = GetWindowPosition().x;
            _cfg.windowPosY = GetWindowPosition().y;
            _cfg.scaleMode = _scaleMode;
            if(!_cfg.save(_cfgPath)) {
                TraceLog(LOG_ERROR, "Couldn't write config to '%s'", _cfgPath.c_str());
            }
        }
#endif
    }

    void updateBehaviorSelects()
    {
        if (auto idx = _cores.classIndex(*_properties); idx >= 0) {
            _behaviorSel = idx;
            _subBehaviorSel = static_cast<int>(emu::CoreRegistry::variantIndex(*_properties).index);
        }
    }

    void whenEmuChanged(emu::IEmulationCore& emu) override
    {
        _debugger.updateCore(&emu);
        _propsMemento = *_properties;
        // TODO: Fix this
        // _editor.updateCompilerOptions(_options.startAddress);
        reloadRom();
        updateBehaviorSelects();
        resetStats();
    }

    void resetStats()
    {
        _ipfAverage.reset();
        _frameTimeAverage_us.reset();
        _frameDelta.reset();
        updateScreen();
    }

    void whenRomLoaded(const std::string& filename, bool autoRun, emu::OctoCompiler* compiler, const std::string& source) override
    {
        _logView.clear();
        _audioBuffer.reset();
        _frameBoost = 1;
        updateBehaviorSelects();
        _editor.setText(source);
        _editor.setFilename(filename);
        resetStats();
        if(compiler)
            _debugger.updateOctoBreakpoints(*compiler);
        saveConfig();
        if(autoRun)
            _mainView = eVIDEO;
        else if(compiler && compiler->isError())
            _mainView = eEDITOR;
    }

    void reloadRom(bool force = false)
    {
        if(!_romImage.empty() || force) {
            _chipEmu->reset();
            _audioBuffer.reset();
            updateScreen();
            // TODO: Fix this
            if(Librarian::isPrefixedTPDRom(_romImage.data(), _romImage.size()))
                std::memcpy(_chipEmu->memory() + 512, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - 512)));
            else
                std::memcpy(_chipEmu->memory() + 512/* TODO: _options.startAddress */, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - 512/* TODO: _options.startAddress */)));
        }
        _debugger.captureStates();
    }

    bool windowShouldClose() const
    {
        return _shouldClose || WindowShouldClose();
    }

private:
    std::mutex _audioMutex;
    ResourceManager _resources;
    gui::StyleManager _styleManager;
    Image _fontImage{};
    Image _microFont{};
    Image _titleImage{};
    Image _icon{};
    Font _font{};
    Image _screen{};
    Image _crt{};
    Image _screenShot{};
    Texture2D _titleTexture{};
    Texture2D _screenTexture{};
    Texture2D _crtTexture{};
    Texture2D _screenShotTexture{};
    Librarian::Screenshot _screenshotData;
    Sha1::Digest _screenShotSha1;
    RenderTexture _keyboardOverlay{};
    CircularBuffer<int16_t,1> _audioBuffer;
    int64_t _audioGaps{};
    bool _shouldClose{false};
    bool _showKeyMap{false};
    int _screenWidth{};
    int _screenHeight{};
    bool _windowInvisible{false};
    VideoRenderMode _videoRenderMode{eFAST};
    std::unique_ptr<TextureScaler> _textureScaler;
    //RenderTexture _renderTexture{};
    AudioStream _audioStream{};
    float _volumeSlider{0.5f};
    float _volume{0.5f};
    SMA<60,uint64_t> _ipfAverage;
    SMA<120,uint32_t> _frameTimeAverage_us;
    SMA<120,int> _frameDelta;
    emu::FpsMeasure _fps;
    int _partialFrameTime{0};
    int _scaleMode{1};
    int _behaviorSel{0};
    int _subBehaviorSel{0};
    //float _messageTime{};
    std::string _timedMessage;
    bool _renderCrt{false};
    bool _updateScreen{false};
    int _frameBoost{1};
    std::atomic_uint _audioCallbackAvgFrames{};
    bool _colorSelectOpen{false};
    uint32_t* _selectedColor{nullptr};
    std::string _colorText;
    uint32_t _previousColor{};
    Rectangle _screenOverlay{};
    int _currentMonitor{0};
    int _screenScale{1};
    emu::Properties _propsMemento{};
    std::string _avgGuiRenderTime;

    //std::string _romName;
    //std::vector<uint8_t> _romImage;
    //std::string _romSha1Hex;
    //bool _romIsWellKnown{false};
    //emu::Chip8EmulatorOptions _romWellKnownOptions;
    std::array<double,16> _keyScanTime{};
    std::array<bool,16> _keyMatrix{};
    volatile bool _grid{false};
    MainView _mainView{eDEBUGGER};
    MainView _lastView{eDEBUGGER};
    MainView _lastRunView{eDEBUGGER};
    Debugger _debugger;
    LogView _logView;
    Editor _editor;
    Shader _scanLineShader;

    inline static KeyboardKey _keyMapping[16] = {KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q, KEY_W, KEY_E, KEY_A, KEY_S, KEY_D, KEY_Z, KEY_C, KEY_FOUR, KEY_R, KEY_F, KEY_V};
    inline static int _keyPosition[16] = {1,2,3,12, 4,5,6,13, 7,8,9,14, 10,0,11,15};
    inline static Cadmium* _instance{};
};

#ifndef PLATFORM_WEB
std::string dumOctoStateLine(octo_emulator* octo)
{
    return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}",
    octo->v[0], octo->v[1], octo->v[2], octo->v[3], octo->v[4], octo->v[5], octo->v[6], octo->v[7], 
    octo->v[8], octo->v[9], octo->v[10], octo->v[11], octo->v[12], octo->v[13], octo->v[14], octo->v[15],
    octo->i, octo->rp, octo->pc, (octo->ram[octo->pc]<<8)|octo->ram[octo->pc+1]);
}
#endif

std::string chip8EmuScreen(emu::IEmulationCore& chip8)
{
    std::string result;
    auto width = chip8.getCurrentScreenWidth();
    auto maxWidth = 256; //chip8.getMaxScreenWidth();
    auto height = chip8.getCurrentScreenHeight();
    const auto* screen = chip8.getScreen();
    if(screen) {
        result.reserve(width * height + height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                result.push_back(screen->getPixel(x, y) ? '#' : '.');
            }
            result.push_back('\n');
        }
    }
    return result;
}

std::string chip8EmuScreenANSI(emu::IEmulationCore& chip8)
{
    static int col[16] = {0,15,7,8, 9,10,12,11, 1,2,4,3, 13,14,5,6};
    std::string result;
    auto width = chip8.getCurrentScreenWidth();
    auto maxWidth = 256; //chip8.getMaxScreenWidth();
    auto height = chip8.getCurrentScreenHeight();
    if (const auto* screen = chip8.getScreen()) {
        result.reserve(width * height * 16);
        if (chip8.isDoublePixel()) {
            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 2) {
                    auto c1 = screen->getPixel(x, y);
                    auto c2 = screen->getPixel(x, y + 2);
                    result += fmt::format("\033[38;5;{}m\033[48;5;{}m\xE2\x96\x84", col[c2 & 15], col[c1 & 15]);
                    // result.push_back(buffer[y*maxWidth + x] ? '#' : '.');
                }
                result += "\033[0m\n";
            }
        }
        else {
            for (int y = 0; y < height; y += 2) {
                for (int x = 0; x < width; ++x) {
                    auto c1 = screen->getPixel(x, y);
                    auto c2 = screen->getPixel(x, y + 1);
                    result += fmt::format("\033[38;5;{}m\033[48;5;{}m\xE2\x96\x84", col[c2 & 15], col[c1 & 15]);
                    // result.push_back(buffer[y*maxWidth + x] ? '#' : '.');
                }
                result += "\033[0m\n";
            }
        }
    }
    return result;
}

#ifndef PLATFORM_WEB
std::string octoScreen(octo_emulator& octo)
{
    std::string result;
    result.reserve(65*32+1);
    for(int y = 0; y < 32; ++y) {
        for(int x = 0; x < 64; ++x) {
            result.push_back(octo.px[y*64 + x] ? '#' : ' ');
        }
        result.push_back('\n');
    }
    return result;
}
#endif

#ifdef PLATFORM_WEBx
extern "C" {

EMSCRIPTEN_KEEPALIVE int load_file(uint8_t *buffer, size_t size) {
    /// Load a file - this function is called from javascript when the file upload is activated
    std::cout << "load_file triggered, buffer " << &buffer << " size " << size << std::endl;

    // do whatever you need with the file contents

    return 1;
}

}
#endif

std::string formatOpcodeString(emu::OpcodeType type, uint16_t opcode)
{
    static std::string patterns[] = {"FFFF", "FFFn", "FFnn", "Fnnn", "FxyF", "FxFF", "Fxyn", "Fxnn", "FFyF"};
    //                              0xFFFF, 0xFFF0, 0xFF00, 0xF000, 0xF00F, 0xF0FF, 0xF000, 0xF000, 0xFF0F
    auto opStr = fmt::format("{:04X}", opcode);
    for(size_t i = 0; i <4; ++i) {
        if(std::islower((uint8_t)patterns[type][i]))
            opStr[i] = patterns[type][i];
    }
    return opStr;
}

std::string formatOpcode(emu::OpcodeType type, uint16_t opcode)
{
    auto opStr = formatOpcodeString(type, opcode);
    auto dst = opStr;
    std::transform(dst.begin(), dst.end(), dst.begin(), [](unsigned char c){ return std::tolower(c); });
    return fmt::format("<a href=\"https://chip8.gulrak.net/reference/opcodes/{}\">{}</a>", dst, opStr);
}

void dumpOpcodeTable(std::ostream& os, emu::Chip8Variant variants = (emu::Chip8Variant)0x3FFFFFFFFFFFFFFF)
{
    std::regex quirkRE(R"(\s*\[Q:([^\]]+)\])");
    std::map<std::string, size_t> quirkMap;
    std::vector<std::string> quirkList;
    os << R"(<!DOCTYPE html><html><head><title>CHIP-8 Variant Opcode Table</title>
<style>
body { background: #1b1b1f; color: azure; font-family: Verdana, sans-serif; }
a { color: #8bf; }
table { border: 2px solid #ccc; border-collapse: collapse; }
th { border: 2px solid #ccc; padding: 0.5em; }
td { text-align: center; border: 2px solid #ccc; padding: 0.5em; }
td.clean { background-color: #080; }
td.quirked { background-color: #880; }
td.desc { text-align: left; }
th.rotate { height: 100px; white-space: nowrap; }
th.rotate > div { transform: translate(0px, 2em) rotate(-90deg); width: 30px; }
div.footer { font-size: 0.7em; }
</style></head>
<body><h2>CHIP-8 Variant Opcode Table</h2>
<table class="opcodes"><tr><th class="opcodes">Opcode</th>)";
    auto mask = static_cast<uint64_t>(variants);
    while(mask) {
        auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
        mask &= mask - 1;
        os << R"(<th class="rotate"><div><span>)" << emu::Chip8Decompiler::chipVariantName(cv).first << "</span></div></th>";
    }
    os << "<th>Description</th></tr>";
    for(const auto& info : emu::detail::opcodes) {
        if(uint64_t(info.variants & variants) != 0) {
            os << "<tr><th>" << formatOpcode(info.type, info.opcode) << "</th>";
            mask = static_cast<uint64_t>(variants);
            auto desc = info.description;
            std::smatch m;
            size_t qidx = 0;
            while (std::regex_search(desc, m, quirkRE)) {
                auto iter = quirkMap.find(m[1]);
                if (iter == quirkMap.end()) {
                    quirkMap.emplace(m[1], quirkList.size() + 1);
                    quirkList.push_back(m[1]);
                    qidx = quirkList.size();
                }
                else
                    qidx = iter->second;
                desc = desc.replace(m[0].first, m[0].second, fmt::format(" [<a href=\"#quirk{}\">Quirk {}</a>]", qidx, qidx));
            }
            while (mask) {
                auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
                mask &= mask - 1;
                if ((info.variants & cv) == cv) {
                    if (qidx)
                        os << "<td class=\"quirked\">&#x2713;</td>";
                    else
                        os << "<td class=\"clean\">&#x2713;</td>";
                }
                else
                    os << "<td></td>";
            }
            os << R"(<td class="desc">)" << desc << "</td></tr>" << std::endl;
        }
    }
    os << "</table>\n<ul>";
    size_t qidx = 1;
    for(const auto& quirk : quirkList) {
        os << "<li id=\"quirk" << qidx << "\"> Quirk " << qidx << ": " << quirk << "</li>\n";
        ++qidx;
    }
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    os << "</ul><div class=\"footer\">Generated by Cadmium v" << CADMIUM_VERSION << ", on " << std::put_time( std::gmtime( &t ), "%F" ) << "</div></body></html>";
}

void dumpOpcodeJSON(std::ostream& os, emu::Chip8Variant variants = (emu::Chip8Variant)0x3FFFFFFFFFFFFFFF)
{
    using namespace nlohmann;
    ordered_json root = ordered_json::object({});
    ordered_json collection = json::array({});
    std::regex quirkRE(R"(\s*\[Q:([^\]]+)\])");
    std::map<std::string, size_t> quirkMap;
    std::vector<std::string> quirkList;
    for(const auto& info : emu::detail::opcodes) {
        if(uint64_t(info.variants & variants) != 0) {
            auto obj = ordered_json::object({});
            obj["opcode"] = formatOpcodeString(info.type, info.opcode);
            obj["mask"] = emu::detail::opcodeMasks[info.type];
            obj["size"] = info.size;
            obj["octo"] = info.octo;
            auto mnemonic = info.octo.substr(0, info.octo.find(" "));
            if(emu::detail::octoMacros.count(mnemonic)) {
                obj["macro"] = emu::detail::octoMacros.at(mnemonic);
            }
            if(!info.mnemonic.empty()) {
                obj["chipper"] = info.mnemonic;
            }
            obj["platforms"] = json::array();
            auto mask = static_cast<uint64_t>(variants & info.variants);
            while(mask) {
                auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
                mask &= mask - 1;
                obj["platforms"].push_back(emu::Chip8Decompiler::chipVariantName(cv).first);
            }
            auto desc = info.description;
            std::smatch m;
            size_t qidx = 0;
            ordered_json quirks = json::array({});
            while (std::regex_search(desc, m, quirkRE)) {
                auto iter = quirkMap.find(m[1]);
                if (iter == quirkMap.end()) {
                    quirkMap.emplace(m[1], quirkList.size());
                    qidx = quirkList.size();
                    quirkList.push_back(trim(m[1]));
                }
                else
                    qidx = iter->second;
                quirks.push_back(qidx);
                desc = desc.replace(m[0].first, m[0].second, "");
            }
            obj["description"] = trim(desc);
            if(!quirks.empty())
                obj["quirks"] = quirks;
            collection.push_back(obj);
        }
    }
    root["generator"] = "Cadmium";
    root["version"] = CADMIUM_VERSION " " CADMIUM_GIT_HASH;
    std::stringstream oss;
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    oss << std::put_time( std::gmtime( &t ), "%F" );
    root["date"] = oss.str();
    root["opcodes"] = collection;
    root["quirks"] = json(quirkList);
    os << root.dump() << std::endl;
}

void dumpLibraryNickel()
{
#if 0
    std::set<std::pair<std::string,std::string>> variantSet;
    size_t romCount = 0;
    for(size_t i = 0; i < Librarian::numKnownRoms(); ++i) {
        const auto& rom = Librarian::getRomInfo(i);
        auto options = Librarian::getOptionsForSha1(rom.sha1);
        if ((options.behaviorBase != emu::Chip8EmulatorOptions::eCHIP8 &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eSCHIP11 &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eSCHIP_MODERN &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eSCHPC &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eXOCHIP) ||
            options.optLoadStoreIncIByX || options.optOnlyHires || options.optCyclicStack) {
            std::cout << fmt::format("    //{{0x{}, {{{}, {}, {}}}}},  // {}", std::string(rom.sha1, 8), emu::Chip8EmulatorOptions::shortNameOfPreset(options.behaviorBase), "???", options.instructionsPerFrame, rom.name ? rom.name : "") << std::endl;
        }
        else {
            std::string variant = "CHIP8";
            std::ostringstream quirks;
            //{ LogicQuirk = 1, ShiftQuirk = 2, MemoryIUnchanged = 4, JumpQuirk = 8, BytePixel = 16, DisplayWait = 32, ColorSupport = 64, Lores16Pixel = 128, Wrapping = 256 };
            if (!options.optDontResetVf)
                quirks << "|LogicQuirk";
            if (options.optJustShiftVx)
                quirks << "|ShiftQuirk";
            if (options.optLoadStoreDontIncI)
                quirks << "|MemoryIUnchanged";
            if (options.optJump0Bxnn)
                quirks << "|JumpQuirk";
            if (!options.optInstantDxyn)
                quirks << "|DisplayWait";
            if (options.optWrapSprites)
                quirks << "|Wrapping";
            if (options.optLoresDxy0Is16x16)
                quirks << "|Lores16Pixel";
            if (options.optAllowColors)
                quirks << "|ColorSupport|BytePixel";
            if (options.optSCLoresDrawing)
                quirks << "|ColorSupport|BytePixel|SCLoresDraw";
            if (options.optHas16BitAddr)
                variant = "XOCHIP";
            auto quirkStr = quirks.str();
            quirkStr = quirkStr.empty() ? "0" : quirkStr.substr(1);
            std::cout << fmt::format("    {{0x{}, {{{}, {}, {}}}}},  // {}: {}", std::string(rom.sha1, 16), variant, quirkStr, options.instructionsPerFrame, emu::Chip8EmulatorOptions::shortNameOfPreset(options.behaviorBase), rom.name ? rom.name : "") << std::endl;
            variantSet.insert({variant, quirkStr});
            ++romCount;
        }
    }
    std::cout << "------ " << variantSet.size() << " variants in " << romCount << " roms" << std::endl;
    for(const auto& vari : variantSet) {
        std::cout << "VARIANT(" << vari.first << ", " << (vari.first == "XOCHIP" ? 65536 : 4096) << ", " << vari.second << ")" << std::endl;
    }
#endif
}

void convertKnownRomList()
{
#ifndef NEW_ROMLIST_FORMAT
    std::map<std::string, KnownRomInfo2> knownRoms;
    std::set<std::string> optionsStrings;
    std::set<std::string> keys;
    std::set<std::string> advancedKeys;
    c8db::Database db("/Users/schuemann/Development/c8/chip-8-database/database");

    for(size_t i = 0; i < Librarian::numKnownRoms(); ++i) {
        auto info = Librarian::getRomInfo(i);
        const char* preset = nullptr;
        bool unsure = false;
        switch(info.variant) {
            case emu::chip8::Variant::CHIP_8: preset = "!chip-8"; break;
            case emu::chip8::Variant::CHIP_10: preset = "!chip-10"; break;
            case emu::chip8::Variant::CHIP_8E: preset = "!chip-8e"; break;
            case emu::chip8::Variant::CHIP_8X: preset = "!chip-8x"; break;
            case emu::chip8::Variant::SCHIP_1_0: preset = "!schip-1.0"; break;
            case emu::chip8::Variant::SCHIP_1_1: preset = "!schip-1.1"; break;
            case emu::chip8::Variant::SCHIPC: preset = "!schipc"; break;
            case emu::chip8::Variant::SCHIP_MODERN: preset = "!schip-modern"; break;
            case emu::chip8::Variant::MEGA_CHIP: preset = "!megachip"; break;
            case emu::chip8::Variant::XO_CHIP: preset = "!xo-chip"; break;
            case emu::chip8::Variant::COSMAC_VIP: preset = "!vip"; break;
            case emu::chip8::Variant::CHIP_8_COSMAC_VIP: preset = "!vip-chip-8"; break;
            case emu::chip8::Variant::CHIP_8_TPD: preset = "!vip-chip-8-tpd"; break;
            case emu::chip8::Variant::CHIP_8X_TPD: preset = "!vip-chip-8x-tpd"; break;
            case emu::chip8::Variant::HI_RES_CHIP_8: preset = "!vip-chip-8-fpd"; break;
            case emu::chip8::Variant::HI_RES_CHIP_8X: preset = "!vip-chip-8x-fpd"; break;
            case emu::chip8::Variant::GENERIC_CHIP_8: preset = "!generic-chip-8"; break;
            default: preset = "?chip-8"; unsure = true; break;
        }
        const char* optionsString = nullptr;
        if(info.options) {
            auto options = nlohmann::json::parse(info.options);
            if(!options.contains("optAllowHires")) {
                for(const auto& [key, val] : options.items()) {
                    keys.insert(key);
                }
                auto presetProperties = emu::CoreRegistry::propertiesForPreset(preset);
                auto romProperties = presetProperties;
                if(preset) {
                    if(options.contains("instructionsPerFrame")) {
                        romProperties.at("instructionsPerFrame").setInt(options.at("instructionsPerFrame"));
                    }
                    if(options.contains("optDontResetVf")) {
                        romProperties.at("8xy1/8xy2/8xy3 don't reset VF").setBool(options.at("optDontResetVf"));
                    }
                    if(options.contains("optInstantDxyn")) {
                        romProperties.at("Dxyn doesn't wait for vsync").setBool(options.at("optInstantDxyn"));
                    }
                    if(options.contains("optJustShiftVx")) {
                        romProperties.at("8xy6/8xyE just shift VX").setBool(options.at("optJustShiftVx"));
                    }
                    if(options.contains("optLoadStoreDontIncI")) {
                        romProperties.at("Fx55/Fx65 increment I by X + 1").setBool(!options.at("optLoadStoreDontIncI").get<bool>());
                        romProperties.at("Fx55/Fx65 increment I by X").setBool(false);
                    }
                    if(options.contains("optWrapSprites")) {
                        romProperties.at("wrap Sprite pixels").setBool(options.at("optWrapSprites"));
                    }
                    if(options.contains("advanced")) {
                        for(const auto& [akey, avalue] : options.at("advanced").items()) {
                            advancedKeys.insert(akey);
                            auto& palette = romProperties.palette();
                            if(akey == "col0") {
                                if(palette.colors.size() < 2)
                                    romProperties.palette().colors.resize(2, emu::Palette::Color{0});
                                palette.colors[0] = emu::Palette::Color(avalue.get<std::string>());
                            }
                            else if(akey == "col1") {
                                if(palette.colors.size() < 2)
                                    romProperties.palette().colors.resize(2, emu::Palette::Color{0});
                                palette.colors[1] = emu::Palette::Color(avalue.get<std::string>());
                            }
                            else if(akey == "col2") {
                                if(palette.colors.size() < 4)
                                    romProperties.palette().colors.resize(4, emu::Palette::Color{0});
                                palette.colors[2] = emu::Palette::Color(avalue.get<std::string>());
                            }
                            else if(akey == "col3") {
                                if(palette.colors.size() < 4)
                                    romProperties.palette().colors.resize(4, emu::Palette::Color{0});
                                palette.colors[3] = emu::Palette::Color(avalue.get<std::string>());
                            }
                            else if(akey == "buzzColor") {
                                palette.signalColor = emu::Palette::Color(avalue.get<std::string>());
                            }
                            else if(akey == "quietColor") {
                                palette.borderColor = emu::Palette::Color(avalue.get<std::string>());
                            }
                            else if(akey == "palette") {
                                palette.colors.clear();
                                for(const std::string col : avalue) {
                                    palette.colors.emplace_back(col);
                                }
                            }
                            else if (akey == "screenRotation") {
                                romProperties.at("screenRotation").setSelectedText(std::to_string(int(avalue)));
                            }
                            else if (akey == "fontStyle") {
                                romProperties.at("fontStyle").setSelectedText(avalue);
                            }
                        }
                    }
                }
                auto diff = presetProperties.createDiff(romProperties);
                optionsString = optionsStrings.insert("R\"(" + diff.dump() + ")\"").first->c_str();
            }
        }
        auto info2 = KnownRomInfo2{Sha1::Digest(info.sha1), preset, info.name, optionsString, nullptr};
        knownRoms.emplace(info.sha1, info2);
    }
    std::cout << "static KnownRomInfo g_knownRoms[] = {" << std::endl;
    for(const auto& [key, info] : knownRoms) {
        std::cout << "    {\"" << key << "\"_sha1";
        std::cout << ", \"" << info.preset+1 << "\"";
        std::cout << ", \"" << info.name << "\"";
        std::cout << "," << (info.options ? info.options : "nullptr") << ", " << (info.url ? info.url : "nullptr");
        std::cout << "},";
        if(*info.preset == '?') {
            std::cout << " // ???";
        }
        std::cout << std::endl;
    }
    std::cout << "};" << std::endl;
    for(const auto& key : keys) {
        std::cout << "Option: " << key << std::endl;
    }
    for(const auto& akey : advancedKeys) {
        std::cout << "Advanced Option: " << akey << std::endl;
    }
    std::cout << "Found " << db.numRoms() << " rom files in programs.json" << std::endl;
    size_t dbNew = 0;
    for(const auto& [key, info] : db.romTable()) {
        if(!knownRoms.contains(key)) {
            std::cout << "Unknown rom in db: " << key << ", " << info->title << std::endl;
            dbNew++;
        }
    }
    size_t cadNew = 0;
    for(const auto& [key, info] : knownRoms) {
        if(!db.romTable().contains(key)) {
            //std::cout << "Rom missing in db: " << key << ", " << info.name << std::endl;
            cadNew++;
        }
    }
    std::cout << "Chip-8 Program database contains " << dbNew << " roms new to Cadmium," << std::endl;
    std::cout << "Cadmium detects " << cadNew << " roms not in the database." << std::endl;
    std::cout << "Done converting " << knownRoms.size() << " rom infos." << std::endl;
#endif
}

int main(int argc, char* argv[])
{
    static emu::Properties coreProperties;
    ghc::CLI cli(argc, argv);
    int64_t traceLines = -1;
    bool compareRun = false;
    int64_t benchmark= 0;
    bool showHelp = false;
    bool opcodeTable = false;
    bool opcodeJSON = false;
    bool dumpLibNickel = false;
    bool convertRomList = false;
    bool startRom = false;
    bool screenDump = false;
    bool drawDump = false;
    std::string dumpInterpreter;
    int64_t execSpeed = -1;
    std::string randomGen;
    std::string emulationCore;
    int64_t randomSeed = 12345;
    std::vector<std::string> romFile;
    std::string presetName;
    int64_t testSuiteMenuVal = 0;
    cli.category("General Options");
#ifndef PLATFORM_WEB
    cli.option({"-h", "--help"}, showHelp, "Show this help text");
    cli.option({"-t", "--trace"}, traceLines, "Run headless and dump given number of trace lines");
    cli.option({"-c", "--compare"}, compareRun, "Run and compare with reference engine, trace until diff");
    cli.option({"-b", "--benchmark"}, benchmark, "Run given number of cycles as benchmark");
    cli.option({"--screen-dump"}, screenDump, "When in trace mode, dump the final screen content to the console");
    cli.option({"--draw-dump"}, drawDump, "Dump screen after every draw when in trace mode.");
    cli.option({"--test-suite-menu"}, testSuiteMenuVal, "Sets 0x1ff to the given value before starting emulation in trace mode, useful for test suite runs.");
    cli.option({"--opcode-json"}, opcodeJSON, "Dump opcode information as JSON to stdout");
#ifndef NDEBUG
    cli.option({"--dump-interpreter"}, dumpInterpreter, "Dump the given interpreter in a local file named '<interpreter>.ram' and exit");
    cli.option({"--dump-library-nickel"}, dumpLibNickel, "Dump library table for Nickel");
    cli.option({"--convert-rom-list"}, convertRomList, "Convert list of known roms (just temporary available)");
#endif
#else
#ifdef WEB_WITH_FETCHING
    std::string urlLoad;
    cli.option({"-u", "--url"}, urlLoad, "An url that will be tried to load a rom or source from");
#endif
#endif
    cli.option({"-r", "--run"}, startRom, "if a ROM is given (positional) start it");
    emu::CoreRegistry reg{};
    std::string coresAvailable;
    std::string presetsDescription;
    for(const auto& [name, info] : reg) {
        //std::cout << toOptionName(name) << std::endl;
        coresAvailable += fmt::format("        {} - {}\n", toOptionName(name), info->description);
        presetsDescription += fmt::format("        {}:\n", info->description);
        for(size_t i = 0; i < info->numberOfVariants(); ++i) {
            if(info->prefix().empty())
                presetsDescription += fmt::format("            {} - {} ({})\n", toOptionName(info->variantName(i)), info->variantDescription(i), info->variantExtensions(i));
            else
                presetsDescription += fmt::format("            {} - {} ({})\n", toOptionName(info->prefix() + '-' + info->variantName(i)), info->variantDescription(i), info->variantExtensions(i));
        }
        auto proto = info->propertiesPrototype();
        auto oldCat = cli.category(fmt::format("{} Options (only available if preset uses {} core)", name, info->prefix().empty() ? "default" : toOptionName(info->prefix())));
        for(size_t i = 0; i < proto.numProperties(); ++i) {
            auto& prop = proto[i];
            if(prop.access() == emu::eWritable) {
                auto dependencyCheck = [&presetName, info]() {
                    return info->hasVariant(presetName);
                };
                std::visit(emu::visitor{
                   [](std::nullptr_t) -> void {  },
                   [dependencyCheck,&prop,&cli](bool& val) -> void { cli.option<bool>({fmt::format("--{}", prop.getOptionName())}, [](const std::string& paramName, const bool& value) {
                       coreProperties.at(paramName).setBool(value);
                   }, prop.getDescription()).dependsOn(dependencyCheck); },
                   [dependencyCheck,&prop,&cli](emu::Property::Integer& val) -> void { cli.option<std::string>({fmt::format("--{}", prop.getOptionName())}, [](const std::string& paramName, const std::string& value) {
                       coreProperties.at(paramName).setString(value);
                   }, prop.getDescription()).dependsOn(dependencyCheck).range(prop.getIntMin(), prop.getIntMax()); },
                   [dependencyCheck,&prop,&cli](std::string& val) -> void { cli.option<int>({fmt::format("--{}", prop.getOptionName())}, [](const std::string& paramName, const int& value) {
                       coreProperties.at(paramName).setInt(value);
                   }, prop.getDescription()).dependsOn(dependencyCheck); },
                   [dependencyCheck,&prop,&cli](emu::Property::Combo& val) -> void {
                       const emu::Property::Combo& combo = std::get<emu::Property::Combo>(prop.getValue());
                       std::ostringstream optionList;
                       for (auto i = combo.options.begin(); i != combo.options.end(); ++i) {
                           if (i != combo.options.begin()) {
                               optionList << ", ";
                           }
                           optionList << toOptionName(*i);
                       }
                       cli.option<ghc::CLI::Combo>({fmt::format("--{}", prop.getOptionName())}, [](const std::string& paramName, const ghc::CLI::Combo& value) {
                            coreProperties.at(paramName).setSelectedIndex(value.index);
                       }, prop.getDescription() + " (" + optionList.str() +")").dependsOn(dependencyCheck);
                   }
               }, prop.getValue());
            }
        }
        cli.category(oldCat);
    }
    cli.option({"-p", "--preset"}, presetName, "Select one of the following available preset:\n" + trimRight(presetsDescription), [&presetName](std::string) {
        // Todo: Set coreProperties to a matching instance
        coreProperties = emu::CoreRegistry::propertiesForPreset(presetName);
        if(!coreProperties) {
            throw std::runtime_error(fmt::format("Unknown preset: '{}' (use --help to see supported presets)", presetName));
        }
    });
#if 0
    cli.option({"-s", "--exec-speed"}, execSpeed, "Set execution speed in instructions per frame (0-500000, 0: unlimited)");
    cli.option({"--random-gen"}, randomGen, "Select a predictable random generator used for trace log mode (rand-lgc or counting)");
    cli.option({"--random-seed"}, randomSeed, "Select a random seed for use in combination with --random-gen, default: 12345");
    cli.option({"--trace-log"}, options.optTraceLog, "If true, enable trace logging into log-view");
    //cli.option({"--opcode-table"}, opcodeTable, "Dump an opcode table to stdout");
    cli.category("Quirks");
    cli.option({"--just-shift-vx"}, options.optJustShiftVx, "If true, 8xy6/8xyE will just shift Vx and ignore Vy");
    cli.option({"--dont-reset-vf"}, options.optDontResetVf, "If true, Vf will not be reset by 8xy1/8xy2/8xy3");
    cli.option({"--load-store-inc-i-by-x"}, options.optLoadStoreIncIByX, "If true, Fx55/Fx65 increment I by x");
    cli.option({"--load-store-dont-inc-i"}, options.optLoadStoreDontIncI, "If true, Fx55/Fx65 don't change I");
    cli.option({"--wrap-sprites"}, options.optWrapSprites, "If true, Dxyn wrap sprites around border");
    cli.option({"--instant-dxyn"}, options.optInstantDxyn, "If true, Dxyn don't wait for vsync");
    cli.option({"--lores-dxy0-width-8"}, options.optLoresDxy0Is8x16, "If true, draw Dxy0 sprites have width 8");
    cli.option({"--lores-dxy0-width-16"}, options.optLoresDxy0Is16x16, "If true, draw Dxy0 sprites have width 16");
    cli.option({"--sc11-collision"}, options.optSC11Collision, "If true, use SCHIP1.1 collision logic");
    cli.option({"--half-pixel-scroll"}, options.optHalfPixelScroll, "If true, use SCHIP1.1 lores half pixel scrolling");
    cli.option({"--mode-change-clear"}, options.optModeChangeClear, "If true, clear screen on lores/hires changes");
    cli.option({"--jump0-bxnn"}, options.optJump0Bxnn, "If true, use Vx as offset for Bxnn");
    cli.option({"--allow-hires"}, options.optAllowHires, "If true, support for hires (128x64) is enabled");
    cli.option({"--only-hires"}, options.optOnlyHires, "If true, emulation has hires mode only");
    cli.option({"--allow-color"}, options.optAllowColors, "If true, support for multi-plane drawing is enabled");
    cli.option({"--cyclic-stack"}, options.optCyclicStack, "If true, stack operations wrap around, overwriting used slots");
    cli.option({"--has-16bit-addr"}, options.optHas16BitAddr, "If true, address space is 16bit (64k ram)");
    cli.option({"--xo-chip-sound"}, options.optXOChipSound, "If true, use XO-CHIP sound instead of buzzer");
    cli.option({"--extended-display-wait"}, options.optExtendedVBlank, "If true, Dxyn might even wait 2 screens depending on size and position");
#endif
    auto extensions = join(reg.getSupportedExtensions().begin(), reg.getSupportedExtensions().end(), ", ");
    cli.positional(romFile, fmt::format("ROM file or source to load ({})", extensions));

    CadmiumConfiguration config;
#ifndef PLATFORM_WEB
    auto cfgPath = (fs::path(dataPath())/"config.json").string();
    if(config.load(cfgPath)) {
        coreProperties = config.emuProperties;
    }
#endif
    try {
        cli.parse();
    }
    catch(std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        exit(1);
    }
    if(showHelp) {
        cli.usage();
        exit(0);
    }
#ifndef PLATFORM_WEB
    if(convertRomList) {
        convertKnownRomList();
        exit(0);
    }
    if(opcodeTable) {
        dumpOpcodeTable(std::cout, emu::C8V::CHIP_8|emu::C8V::CHIP_10|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP);
        exit(0);
    }
    if(opcodeJSON) {
        dumpOpcodeJSON(std::cout, emu::C8V::CHIP_8|emu::C8V::CHIP_8_I|emu::C8V::CHIP_8X|emu::C8V::CHIP_8E|emu::C8V::CHIP_10|emu::C8V::CHIP_8_D6800|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::SCHIPC|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP);
        exit(0);
    }
    if(dumpLibNickel) {
        dumpLibraryNickel();
        exit(0);
    }
    if(!dumpInterpreter.empty()) {
        // TODO: Fix this
        /*
        auto data = emu::CosmacVIP::getInterpreterCode(toUpper(dumpInterpreter));
        if(!data.empty()) {
            {
                std::ofstream os(dumpInterpreter + ".ram", std::ios::binary);
                os.write((const char*)data.data(), static_cast<ssize_t>(data.size()));
            }
            std::cout << "Written " << data.size() << " bytes to '" << dumpInterpreter << ".ram'." << std::endl;
            exit(0);
        }
        else {
            std::cerr << "ERROR: Unknown interpreter '" << dumpInterpreter << "'." << std::endl;
            exit(1);
        }
        */
    }
#endif
    if(romFile.size() > 1) {
        std::cerr << "ERROR: only one ROM/source file supported" << std::endl;
        exit(1);
    }
    if(romFile.empty() && startRom) {
        std::cerr << "ERROR: can't start anything without a ROM/source file" << std::endl;
        exit(1);
    }
    if(!randomGen.empty() && (traceLines<0 || (randomGen != "rand-lgc" && randomGen != "counting"))) {
        std::cerr << "ERROR: random generator must be 'rand-lgc' or 'counting' and trace must be used." << std::endl;
        exit(1);
    }
#ifndef PLATFORM_WEB
    //if(execSpeed >= 0) {
    //    options.instructionsPerFrame = execSpeed;
    //}
    if(traceLines < 0 && !compareRun && !benchmark) {
#else
    ghc::CLI cli(argc, argv);
    std::string presetName = "schipc";
    int64_t execSpeed = -1;
    cli.option({"-p", "--preset"}, presetName, "Select CHIP-8 preset to use: chip-8, chip-10, chip-48, schip1.0, schip1.1, megachip8, xo-chip of vip-chip-8");
    cli.option({"-s", "--exec-speed"}, execSpeed, "Set execution speed in instructions per frame (0-500000, 0: unlimited)");
    try {
        cli.parse();
    }
    catch(std::exception& ex)
    {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        exit(1);
    }
    if(!presetName.empty()) {
        try {
            preset = emu::Chip8EmulatorOptions::presetForName(presetName);
        }
        catch(std::runtime_error e) {
            std::cerr << "ERROR: " << e.what() << ", check help for supported presets." << std::endl;
            exit(1);
        }
    }
    auto chip8options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
    if(execSpeed >= 0) {
        chip8options.instructionsPerFrame = execSpeed;
    }
    {
#endif

#ifndef PLATFORM_WEB
        Cadmium cadmium(config, coreProperties);
        if (!romFile.empty()) {
            Cadmium::LoadOptions loadOpt = Cadmium::LoadOptions::None;
            if(startRom)
                loadOpt |= Cadmium::LoadOptions::SetToRun;
            if(!presetName.empty())
                loadOpt |= Cadmium::LoadOptions::DontChangeOptions;
            cadmium.loadRom(romFile.front().c_str(), loadOpt);
        }
        //SetTargetFPS(60);
        while (!cadmium.windowShouldClose()) {
            cadmium.updateAndDraw();
        }
#else
        try {
            Cadmium cadmium(presetName.empty() ? nullptr : &chip8options);
#ifdef WEB_WITH_FETCHING
            if(!urlLoad.empty()) {
                auto ri = Librarian::findKnownRom(urlLoad);
                if(ri && ri->url) {
                    if(startsWith(ri->url, "@GH")) {
                        urlLoad = "https://raw.githubusercontent.com" + std::string(ri->url + 3);
                    }
                    else {
                        urlLoad = ri->url;
                    }
                }
                emscripten_fetch_attr_t attr;
                emscripten_fetch_attr_init(&attr);
                strcpy(attr.requestMethod, "GET");
                attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                attr.onsuccess = loadBinaryCallbackC;
                attr.onerror = downloadFailedCallbackC;
                Cadmium::LoadOptions loadOpt = Cadmium::LoadOptions::None;
                if(!presetName.empty())
                    loadOpt |= Cadmium::LoadOptions::DontChangeOptions;
                loadBinaryCallback = [&](std::string filename, const uint8_t* data, size_t size) { cadmium.loadBinary(filename, data, size, loadOpt); };
                emscripten_fetch(&attr, urlLoad.c_str());
            }
#endif
            emscripten_set_main_loop_arg(Cadmium::updateAndDrawFrame, &cadmium, 0, 1);
        }
        catch(std::exception& ex) {
            std::cerr << "Exception: " << ex.what() << std::endl;
        }
#endif
    }
#if !defined(PLATFORM_WEB)
    else {
        emu::HeadlessHost host;
        //chip8options.optHas16BitAddr = true;
        //chip8options.optWrapSprites = true;
        //chip8options.optAllowColors = true;
        //chip8options.optJustShiftVx = false;
        //chip8options.optLoadStoreDontIncI = false;
        //chip8options.optDontResetVf = true;
        //chip8options.optInstantDxyn = true;
        //options.optExtendedVBlank = false;
        //if(!randomGen.empty()) {
        //    options.advanced = nlohmann::ordered_json::object({
        //        {"random", randomGen},
        //        {"seed", randomSeed}
        //    });
        //    options.updatedAdvanced();
        //}
        host.updateEmulatorOptions(coreProperties);
        auto* chip8 = dynamic_cast<emu::IChip8Emulator*>(host.emuCore().executionUnit(0));
        if (!chip8) {
            std::cerr << "Selected core is not capable of CHIP-8 control." << std::endl;
            exit(1);
        }
        std::clog << "Engine:  " << chip8->name() << ", active variant: " << presetName << std::endl;
        octo_emulator octo;
        octo_options oopt{};
        oopt.q_clip = 1;
        //oopt.q_loadstore = 1;

        chip8->reset();
        if(!romFile.empty()) {
            int size = 0;
            uint8_t* data = LoadFileData(romFile.front().c_str(), &size);
            if (size < chip8->memSize() - 512) {
                std::memcpy(chip8->memory() + 512, data, size);
            }
            UnloadFileData(data);
            //chip8.loadRom(romFile.c_str());
        }
        int64_t i = 0;
        if(compareRun) {
            octo_emulator_init(&octo, (char*)chip8->memory() + 512, 4096 - 512, &oopt, nullptr);
            std::clog << "Engine2: C-Octo" << std::endl;
            do {
                if ((i & 7) == 0) {
                    chip8->handleTimer();
                    if (octo.dt)
                        --octo.dt;
                    if (octo.st)
                        --octo.st;
                }
                chip8->executeInstruction();
                octo_emulator_instruction(&octo);
                if (!(i % 500000)) {
                    std::clog << i << ": " << chip8->dumpStateLine() << std::endl;
                    std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
                }
                if(!(i % 500000)) {
                    std::cout << chip8EmuScreen(host.emuCore());
                }
                ++i;
            } while ((i & 0xfff) || (chip8->dumpStateLine() == dumOctoStateLine(&octo) && chip8EmuScreen(host.emuCore()) == octoScreen(octo)));
            std::clog << i << ": " << chip8->dumpStateLine() << std::endl;
            std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
            std::cerr << chip8EmuScreen(host.emuCore());
            std::cerr << "---" << std::endl;
            std::cerr << octoScreen(octo) << std::endl;
        }
        else if(benchmark > 0) {
            uint64_t instructions = benchmark;
            int ipf = 42;
            if(coreProperties.contains("instructionsPerFrame")) {
                ipf = coreProperties.at("instructionsPerFrame").getInt();
            }
            std::cout << "Executing benchmark (" << /*chip8->options.instructionsPerFrame*/ ipf << "ipf)..." << std::endl;
            auto startChip8 = std::chrono::steady_clock::now();
            auto ticks = uint64_t(instructions / ipf);
            for(i = 0; i < ticks; ++i) {
                host.emuCore().executeFrame();
            }
            int64_t lastCycles = -1;
            int64_t cycles = 0;
            while((cycles = chip8->cycles()) < instructions && cycles != lastCycles) {
                chip8->executeInstruction();
                lastCycles = cycles;
            }
            auto durationChip8 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startChip8);
            if(screenDump) {
                std::cout << chip8EmuScreenANSI(host.emuCore());
            }
            std::cout << "Executed instructions: " << chip8->cycles() << std::endl;
            std::cout << "Executed frames: " << chip8->frames() << std::endl;
            std::cout << "Cadmium: " << durationChip8.count() << "us, " << int(double(chip8->cycles())/durationChip8.count()) << "MIPS" << std::endl;
        }
        else if(traceLines >= 0) {
#if 0
            chip8.memory()[0x1ff] = testSuiteMenuVal & 0xff;
            size_t waits = 0;
            do {
                bool isDraw = (chip8.opcode() & 0xF000) == 0xD000;
                bool isWait = !(!isDraw || options.optInstantDxyn || (chip8.getCycles() % options.instructionsPerFrame == 0));
                if ((chip8.getCycles() % options.instructionsPerFrame) == 0) {
                    std::cout << "--- handle timer ---" << std::endl;
                    chip8.handleTimer();
                }
                std::cout << (i - waits) << "/" << chip8.getCycles() << ": " << chip8.dumpStateLine() << (isWait ? " (WAIT)" : "") << std::endl;
                if(isWait) ++waits;
                uint16_t opcode = chip8.opcode();
                chip8.executeInstruction();
                if(chip8.needsScreenUpdate()) {
                    std::cout << chip8EmuScreen(chip8);
                }
                else if((opcode & 0xF0FF) == 0xF00A)
                    break;
                ++i;
            } while (i <= traceLines && chip8.getExecMode() == emu::IChip8Emulator::ExecMode::eRUNNING);
            if(screenDump) {
                std::cout << chip8EmuScreenANSI(chip8);
            }
#endif
        }
    }
#endif
    return 0;
}
