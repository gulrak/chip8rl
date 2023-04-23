#include <emulation/chip8vip.hpp>
#include <emulation/logger.hpp>
#include <emulation/hardware/cdp186x.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <fstream>

#define VIDEO_FIRST_VISIBLE_LINE 80
#define VIDEO_FIRST_INVISIBLE_LINE  208

namespace emu {

class Chip8VIP::Private {
public:
    static constexpr uint16_t FETCH_LOOP_ENTRY = 0x01B;
    explicit Private(Chip8EmulatorHost& host, Cdp1802Bus& bus, const Chip8EmulatorOptions& options) : _host(host), _cpu(bus), _video(Cdp186x::eCDP1861, _cpu, options) {}
    Chip8EmulatorHost& _host;
    Cdp1802 _cpu;
    Cdp186x _video;
    int64_t _irqStart{0};
    int64_t _nextFrame{0};
    uint8_t _keyLatch{0};
    std::atomic<float> _wavePhase{0};
    std::array<uint8_t,MAX_MEMORY_SIZE> _ram{};
    std::array<uint8_t,512> _rom{};
    std::array<uint8_t,256*192> _screenBuffer;
};


struct Patch {
    uint16_t offset;
    std::vector<uint8_t> bytes;
};

struct PatchSet {
    std::vector<Patch> patches;
};

static const uint8_t _chip8_cvip[0x200] = {
    0x91, 0xbb, 0xff, 0x01, 0xb2, 0xb6, 0xf8, 0xcf, 0xa2, 0xf8, 0x81, 0xb1, 0xf8, 0x46, 0xa1, 0x90, 0xb4, 0xf8, 0x1b, 0xa4, 0xf8, 0x01, 0xb5, 0xf8, 0xfc, 0xa5, 0xd4, 0x96, 0xb7, 0xe2, 0x94, 0xbc, 0x45, 0xaf, 0xf6, 0xf6, 0xf6, 0xf6, 0x32, 0x44,
    0xf9, 0x50, 0xac, 0x8f, 0xfa, 0x0f, 0xf9, 0xf0, 0xa6, 0x05, 0xf6, 0xf6, 0xf6, 0xf6, 0xf9, 0xf0, 0xa7, 0x4c, 0xb3, 0x8c, 0xfc, 0x0f, 0xac, 0x0c, 0xa3, 0xd3, 0x30, 0x1b, 0x8f, 0xfa, 0x0f, 0xb3, 0x45, 0x30, 0x40, 0x22, 0x69, 0x12, 0xd4, 0x00,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x7c, 0x75, 0x83, 0x8b, 0x95, 0xb4, 0xb7, 0xbc, 0x91, 0xeb, 0xa4, 0xd9, 0x70, 0x99, 0x05, 0x06, 0xfa, 0x07, 0xbe, 0x06, 0xfa, 0x3f, 0xf6,
    0xf6, 0xf6, 0x22, 0x52, 0x07, 0xfa, 0x1f, 0xfe, 0xfe, 0xfe, 0xf1, 0xac, 0x9b, 0xbc, 0x45, 0xfa, 0x0f, 0xad, 0xa7, 0xf8, 0xd0, 0xa6, 0x93, 0xaf, 0x87, 0x32, 0xf3, 0x27, 0x4a, 0xbd, 0x9e, 0xae, 0x8e, 0x32, 0xa4, 0x9d, 0xf6, 0xbd, 0x8f, 0x76,
    0xaf, 0x2e, 0x30, 0x98, 0x9d, 0x56, 0x16, 0x8f, 0x56, 0x16, 0x30, 0x8e, 0x00, 0xec, 0xf8, 0xd0, 0xa6, 0x93, 0xa7, 0x8d, 0x32, 0xd9, 0x06, 0xf2, 0x2d, 0x32, 0xbe, 0xf8, 0x01, 0xa7, 0x46, 0xf3, 0x5c, 0x02, 0xfb, 0x07, 0x32, 0xd2, 0x1c, 0x06,
    0xf2, 0x32, 0xce, 0xf8, 0x01, 0xa7, 0x06, 0xf3, 0x5c, 0x2c, 0x16, 0x8c, 0xfc, 0x08, 0xac, 0x3b, 0xb3, 0xf8, 0xff, 0xa6, 0x87, 0x56, 0x12, 0xd4, 0x9b, 0xbf, 0xf8, 0xff, 0xaf, 0x93, 0x5f, 0x8f, 0x32, 0xdf, 0x2f, 0x30, 0xe5, 0x00, 0x42, 0xb5,
    0x42, 0xa5, 0xd4, 0x8d, 0xa7, 0x87, 0x32, 0xac, 0x2a, 0x27, 0x30, 0xf5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0xa3, 0x98, 0x56, 0xd4, 0xf8, 0x81, 0xbc, 0xf8, 0x95, 0xac, 0x22, 0xdc, 0x12, 0x56, 0xd4, 0x06, 0xb8, 0xd4,
    0x06, 0xa8, 0xd4, 0x64, 0x0a, 0x01, 0xe6, 0x8a, 0xf4, 0xaa, 0x3b, 0x28, 0x9a, 0xfc, 0x01, 0xba, 0xd4, 0xf8, 0x81, 0xba, 0x06, 0xfa, 0x0f, 0xaa, 0x0a, 0xaa, 0xd4, 0xe6, 0x06, 0xbf, 0x93, 0xbe, 0xf8, 0x1b, 0xae, 0x2a, 0x1a, 0xf8, 0x00, 0x5a,
    0x0e, 0xf5, 0x3b, 0x4b, 0x56, 0x0a, 0xfc, 0x01, 0x5a, 0x30, 0x40, 0x4e, 0xf6, 0x3b, 0x3c, 0x9f, 0x56, 0x2a, 0x2a, 0xd4, 0x00, 0x22, 0x86, 0x52, 0xf8, 0xf0, 0xa7, 0x07, 0x5a, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x5b, 0x12, 0xd4, 0x22, 0x86, 0x52,
    0xf8, 0xf0, 0xa7, 0x0a, 0x57, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x6b, 0x12, 0xd4, 0x15, 0x85, 0x22, 0x73, 0x95, 0x52, 0x25, 0x45, 0xa5, 0x86, 0xfa, 0x0f, 0xb5, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x82, 0x15, 0x15, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x88,
    0xd4, 0x45, 0x07, 0x30, 0x8c, 0x45, 0x07, 0x30, 0x84, 0xe6, 0x62, 0x26, 0x45, 0xa3, 0x36, 0x88, 0xd4, 0x3e, 0x88, 0xd4, 0xf8, 0xf0, 0xa7, 0xe7, 0x45, 0xf4, 0xa5, 0x86, 0xfa, 0x0f, 0x3b, 0xb2, 0xfc, 0x01, 0xb5, 0xd4, 0x45, 0x56, 0xd4, 0x45,
    0xe6, 0xf4, 0x56, 0xd4, 0x45, 0xfa, 0x0f, 0x3a, 0xc4, 0x07, 0x56, 0xd4, 0xaf, 0x22, 0xf8, 0xd3, 0x73, 0x8f, 0xf9, 0xf0, 0x52, 0xe6, 0x07, 0xd2, 0x56, 0xf8, 0xff, 0xa6, 0xf8, 0x00, 0x7e, 0x56, 0xd4, 0x19, 0x89, 0xae, 0x93, 0xbe, 0x99, 0xee,
    0xf4, 0x56, 0x76, 0xe6, 0xf4, 0xb9, 0x56, 0x45, 0xf2, 0x56, 0xd4, 0x45, 0xaa, 0x86, 0xfa, 0x0f, 0xba, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x4b
};

static const uint8_t _chip8tdp_cvip[] = {
    0x91, 0xbb, 0xff, 0x02, 0xb2, 0xb6, 0xf8, 0xcf, 0xa2, 0xf8, 0x02, 0xb1, 0xf8, 0x06, 0xa1, 0x90, // 0x0000
    0xb4, 0xf8, 0x1b, 0xa4, 0xf8, 0x01, 0xb5, 0xf8, 0xfa, 0xa5, 0xd4, 0x96, 0xb7, 0xe2, 0x94, 0xbc, // 0x0010
    0x45, 0xaf, 0xf6, 0xf6, 0xf6, 0xf6, 0x32, 0x44, 0xf9, 0x50, 0xac, 0x8f, 0xfa, 0x0f, 0xf9, 0xf0, // 0x0020
    0xa6, 0x05, 0xf6, 0xf6, 0xf6, 0xf6, 0xf9, 0xf0, 0xa7, 0x4c, 0xb3, 0x8c, 0xfc, 0x0f, 0xac, 0x0c, // 0x0030
    0xa3, 0xd3, 0x30, 0x1b, 0x8f, 0xfa, 0x0f, 0xb3, 0x45, 0x30, 0x40, 0x22, 0x69, 0x12, 0xd4, 0x00, // 0x0040
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, // 0x0050
    0x00, 0x7c, 0x75, 0x83, 0x8b, 0x95, 0xb4, 0xb7, 0xbc, 0x91, 0xeb, 0xa4, 0xd9, 0x70, 0x99, 0x05, // 0x0060
    0x06, 0xfa, 0x07, 0xbe, 0x06, 0xfa, 0x3f, 0xf6, 0xf6, 0xf6, 0x22, 0x52, 0x07, 0xfa, 0x3f, 0xfe, // 0x0070
    0xfe, 0xfe, 0xf1, 0xac, 0x30, 0xe0, 0x45, 0xfa, 0x0f, 0xad, 0xa7, 0xf8, 0xd0, 0xa6, 0x93, 0xaf, // 0x0080
    0x87, 0x32, 0xf3, 0x27, 0x4a, 0xbd, 0x9e, 0xae, 0x8e, 0x32, 0xa4, 0x9d, 0xf6, 0xbd, 0x8f, 0x76, // 0x0090
    0xaf, 0x2e, 0x30, 0x98, 0x9d, 0x56, 0x16, 0x8f, 0x56, 0x16, 0x30, 0x8e, 0x00, 0xec, 0xf8, 0xd0, // 0x00A0
    0xa6, 0x93, 0xa7, 0x8d, 0x32, 0xd9, 0x06, 0xf2, 0x2d, 0x32, 0xbe, 0xf8, 0x01, 0xa7, 0x46, 0xf3, // 0x00B0
    0x5c, 0x02, 0xfb, 0x07, 0x32, 0xd2, 0x1c, 0x06, 0xf2, 0x32, 0xce, 0xf8, 0x01, 0xa7, 0x06, 0xf3, // 0x00C0
    0x5c, 0x2c, 0x16, 0x8c, 0xfc, 0x08, 0xac, 0x30, 0xe6, 0xf8, 0xff, 0xa6, 0x87, 0x56, 0x12, 0xd4, // 0x00D0
    0x9b, 0x7c, 0x00, 0xbc, 0x30, 0x86, 0x9c, 0x7c, 0x00, 0xbc, 0xfb, 0x10, 0x30, 0xfc, 0x42, 0xb5, // 0x00E0
    0x42, 0xa5, 0xd4, 0x8d, 0xa7, 0x87, 0x32, 0xac, 0x2a, 0x27, 0x30, 0xf5, 0x3a, 0xb3, 0x30, 0xd9, // 0x00F0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0xa3, 0x98, 0x56, 0xd4, 0xf8, 0x81, 0xbc, 0xf8, 0x95, 0xac, // 0x0100
    0x22, 0xdc, 0x12, 0x56, 0xd4, 0x06, 0xb8, 0xd4, 0x06, 0xa8, 0xd4, 0x64, 0x0a, 0x01, 0xe6, 0x8a, // 0x0110
    0xf4, 0xaa, 0x3b, 0x28, 0x9a, 0xfc, 0x01, 0xba, 0xd4, 0xf8, 0x81, 0xba, 0x06, 0xfa, 0x0f, 0xaa, // 0x0120
    0x0a, 0xaa, 0xd4, 0xe6, 0x06, 0xbf, 0x93, 0xbe, 0xf8, 0x1b, 0xae, 0x2a, 0x1a, 0xf8, 0x00, 0x5a, // 0x0130
    0x0e, 0xf5, 0x3b, 0x4b, 0x56, 0x0a, 0xfc, 0x01, 0x5a, 0x30, 0x40, 0x4e, 0xf6, 0x3b, 0x3c, 0x9f, // 0x0140
    0x56, 0x2a, 0x2a, 0xd4, 0x00, 0x22, 0x86, 0x52, 0xf8, 0xf0, 0xa7, 0x07, 0x5a, 0x87, 0xf3, 0x17, // 0x0150
    0x1a, 0x3a, 0x5b, 0x12, 0xd4, 0x22, 0x86, 0x52, 0xf8, 0xf0, 0xa7, 0x0a, 0x57, 0x87, 0xf3, 0x17, // 0x0160
    0x1a, 0x3a, 0x6b, 0x12, 0xd4, 0x15, 0x85, 0x22, 0x73, 0x95, 0x52, 0x25, 0x45, 0xa5, 0x86, 0xfa, // 0x0170
    0x0f, 0xb5, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x82, 0x15, 0x15, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x88, // 0x0180
    0xd4, 0x45, 0x07, 0x30, 0x8c, 0x45, 0x07, 0x30, 0x84, 0xe6, 0x62, 0x26, 0x45, 0xa3, 0x36, 0x88, // 0x0190
    0xd4, 0x3e, 0x88, 0xd4, 0xf8, 0xf0, 0xa7, 0xe7, 0x45, 0xf4, 0xa5, 0x86, 0xfa, 0x0f, 0x3b, 0xb2, // 0x01A0
    0xfc, 0x01, 0xb5, 0xd4, 0x45, 0x56, 0xd4, 0x45, 0xe6, 0xf4, 0x56, 0xd4, 0x45, 0xfa, 0x0f, 0x3a, // 0x01B0
    0xc4, 0x07, 0x56, 0xd4, 0xaf, 0x22, 0xf8, 0xd3, 0x73, 0x8f, 0xf9, 0xf0, 0x52, 0xe6, 0x07, 0xd2, // 0x01C0
    0x56, 0xf8, 0xff, 0xa6, 0xf8, 0x00, 0x7e, 0x56, 0xd4, 0x19, 0x89, 0xae, 0x93, 0xbe, 0x99, 0xee, // 0x01D0
    0xf4, 0x56, 0x76, 0xe6, 0xf4, 0xb9, 0x56, 0x45, 0xf2, 0x56, 0xd4, 0x45, 0xaa, 0x86, 0xfa, 0x0f, // 0x01E0
    0xba, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x45, 0x02, 0x30, 0x00, 0x4b, // 0x01F0

    0x12, 0x60, 0x01, 0x7a, 0x42, 0x70, 0x22, 0x78, 0x22, 0x52, 0xc4, 0x19, 0xf8, 0x00, 0xa0, 0x9b,
    0xfa, 0x0e, 0xb0, 0xe2, 0xe2, 0x80, 0xe2, 0x20, 0xa0, 0xe2, 0x3c, 0x15, 0x80, 0xe2, 0x20, 0xa0,
    0x34, 0x1c, 0x98, 0x32, 0x29, 0xab, 0x2b, 0x8b, 0xb8, 0x88, 0x32, 0x03, 0x7b, 0x28, 0x30, 0x04,
    0xf8, 0x02, 0xae, 0x9b, 0xbf, 0xf8, 0x00, 0xaf, 0xf8, 0x00, 0x5f, 0x1f, 0x8f, 0x3a, 0x38, 0x2e,
    0x8e, 0x3a, 0x38, 0xd4, 0x01, 0x9b, 0xff, 0x01, 0xbb, 0xd4 //, 0x9b, 0x7c, 0x00, 0xbc, 0x30, 0x86,
    //0x9c, 0x7c, 0x00, 0xbc, 0xfb, 0x10, 0x30, 0xfc, 0x3a, 0xb3, 0x30, 0xd9, 0x02, 0x45, 0x02, 0x30
};

static const uint8_t _chip8x_cvip[0x300] = {
    0x91, 0xbb, 0xff, 0x01, 0xb2, 0xb6, 0xf8, 0xcf, 0xa2, 0xf8, 0x81, 0xb1, 0xf8, 0x46, 0xa1, 0x90, 0xb4, 0xf8, 0x1b, 0xa4, 0xf8, 0x02, 0xb5, 0xf8, 0xfa, 0xa5, 0xd4, 0x96, 0xb7, 0xe2, 0x94, 0xbc, 0x45, 0xaf, 0xf6, 0xf6, 0xf6, 0xf6, 0x32, 0x44,
    0xf9, 0x50, 0xac, 0x8f, 0xfa, 0x0f, 0xf9, 0xf0, 0xa6, 0x05, 0xf6, 0xf6, 0xf6, 0xf6, 0xf9, 0xf0, 0xa7, 0x4c, 0xb3, 0x8c, 0xfc, 0x0f, 0xac, 0x0c, 0xa3, 0xd3, 0x30, 0x1b, 0x8f, 0xfa, 0x0f, 0xb3, 0x45, 0x30, 0x40, 0x22, 0x69, 0x12, 0xd4, 0x00,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x00, 0x01, 0x01, 0x00, 0x7c, 0x75, 0x83, 0x8b, 0xfe, 0xb4, 0xb7, 0xbc, 0x91, 0xeb, 0x00, 0xd9, 0x70, 0x99, 0x05, 0x06, 0xfa, 0x07, 0xbe, 0x06, 0xfa, 0x3f, 0xf6,
    0xf6, 0xf6, 0x22, 0x52, 0x07, 0xfa, 0x1f, 0xfe, 0xfe, 0xfe, 0xf1, 0xac, 0x9b, 0xbc, 0x45, 0xfa, 0x0f, 0xad, 0xa7, 0xf8, 0xd0, 0xa6, 0x93, 0xaf, 0x87, 0x32, 0xf3, 0x27, 0x4a, 0xbd, 0x9e, 0xae, 0x8e, 0x32, 0xa4, 0x9d, 0xf6, 0xbd, 0x8f, 0x76,
    0xaf, 0x2e, 0x30, 0x98, 0x9d, 0x56, 0x16, 0x8f, 0x56, 0x16, 0x30, 0x8e, 0x00, 0xec, 0xf8, 0xd0, 0xa6, 0x93, 0xa7, 0x8d, 0x32, 0xd9, 0x06, 0xf2, 0x2d, 0x32, 0xbe, 0xf8, 0x01, 0xa7, 0x46, 0xf3, 0x5c, 0x02, 0xfb, 0x07, 0x32, 0xd2, 0x1c, 0x06,
    0xf2, 0x32, 0xce, 0xf8, 0x01, 0xa7, 0x06, 0xf3, 0x5c, 0x2c, 0x16, 0x8c, 0xfc, 0x08, 0xac, 0x3b, 0xb3, 0xf8, 0xff, 0xa6, 0x87, 0x56, 0x12, 0xd4, 0x9b, 0xbf, 0xf8, 0xff, 0xaf, 0x93, 0x5f, 0x8f, 0x32, 0xdf, 0x2f, 0x30, 0xe5, 0x00, 0x42, 0xb5,
    0x42, 0xa5, 0xd4, 0x8d, 0xa7, 0x87, 0x32, 0xac, 0x2a, 0x27, 0x30, 0xf5, 0x00, 0x00, 0x05, 0xf6, 0x33, 0xa4, 0x30, 0x95, 0x00, 0x45, 0xa3, 0x98, 0x56, 0xd4, 0xf8, 0x81, 0xbc, 0xf8, 0x95, 0xac, 0x22, 0xdc, 0x12, 0x56, 0xd4, 0x06, 0xb8, 0xd4,
    0x06, 0xa8, 0xd4, 0x64, 0x0a, 0x01, 0xe6, 0x8a, 0xf4, 0xaa, 0x3b, 0x28, 0x9a, 0xfc, 0x01, 0xba, 0xd4, 0xf8, 0x81, 0xba, 0x06, 0xfa, 0x0f, 0xaa, 0x0a, 0xaa, 0xd4, 0xe6, 0x06, 0xbf, 0x93, 0xbe, 0xf8, 0x1b, 0xae, 0x2a, 0x1a, 0xf8, 0x00, 0x5a,
    0x0e, 0xf5, 0x3b, 0x4b, 0x56, 0x0a, 0xfc, 0x01, 0x5a, 0x30, 0x40, 0x4e, 0xf6, 0x3b, 0x3c, 0x9f, 0x56, 0x2a, 0x2a, 0xd4, 0x00, 0x22, 0x86, 0x52, 0xf8, 0xf0, 0xa7, 0x07, 0x5a, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x5b, 0x12, 0xd4, 0x22, 0x86, 0x52,
    0xf8, 0xf0, 0xa7, 0x0a, 0x57, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x6b, 0x12, 0xd4, 0x15, 0x85, 0x22, 0x73, 0x95, 0x52, 0x25, 0x45, 0xa5, 0x86, 0xfa, 0x0f, 0xb5, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x82, 0x15, 0x15, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x88,
    0xd4, 0x45, 0x07, 0x30, 0x8c, 0x45, 0x07, 0x30, 0x84, 0xe6, 0x62, 0x26, 0x45, 0xa3, 0x36, 0x88, 0xd4, 0x3e, 0x88, 0xd4, 0xe6, 0x06, 0xfa, 0x77, 0x56, 0x07, 0xfa, 0x77, 0xf4, 0xfa, 0x77, 0x56, 0x15, 0xd4, 0x00, 0x00, 0x45, 0x56, 0xd4, 0x45,
    0xe6, 0xf4, 0x56, 0xd4, 0x45, 0xfa, 0x0f, 0x3a, 0xc4, 0x07, 0x56, 0xd4, 0xaf, 0x22, 0xf8, 0xd3, 0x73, 0x8f, 0xf9, 0xf0, 0x52, 0xe6, 0x07, 0xd2, 0x56, 0xf8, 0xff, 0xa6, 0xf8, 0x00, 0x7e, 0x56, 0xd4, 0x19, 0x89, 0xae, 0x93, 0xbe, 0x99, 0xee,
    0xf4, 0x56, 0x76, 0xe6, 0xf4, 0xb9, 0x56, 0x45, 0xf2, 0x56, 0xd4, 0x45, 0xaa, 0x86, 0xfa, 0x0f, 0xba, 0xd4, 0x37, 0x88, 0xd4, 0x3f, 0x88, 0xd4, 0xe6, 0x63, 0xd4, 0xe6, 0x3f, 0xfc, 0x6b, 0xd4, 0x92, 0xbd, 0xf8, 0x9f, 0xad, 0x0d, 0x32, 0x0a,
    0x45, 0xd4, 0x00, 0x45, 0xfa, 0x0f, 0xaf, 0x32, 0x31, 0x46, 0xfa, 0x3f, 0xf6, 0xf6, 0xf6, 0x22, 0x52, 0xe2, 0x06, 0xfa, 0x1f, 0xfe, 0xfe, 0xfe, 0xf1, 0xac, 0x12, 0xf8, 0xd0, 0xbc, 0x07, 0x5c, 0x9c, 0xfc, 0x08, 0xac, 0x2f, 0x8f, 0x3a, 0x26,
    0xd4, 0x07, 0xbd, 0x46, 0xac, 0x06, 0xbc, 0x8c, 0xfa, 0x07, 0x22, 0x52, 0xe2, 0xf8, 0xc0, 0xbe, 0x9c, 0xf6, 0xf6, 0xf6, 0xf6, 0xfa, 0x07, 0xad, 0x9c, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xf4, 0xae, 0x8c, 0xf6, 0xf6, 0xf6, 0xf6, 0xfa, 0x07, 0xaf,
    0x9e, 0xb7, 0x8e, 0xa7, 0x8d, 0xbc, 0xf8, 0x04, 0xac, 0x9d, 0x57, 0x87, 0xfc, 0x08, 0xa7, 0x2c, 0x8c, 0x3a, 0x61, 0x9c, 0x32, 0x73, 0xff, 0x01, 0xbc, 0x30, 0x5e, 0x1e, 0x8e, 0xfa, 0xe7, 0xae, 0x8f, 0x32, 0x7e, 0x2f, 0x30, 0x58, 0x12, 0xd4,
    0x92, 0xbd, 0xf8, 0x9f, 0xad, 0xf8, 0xc0, 0xbc, 0x94, 0xac, 0xf8, 0xaa, 0x5c, 0x94, 0xbc, 0xac, 0x0c, 0xfb, 0x91, 0x32, 0x9c, 0xf8, 0x91, 0x5c, 0xf8, 0x01, 0x5d, 0xd4, 0xf8, 0x00, 0x5d, 0xd4, 0xe2, 0x65, 0x22, 0xd4, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x80, 0x00, 0xe0, 0x00, 0x4b
};

static std::map<std::string,PatchSet> g_patchSets = {
    {
        "CHIP-8I",
        {{
            {0x1A4, {0x86, 0xFA, 0x01, 0x3A, 0xAC, 0xE5, 0x63, 0xD4, 0xE7, 0x45, 0xFA, 0x01, 0x3A, 0xF2, 0x63, 0xD4}},
            {0x1F2, {0x3F, 0xF2, 0x6B, 0x3F, 0xF5, 0xD4}}
        }}
    },
    {
        "CHIP-10", // 4k version, Ben H. Hutchinson, Jr.
        {{
            {0x000, {0x91, 0xFF, 0x03, 0xBB, 0xFF, 0x01, 0xB2, 0xB6, 0xF8, 0xCF, 0xA2, 0xF8, 0x73, 0xA1, 0x90, 0xB1}},
            {0x05D, {0x0B}},
            {0x06D, {0x00}},
            {0x070, {0x00, 0x42, 0x70, 0xC4, 0x22, 0x78, 0x22, 0x52, 0x19, 0xF8, 0x00, 0xA0, 0x9B, 0xB0, 0x98, 0x32, 0x85, 0xAB, 0x2B, 0x8B, 0xB8, 0x88, 0x32, 0x8C, 0x7B, 0x28, 0x30, 0x8D, 0x7A, 0x34, 0x71, 0x30, 0x8D}},
            {0x091, {0x42, 0x70, 0xC4, 0x22, 0x78, 0x22, 0x52, 0xF8, 0x00, 0xA0, 0x9B, 0xB0, 0xE2, 0xE2, 0x30, 0x91}},
            {0x0DE, {0x12, 0xD4, 0x9B, 0xBF, 0xFC, 0x04, 0x22, 0x52, 0x93, 0xAF, 0x93, 0x5F, 0x1F, 0x30, 0xF3, 0x00}},
            {0x0F3, {0x9F, 0xF3, 0x32, 0xDE, 0x30, 0xE8}},
            {0xB00, {0x06, 0xFA, 0x07, 0xBE, 0x06, 0xFA, 0x7F, 0xF6, 0xF6, 0xF6, 0x22, 0x52, 0x07, 0xFA, 0x3F, 0xFE}},
            {0xB10, {0xFE, 0xFE, 0xAC, 0x94, 0x7E, 0xBC, 0x8C, 0xFE, 0xF1, 0xAC, 0x9C, 0x7E, 0x52, 0x9B, 0xF4, 0xBC}},
            {0xB20, {0x45, 0xFA, 0x0F, 0xAD, 0xA7, 0xF8, 0xD0, 0xA6, 0x94, 0xAF, 0x87, 0x32, 0x85, 0x27, 0x4A, 0xBD}},
            {0xB30, {0x9E, 0xAE, 0x8E, 0x32, 0x3E, 0x9D, 0xF6, 0xBD, 0x8F, 0x76, 0xAF, 0x2E, 0x30, 0x32, 0x9D, 0x56}},
            {0xB40, {0x16, 0x8F, 0x56, 0x16, 0x30, 0x28, 0x00, 0xEC, 0xF8, 0xD0, 0xA6, 0x94, 0xA7, 0x8D, 0x32, 0x7E}},
            {0xB50, {0x06, 0xF2, 0x2D, 0x32, 0x58, 0xF8, 0x01, 0xA7, 0x46, 0xF3, 0x5C, 0x02, 0xFB, 0x0F, 0x32, 0x6C}},
            {0xB60, {0x1C, 0x06, 0xF2, 0x32, 0x68, 0xF8, 0x01, 0xA7, 0x06, 0xF3, 0x5C, 0x2C, 0x16, 0x8C, 0xFC, 0x10}},
            {0xB70, {0xAC, 0x9C, 0x7C, 0x00, 0xBC, 0xE2, 0x52, 0x9B, 0xFC, 0x04, 0xF3, 0xEC, 0x3A, 0x4D, 0xF8, 0xFF}},
            {0xB80, {0xA6, 0x87, 0x56, 0x12, 0xD4, 0x8D, 0xA7, 0x87, 0x32, 0x46, 0x2A, 0x27, 0x30, 0x87}}
        }}
    },
    {
        "CHIP-8-RB", // Relative Branching in CHIP-8, Wayne Smith
        {{
            {0x1A4, {0xE5, 0x86, 0xFC, 0x04, 0x85, 0x33, 0xFF, 0xF7, 0xA5, 0x33, 0xFA, 0x95, 0xFF, 0x01, 0x30}},
            {0x1F2, {0xF4, 0xA5, 0x3B, 0xFA, 0x95, 0xFC, 0x01, 0xB5, 0x25, 0xD4}}
        }}
    }
};


static const uint8_t _rom_cvip[0x200] = {
    0xf8, 0x80, 0xb2, 0xf8, 0x08, 0xa2, 0xe2, 0xd2, 0x64, 0x00, 0x62, 0x0c, 0xf8, 0xff, 0xa1, 0xf8, 0x0f, 0xb1, 0xf8, 0xaa, 0x51, 0x01, 0xfb, 0xaa, 0x32, 0x22, 0x91, 0xff, 0x04, 0x3b, 0x22, 0xb1, 0x30, 0x12, 0x36, 0x28, 0x90, 0xa0, 0xe0, 0xd0,
    0xe1, 0xf8, 0x00, 0x73, 0x81, 0xfb, 0xaf, 0x3a, 0x29, 0xf8, 0xd2, 0x73, 0xf8, 0x9f, 0x51, 0x81, 0xa0, 0x91, 0xb0, 0xf8, 0xcf, 0xa1, 0xd0, 0x73, 0x20, 0x20, 0x40, 0xff, 0x01, 0x20, 0x50, 0xfb, 0x82, 0x3a, 0x3e, 0x92, 0xb3, 0xf8, 0x51, 0xa3,
    0xd3, 0x90, 0xb2, 0xbb, 0xbd, 0xf8, 0x81, 0xb1, 0xb4, 0xb5, 0xb7, 0xba, 0xbc, 0xf8, 0x46, 0xa1, 0xf8, 0xaf, 0xa2, 0xf8, 0xdd, 0xa4, 0xf8, 0xc6, 0xa5, 0xf8, 0xba, 0xa7, 0xf8, 0xa1, 0xac, 0xe2, 0x69, 0xdc, 0xd7, 0xd7, 0xd7, 0xb6, 0xd7, 0xd7,
    0xd7, 0xa6, 0xd4, 0xdc, 0xbe, 0x32, 0xf4, 0xfb, 0x0a, 0x32, 0xef, 0xdc, 0xae, 0x22, 0x61, 0x9e, 0xfb, 0x0b, 0x32, 0xc2, 0x9e, 0xfb, 0x0f, 0x3a, 0x8f, 0xf8, 0x6f, 0xac, 0xf8, 0x40, 0xb9, 0x93, 0xf6, 0xdc, 0x29, 0x99, 0x3a, 0x97, 0xf8, 0x10,
    0xa7, 0xf8, 0x08, 0xa9, 0x46, 0xb7, 0x93, 0xfe, 0xdc, 0x86, 0x3a, 0xad, 0x2e, 0x97, 0xf6, 0xb7, 0xdc, 0x29, 0x89, 0x3a, 0xad, 0x17, 0x87, 0xf6, 0xdc, 0x8e, 0x3a, 0x9e, 0xdc, 0x69, 0x26, 0xd4, 0x30, 0xc0, 0xf8, 0x83, 0xac, 0xf8, 0x0a, 0xb9,
    0xdc, 0x33, 0xc5, 0x29, 0x99, 0x3a, 0xc8, 0xdc, 0x3b, 0xcf, 0xf8, 0x09, 0xa9, 0xa7, 0x97, 0x76, 0xb7, 0x29, 0xdc, 0x89, 0x3a, 0xd6, 0x87, 0xf6, 0x33, 0xe3, 0x7b, 0x97, 0x56, 0x16, 0x86, 0x3a, 0xcf, 0x2e, 0x8e, 0x3a, 0xcf, 0x30, 0xbd, 0xdc,
    0x16, 0xd4, 0x30, 0xef, 0xd7, 0xd7, 0xd7, 0x56, 0xd4, 0x16, 0x30, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x30, 0x39, 0x22, 0x2a, 0x3e, 0x20, 0x24, 0x34, 0x26, 0x28, 0x2e, 0x18, 0x14, 0x1c, 0x10, 0x12, 0xf0, 0x80, 0xf0, 0x80, 0xf0, 0x80, 0x80, 0x80,
    0xf0, 0x50, 0x70, 0x50, 0xf0, 0x50, 0x50, 0x50, 0xf0, 0x80, 0xf0, 0x10, 0xf0, 0x80, 0xf0, 0x90, 0xf0, 0x90, 0xf0, 0x10, 0xf0, 0x10, 0xf0, 0x90, 0xf0, 0x90, 0x90, 0x90, 0xf0, 0x10, 0x10, 0x10, 0x10, 0x60, 0x20, 0x20, 0x20, 0x70, 0xa0, 0xa0,
    0xf0, 0x20, 0x20, 0x7a, 0x42, 0x70, 0x22, 0x78, 0x22, 0x52, 0xc4, 0x19, 0xf8, 0x00, 0xa0, 0x9b, 0xb0, 0xe2, 0xe2, 0x80, 0xe2, 0xe2, 0x20, 0xa0, 0xe2, 0x20, 0xa0, 0xe2, 0x20, 0xa0, 0x3c, 0x53, 0x98, 0x32, 0x67, 0xab, 0x2b, 0x8b, 0xb8, 0x88,
    0x32, 0x43, 0x7b, 0x28, 0x30, 0x44, 0xd3, 0xf8, 0x0a, 0x3b, 0x76, 0xf8, 0x20, 0x17, 0x7b, 0xbf, 0xff, 0x01, 0x3a, 0x78, 0x39, 0x6e, 0x7a, 0x9f, 0x30, 0x78, 0xd3, 0xf8, 0x10, 0x3d, 0x85, 0x3d, 0x8f, 0xff, 0x01, 0x3a, 0x87, 0x17, 0x9c, 0xfe,
    0x35, 0x90, 0x30, 0x82, 0xd3, 0xe2, 0x9c, 0xaf, 0x2f, 0x22, 0x8f, 0x52, 0x62, 0xe2, 0xe2, 0x3e, 0x98, 0xf8, 0x04, 0xa8, 0x88, 0x3a, 0xa4, 0xf8, 0x04, 0xa8, 0x36, 0xa7, 0x88, 0x31, 0xaa, 0x8f, 0xfa, 0x0f, 0x52, 0x30, 0x94, 0x00, 0x00, 0x00,
    0x00, 0xd3, 0xdc, 0xfe, 0xfe, 0xfe, 0xfe, 0xae, 0xdc, 0x8e, 0xf1, 0x30, 0xb9, 0xd4, 0xaa, 0x0a, 0xaa, 0xf8, 0x05, 0xaf, 0x4a, 0x5d, 0x8d, 0xfc, 0x08, 0xad, 0x2f, 0x8f, 0x3a, 0xcc, 0x8d, 0xfc, 0xd9, 0xad, 0x30, 0xc5, 0xd3, 0x22, 0x06, 0x73,
    0x86, 0x73, 0x96, 0x52, 0xf8, 0x06, 0xae, 0xf8, 0xd8, 0xad, 0x02, 0xf6, 0xf6, 0xf6, 0xf6, 0xd5, 0x42, 0xfa, 0x0f, 0xd5, 0x8e, 0xf6, 0xae, 0x32, 0xdc, 0x3b, 0xea, 0x1d, 0x1d, 0x30, 0xea, 0x01
};



Chip8VIP::Chip8VIP(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other)
    : Chip8RealCoreBase(host, options)
    , _impl(new Private(host, *this, options))
{
    std::memcpy(_impl->_rom.data(), _rom_cvip, sizeof(_rom_cvip));
    _impl->_cpu.setInputHandler([this](uint8_t port) {
        if(port == 1)
            _impl->_video.enableDisplay();
        return 0;
    });
    _impl->_cpu.setOutputHandler([this](uint8_t port, uint8_t val) {
        switch (port) {
        case 1:
            _impl->_video.disableDisplay();
            break;
        case 2:
            _impl->_keyLatch = val & 0xf;
            break;
        default:
            break;
        }
    });
    _impl->_cpu.setNEFInputHandler([this](uint8_t idx) {
       switch(idx) {
           case 0: { // EF1 is set from four machine cycles before the video line to four before the end
               return _impl->_video.getNEFX();
           }
           case 2: {
               return _impl->_host.isKeyDown(_impl->_keyLatch);
           }
           default:
               return true;
       }
    });
    Chip8VIP::reset();
    if(other && false) {
        std::memcpy(_impl->_ram.data() + 0x200, other->memory() + 0x200, std::min(_impl->_ram.size() - 0x200 - 0x170, (size_t)other->memSize()));
        for(size_t i = 0; i < 16; ++i) {
            _state.v[i] = other->getV(i);
        }
        _state.i = other->getI();
        _state.pc = other->getPC();
        _state.sp = other->getSP();
        _state.dt = other->delayTimer();
        _state.st = other->soundTimer();
        std::memcpy(_state.s.data(), other->getStackElements(), stackSize() * sizeof(uint16_t));
        forceState();
    }
    {
        static bool first = true;
        if(first) {
            std::ofstream os("chip8tpd.bin", std::ios::binary);
            os.write((char*)_chip8tdp_cvip, sizeof(_chip8tdp_cvip));
        }
    }
}

Chip8VIP::~Chip8VIP()
{

}

void Chip8VIP::reset()
{
    if(_options.optTraceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.getCycles(), {_frames, frameCycle()}, fmt::format("--- RESET ---", _impl->_cpu.getCycles(), frameCycle()).c_str());
    std::memset(_impl->_ram.data(), 0, MAX_MEMORY_SIZE);
    if(_options.advanced && _options.advanced->contains("interpreter") && _options.advanced->at("interpreter") == "chip8tdp")
        std::memcpy(_impl->_ram.data(), _chip8tdp_cvip, sizeof(_chip8tdp_cvip));
    else
        std::memcpy(_impl->_ram.data(), _chip8_cvip, sizeof(_chip8_cvip));
    std::memset(_impl->_screenBuffer.data(), 0, 256*192);
    _impl->_video.reset();
    _impl->_cpu.reset();
    _cycles = 0;
    _frames = 0;
    _impl->_nextFrame = 0;
    _cpuState = eNORMAL;
    setExecMode(eRUNNING);
    while(!executeCdp1802() || getPC() != _options.startAddress); // fast-forward to fetch/decode loop
    setExecMode(_impl->_host.isHeadless() ? eRUNNING : ePAUSED);
    if(_options.optTraceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.getCycles(), {_frames, frameCycle()}, fmt::format("End of reset: {}/{}", _impl->_cpu.getCycles(), frameCycle()).c_str());
}

std::string Chip8VIP::name() const
{
    return "Chip-8-RVIP";
}

void Chip8VIP::fetchState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    auto base = _impl->_cpu.getR(2) & 0xFF00;
    std::memcpy(_state.v.data(), &_impl->_ram[base + 0xF0], 16);
    _state.i = _impl->_cpu.getR(0xA);
    _state.pc = _impl->_cpu.getR(5);
    _state.sp = (0xECF - _impl->_cpu.getR(2)) >> 1;
    _state.dt = _impl->_cpu.getR(8) >> 8;
    _state.st = _impl->_cpu.getR(8) & 0xff;
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _state.s[i] = (_impl->_ram[base + 0xCD - i*2] << 8) | _impl->_ram[base + 0xCF - i*2 - 1];
    }
}

