//
// Created by Steffen Schümann on 04.10.24.
//

#include <doctest/doctest.h>

#include "chip8testhelper.hpp"

TEST_SUITE_BEGIN("CHIP-8-Test-Suite");

static std::string logoScreen = R"(................................................................
............#####.#....................#..........##............
..............#.....##.#...##..###...###.#..#..##..#............
..............#...#.#.#.#.#..#.#..#.#..#.#..#.#.................
..............#...#.#...#.####.#..#.#..#.#..#..#................
..............#...#.#...#.#....#..#.#..#.#..#...#...............
..............#...#.#...#..###.#..#..###..###.##................
................................................................
................................................................
...........#####...##.......##..#####...........#######.........
..........#######.###......###.#######.........###...###........
.........###...##.###......###.###..###.......###.....##........
........###.......###..........###...##.......###.....##........
........###..#.#..###.......##.###...##.......###.....##........
........###.......######...###.###...##........###...##.........
........###.#...#.#######..###.###...##.####....######..........
........###..###..###..###.###.###..###.####...###..###.........
........###.......###...##.###.#######........###....###........
........###.......###...##.###.######........###......##........
........###.......###...##.###.###...........###......##........
........###.......###...##.###.###.#.#...###.###......##........
.........###...##.###...##.###.###.###.....#.####....###........
..........#######.###...##.###.###...#...##...#########.........
...........#####..###...##.###.###...#.#.###...#######..........
................................................................
................................................................
.............###..##...##.#.......##......#.#....##.............
..............#..#..#.#...###....#...#..#...###.#..#............
..............#..####..#..#.......#..#..#.#.#...####............
..............#..#......#.#........#.#..#.#.#...#...............
..............#...###.##...##....##...###.#..##..###............
................................................................
)";

static void runTestForScreen(const std::string& test, const std::string& reference, std::string preset)
{
    auto [host, core, start] = createChip8Instance(preset);
    auto rom = loadFile(fs::path(CHIP8_TEST_SUITE) / "bin" / test);
    core->reset();
    host->load(rom);
    while (core->execMode() != emu::GenericCpu::ePAUSED) {
        host->executeFrame();
    }
    auto screen = host->chip8EmuScreen();
    CHECK(reference == screen);
}

TEST_CASE("chip-8: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "chip-8");
}

TEST_CASE("chip-8e: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "chip-8e");
}

TEST_CASE("chip-48: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "chip-48");
}

TEST_CASE("schip-1-0: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "schip-1-0");
}

TEST_CASE("schip-1-1: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "schip-1-1");
}

TEST_CASE("schipc: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "schipc");
}

TEST_CASE("schip-modern: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "schip-modern");
}

TEST_CASE("megachip: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "megachip");
}

TEST_CASE("xo-chip: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "xo-chip");
}

TEST_CASE("vip-chip-8: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "vip-chip-8");
}

TEST_CASE("vip-chip-8e: 1-chip8-logo.ch8")
{
    runTestForScreen("1-chip8-logo.ch8", logoScreen, "vip-chip-8e");
}

static std::string ibmScreen = R"(................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
............########.#########...#####.........#####..#.#.......
......................................................#.#.......
............########.###########.######.......######...#........
................................................................
..............####.....###...###...#####.....#####....#.#.......
......................................................###.......
..............####.....#######.....#######.#######......#.......
........................................................#.......
..............####.....#######.....###.#######.###..............
.......................................................#........
..............####.....###...###...###..#####..###..............
......................................................###.......
............########.###########.#####...###...#####....#.......
......................................................##........
............########.#########...#####....#....#####..###.......
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
................................................................
)";


TEST_CASE("chip-8: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "chip-8");
}

TEST_CASE("chip-8e: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "chip-8e");
}

TEST_CASE("chip-48: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "chip-48");
}

TEST_CASE("schip-1-0: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "schip-1-0");
}

TEST_CASE("schip-1-1: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "schip-1-1");
}

TEST_CASE("schipc: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "schipc");
}

TEST_CASE("schip-modern: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "schip-modern");
}

TEST_CASE("megachip: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "megachip");
}

TEST_CASE("xo-chip: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "xo-chip");
}

TEST_CASE("vip-chip-8: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "vip-chip-8");
}

TEST_CASE("vip-chip-8e: 2-ibm-logo.ch8")
{
    runTestForScreen("2-ibm-logo.ch8", ibmScreen, "vip-chip-8e");
}

static std::string coraxScreen = R"(................................................................
..###.#.#.........###.#.#.........###.#.#.........###.###.......
...##..#...#.#......#..#...#.#....###.###..#.#....#...##...#.#..
....#.#.#..##.....##..#.#..##.....#.#...#..##.....##....#..##...
..###.#.#..#......###.#.#..#......###...#..#......#...##...#....
................................................................
..#.#.#.#.........###.###.........###.###.........###.###.......
..###..#...#.#....#.#.##...#.#....###.##...#.#....#....##..#.#..
....#.#.#..##.....#.#.#....##.....#.#...#..##.....##....#..##...
....#.#.#..#......###.###..#......###.##...#......#...###..#....
................................................................
..###.#.#.........###.###.........###.###.........###.###.......
..##...#...#.#....###.#.#..#.#....###...#..#.#....#...##...#.#..
....#.#.#..##.....#.#.#.#..##.....#.#..#...##.....##..#....##...
..##..#.#..#......###.###..#......###..#...#......#...###..#....
................................................................
..###.#.#.........###.##..........###..##.............#.#.......
....#..#...#.#....###..#...#.#....###.#....#.#....#.#..#...#.#..
...#..#.#..##.....#.#..#...##.....#.#.###..##.....#.#.#.#..##...
...#..#.#..#......###.###..#......###.###..#.......#..#.#..#....
................................................................
..###.#.#.........###.###.........###.###.......................
..###..#...#.#....###...#..#.#....###.##...#.#..................
....#.#.#..##.....#.#.##...##.....#.#.#....##...................
..##..#.#..#......###.###..#......###.###..#....................
................................................................
..##..#.#.........###.###.........###..##.............#.#...###.
...#...#...#.#....###..##..#.#....#...#....#.#....#.#.###.....#.
...#..#.#..##.....#.#...#..##.....##..###..##.....#.#...#...##..
..###.#.#..#......###.###..#......#...###..#.......#....#.#.###.
................................................................
................................................................
)";

TEST_CASE("chip-8: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "chip-8");
}

TEST_CASE("chip-8e: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "chip-8e");
}

TEST_CASE("chip-48: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "chip-48");
}

TEST_CASE("schip-1-0: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "schip-1-0");
}

TEST_CASE("schip-1-1: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "schip-1-1");
}

TEST_CASE("schipc: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "schipc");
}

TEST_CASE("schip-modern: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "schip-modern");
}

TEST_CASE("megachip: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "megachip");
}

TEST_CASE("xo-chip: 3-corax+.ch8")
{
    runTestForScreen("3-corax+.ch8", coraxScreen, "xo-chip");
}



TEST_SUITE_END();