void Chip8VIP::forceState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    auto base = _impl->_cpu.getR(2) & 0xFF00;
    std::memcpy(&_impl->_ram[base + 0xF0], _state.v.data(), 16);
    _impl->_cpu.setR(0xA, (uint16_t)_state.i);
    _impl->_cpu.setR(0x5, (uint16_t)_state.pc);
    _impl->_cpu.setR(0x8, (uint16_t)(_state.dt << 8 | _state.st));
    _impl->_cpu.setR(0x2, (uint16_t)(base + 0xCF - _state.sp * 2));
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _impl->_ram[base + 0xCD - i*2] = _state.s[i] >> 8;
        _impl->_ram[base + 0xCD - i*2 + 1] = _state.s[i] & 0xFF;
    }
}

bool Chip8VIP::executeCdp1802()
{
    static int lastFC = 0;
    static int endlessLoops = 0;
    auto fc = _impl->_video.executeStep();
    if(_options.optTraceLog  && _impl->_cpu.getCpuState() != Cdp1802::eIDLE)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.getCycles(), {_frames, fc}, fmt::format("{:24} ; {}", _impl->_cpu.disassembleInstructionWithBytes(-1, nullptr), _impl->_cpu.dumpStateLine()).c_str());
    if(_impl->_cpu.PC() == Private::FETCH_LOOP_ENTRY) {
        if(_options.optTraceLog)
            Logger::log(Logger::eCHIP8, _cycles, {_frames, fc}, fmt::format("CHIP8: {:30} ; {}", disassembleInstructionWithBytes(-1, nullptr), dumpStateLine()).c_str());
    }
    _impl->_cpu.executeInstruction();
    if(_impl->_cpu.PC() == Private::FETCH_LOOP_ENTRY) {
        fetchState();
        _cycles++;
        if(_impl->_cpu.getExecMode() == ePAUSED) {
            setExecMode(ePAUSED);
            _backendStopped = true;
        }
        else if (_execMode == eSTEP || (_execMode == eSTEPOVER && getSP() <= _stepOverSP)) {
            setExecMode(ePAUSED);
        }
        auto nextOp = opcode();
        bool newFrame = lastFC > fc;
        lastFC = fc;
        if(newFrame) {
            if ((nextOp & 0xF000) == 0x1000 && (opcode() & 0xFFF) == getPC()) {
                if (++endlessLoops > 2) {
                    setExecMode(ePAUSED);
                    endlessLoops = 0;
                }
            }
            else {
                endlessLoops = 0;
            }
        }
        if(hasBreakPoint(getPC())) {
            if(Chip8VIP::findBreakpoint(getPC()))
                setExecMode(ePAUSED);
        }
        return true;
    }
    else if(_impl->_cpu.getExecMode() == ePAUSED) {
        setExecMode(ePAUSED);
        _backendStopped = true;
    }
    return false;
}

void Chip8VIP::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return;
    }
    //std::clog << "CHIP8: " << dumpStateLine() << std::endl;
    auto start = _impl->_cpu.getCycles();
    while(!executeCdp1802() && _execMode != ePAUSED && _impl->_cpu.getCycles() - start < 3668*14);
}

void Chip8VIP::executeInstructions(int numInstructions)
{
    for(int i = 0; i < numInstructions; ++i) {
        executeInstruction();
    }
}

//---------------------------------------------------------------------------
// For easier handling we shift the line/cycle counting to the start of the
// interrupt (if display is enabled)

inline int Chip8VIP::frameCycle() const
{
    return Cdp186x::frameCycle(_impl->_cpu.getCycles()); // _impl->_irqStart ? ((_impl->_cpu.getCycles() >> 3) - _impl->_irqStart) : 0;
}

inline int Chip8VIP::videoLine() const
{
    return Cdp186x::videoLine(_impl->_cpu.getCycles()); // (frameCycle() + (78*14)) % 3668) / 14;
}

void Chip8VIP::tick(int)
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return;
    }

    auto nextFrame = Cdp186x::nextFrame(_impl->_cpu.getCycles());
    while(_execMode != ePAUSED && _impl->_cpu.getCycles() < nextFrame) {
        executeCdp1802();
    }
}

bool Chip8VIP::isDisplayEnabled() const
{
    return _impl->_video.isDisplayEnabled();
}

uint8_t* Chip8VIP::memory()
{
    return _impl->_ram.data();
}

int Chip8VIP::memSize() const
{
    return 4096;
}

int64_t Chip8VIP::frames() const
{
    return _impl->_video.frames();
}

float Chip8VIP::getAudioPhase() const
{
    return _impl->_wavePhase;
}

void Chip8VIP::setAudioPhase(float phase)
{
    _impl->_wavePhase = phase;
}

uint16_t Chip8VIP::getCurrentScreenWidth() const
{
    return 64;
}

uint16_t Chip8VIP::getCurrentScreenHeight() const
{
    return 128;
}

uint16_t Chip8VIP::getMaxScreenWidth() const
{
    return 64;
}

uint16_t Chip8VIP::getMaxScreenHeight() const
{
    return 128;
}

const uint8_t* Chip8VIP::getScreenBuffer() const
{
    return _impl->_video.getScreenBuffer();
}

GenericCpu& Chip8VIP::getBackendCpu()
{
    return _impl->_cpu;
}

uint8_t Chip8VIP::readByte(uint16_t addr) const
{
    if(addr < 0x1000)
        return _impl->_ram[addr];
    if(addr >= 0x8000 && addr < 0x8200)
        return _impl->_rom[addr & 0x1ff];
    _cpuState = eERROR;
    return 0;
}

uint8_t Chip8VIP::readByteDMA(uint16_t addr) const
{
    if(addr < 0x1000)
        return _impl->_ram[addr];
    if(addr >= 0x8000 && addr < 0x8200)
        return _impl->_rom[addr & 0x1ff];
    return 0;
}

uint8_t Chip8VIP::getMemoryByte(uint32_t addr) const
{
    return readByteDMA(addr);
}

void Chip8VIP::writeByte(uint16_t addr, uint8_t val)
{
    if(addr < 0x1000)
        _impl->_ram[addr] = val;
    else {
        setExecMode(ePAUSED);
        _cpuState = eERROR;
    }
}

}
