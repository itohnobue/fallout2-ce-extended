// PRODUCTION-LINKED regression pins for sfall_global_scripts.cc.
//
// Unlike most targets in this suite, this one links the module for real:
// sfall_global_scripts.cc is compiled into `test_sources`
// (tests/CMakeLists.txt:34), so every assertion below executes production
// code instead of a hand-copied mirror.
//
// The module's script registry is file driven: sfall_gl_scr_init() scans
// `scripts\gl*.int` (sfall_global_scripts.cc:128-142) and
// sfall_gl_scr_exec_start_proc() loads every discovered path
// (sfall_global_scripts.cc:339-378). To make that registry reachable without
// game data this file overrides five stubs that test_common_stubs.cc declares
// weak (the same pattern test_global_vars.cc uses for
// fileClose/fileRead/fileWrite): fileNameListInit, fileOpen,
// programCreateByPath, programFindProcedure and programExecuteProcedure.
//
// Pins:
//   - Frame-interval ("timer") registration: sfall_gl_scr_set_repeat /
//     sfall_gl_scr_set_type plus the `count >= repeat` gate in
//     sfall_gl_scr_process_simple (sfall_global_scripts.cc:426-447). This is
//     the per-script timer the sfall `set_global_script_repeat` /
//     `set_global_script_type` opcodes drive, and the same frame-interval
//     dispatch the W5 rest limb runs on while the pipboy rests
//     (e3fc7467: gPipboyRestDepth + pipboyIsResting() + the guard in
//     scripts.cc). A regression here — a timer that never fires, one that
//     fires every frame, or one whose counter never resets — changes what
//     that path executes and voices.
//   - F-M040: set_repeat resets the frame counter
//     (sfall_global_scripts.cc:490-494).
//   - F-058: game_loaded() tri-state 2/1/0 (sfall_global_scripts.cc:510-529).
//   - F-21: SCRIPT_PROC_NO_PROC == -1 (scripts.h:53), a compile-time pin.
//
// NOT pinned here — no linkable seam exists:
//   - scriptAddTimerEvent / the game-event queue (scripts.cc) behind sfall's
//     add_timer_event / remove_timer_event: scripts.cc is not in test_sources.
//   - The rest limb's own `!pipboyIsResting()` guard (scripts.cc) and the
//     gPipboyRestDepth counter (pipboy.cc): neither TU is linkable, so plan
//     §E.5 row 9 stays a mirror/grep gate.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "db.h"
#include "interpreter.h"
#include "sfall_global_scripts.h"
#include "scripts.h"

// F-21 pin (compile-time). scripts.cc stores this sentinel in Script::procs[]
// and every dispatch site compares against it (scripts.cc:1714, 1717, 1838,
// 2542, 2837). If the sentinel stopped being -1, every script would look like
// it implements every procedure and dispatch would call into empty slots.
static_assert(fallout::SCRIPT_PROC_NO_PROC == -1,
    "SCRIPT_PROC_NO_PROC must stay the -1 'unset procedure' sentinel");

namespace fallout {

// ---------------------------------------------------------------------------
// Strong overrides of shared stubs (declared weak in test_common_stubs.cc)
// ---------------------------------------------------------------------------

// Zero-initialised fake programs. Production only reads `flags` on them
// (sfall_global_scripts.cc:405) and compares pointer identity elsewhere, so a
// zeroed object is enough — no interpreter state is touched.
static constexpr int kFakeProgramCapacity = 4;
static Program gFakePrograms[kFakeProgramCapacity];
static int gFakeProgramUsed = 0;

// The observable of "the registered timer fired": production reaches
// programExecuteProcedure only through sfall_gl_scr_execute_proc_if_ready.
static int gExecutedProcedures = 0;

File* fileOpen(const char* /*filename*/, const char* /*mode*/)
{
    // sfall_gl_scr_exec_start_proc pre-checks that the script file exists
    // (sfall_global_scripts.cc:354-358) before calling programCreateByPath.
    static XFile fakeFile = {};
    return &fakeFile;
}

int fileNameListInit(const char* pattern, char*** fileNames)
{
    // Only the vanilla `scripts\gl*.int` scan finds a file. The RPU
    // `scripts\sfall\gl*.int` scan, the user-configured ddraw.ini paths and
    // the hs_*.int hook scan all report "no files" so exactly one global
    // script is registered.
    if (pattern != nullptr && std::strcmp(pattern, "scripts\\gl*.int") == 0) {
        static char fileName[] = "gl_timer_pin.int";
        static char* fileList[] = { fileName };
        *fileNames = fileList;
        return 1;
    }

    return 0;
}

Program* programCreateByPath(const char* /*path*/)
{
    if (gFakeProgramUsed >= kFakeProgramCapacity) {
        return nullptr;
    }

    return &gFakePrograms[gFakeProgramUsed++];
}

int programFindProcedure(Program* /*prg*/, const char* /*name*/)
{
    // The stubbed gScriptProcNames[] table is empty (test_common_stubs.cc:704
    // defines it as nullptr entries), so the observable contract is that every
    // proc lookup succeeds: production stores the returned index in
    // Script::procs[] and can then execute it.
    return 0;
}

void programExecuteProcedure(Program* /*program*/, int /*procedureIndex*/)
{
    gExecutedProcedures++;
}

} // namespace fallout

using namespace fallout;

// Resets every piece of state this file controls, re-initialises the module and
// registers exactly one global script. A TEST_CASE body runs once per SUBCASE
// and ends with sfall_gl_scr_exit(), so `state` is always null here and the
// module is rebuilt from scratch for every subcase.
static Program* pinRegisterSingleScript()
{
    gFakeProgramUsed = 0;
    gExecutedProcedures = 0;
    sfall_gl_scr_reset_load_count();

    sfall_gl_scr_init();
    sfall_gl_scr_exec_start_proc();

    return &gFakePrograms[0];
}

static void pinPumpMain(int times)
{
    for (int index = 0; index < times; index++) {
        sfall_gl_scr_process_main();
    }
}

TEST_CASE("PRODUCTION-LINKED: global script frame-interval timer fires once per `repeat` ticks")
{
    Program* scr = pinRegisterSingleScript();
    REQUIRE(scr == &gFakePrograms[0]);
    REQUIRE(gFakeProgramUsed == 1); // exactly one global script is registered

    SUBCASE("repeat=3 fires on the 3rd tick only")
    {
        sfall_gl_scr_set_type(scr, 0); // mode 0 = timed
        sfall_gl_scr_set_repeat(scr, 3);

        pinPumpMain(2);
        CHECK(gExecutedProcedures == 0); // 2 of 3 ticks

        pinPumpMain(1);
        CHECK(gExecutedProcedures == 1); // 3rd tick fires, counter reset

        pinPumpMain(2);
        CHECK(gExecutedProcedures == 1); // next interval half-elapsed

        pinPumpMain(1);
        CHECK(gExecutedProcedures == 2); // second interval fires
    }

    SUBCASE("F-M040: a new interval restarts the frame counter")
    {
        sfall_gl_scr_set_type(scr, 0);
        sfall_gl_scr_set_repeat(scr, 10);

        pinPumpMain(9);
        CHECK(gExecutedProcedures == 0); // 9 of 10 ticks

        // Without the `scr->count = 0` reset, count stays 9 and the very next
        // tick satisfies count >= 3 — the script would fire immediately.
        sfall_gl_scr_set_repeat(scr, 3);

        pinPumpMain(2);
        CHECK(gExecutedProcedures == 0);

        pinPumpMain(1);
        CHECK(gExecutedProcedures == 1);
    }

    SUBCASE("repeat=0 disables the timer")
    {
        sfall_gl_scr_set_type(scr, 0);
        sfall_gl_scr_set_repeat(scr, 0);

        pinPumpMain(10);
        CHECK(gExecutedProcedures == 0);
    }

    SUBCASE("negative repeat is rejected and the previous interval survives")
    {
        sfall_gl_scr_set_type(scr, 0);
        sfall_gl_scr_set_repeat(scr, 2);

        sfall_gl_scr_set_repeat(scr, -1); // rejected (sfall_global_scripts.cc:483)

        pinPumpMain(1);
        CHECK(gExecutedProcedures == 0);

        pinPumpMain(1);
        CHECK(gExecutedProcedures == 1); // still the 2-tick interval
    }

    SUBCASE("mode 1 (input) fires from the input pump only")
    {
        sfall_gl_scr_set_type(scr, 1);
        sfall_gl_scr_set_repeat(scr, 2);

        pinPumpMain(2);
        CHECK(gExecutedProcedures == 0);

        sfall_gl_scr_process_worldmap(); // pumps modes 2 and 3
        sfall_gl_scr_process_worldmap();
        CHECK(gExecutedProcedures == 0);

        sfall_gl_scr_process_input();
        sfall_gl_scr_process_input();
        CHECK(gExecutedProcedures == 1);
    }

    SUBCASE("mode 3 (always) fires from the main and worldmap pumps, not the input pump")
    {
        sfall_gl_scr_set_type(scr, 3);
        sfall_gl_scr_set_repeat(scr, 1);

        sfall_gl_scr_process_main(); // pumps modes 0 and 3
        CHECK(gExecutedProcedures == 1);

        sfall_gl_scr_process_input(); // pumps mode 1 only
        CHECK(gExecutedProcedures == 1);

        sfall_gl_scr_process_worldmap(); // pumps modes 2 and 3
        CHECK(gExecutedProcedures == 2);
    }

    SUBCASE("out-of-range type is rejected and the previous mode survives")
    {
        sfall_gl_scr_set_type(scr, 1);

        sfall_gl_scr_set_type(scr, 4); // rejected (sfall_global_scripts.cc:500)
        sfall_gl_scr_set_type(scr, -1); // rejected

        sfall_gl_scr_set_repeat(scr, 1);

        sfall_gl_scr_process_main();
        CHECK(gExecutedProcedures == 0); // still mode 1

        sfall_gl_scr_process_input();
        CHECK(gExecutedProcedures == 1);
    }

    sfall_gl_scr_exit();
}

TEST_CASE("PRODUCTION-LINKED: game_loaded() tri-state (F-058)")
{
    Program* scr = pinRegisterSingleScript();
    REQUIRE(scr == &gFakePrograms[0]);

    SUBCASE("first load reports 2, then 0 once consumed")
    {
        CHECK(sfall_gl_scr_is_loaded(scr) == 2);
        CHECK(sfall_gl_scr_is_loaded(scr) == 0);
    }

    SUBCASE("a load after increment_load_count reports 1")
    {
        sfall_gl_scr_increment_load_count();

        CHECK(sfall_gl_scr_is_loaded(scr) == 1);
    }

    SUBCASE("a program that is not a global script reports 0")
    {
        // &gFakePrograms[1] is allocated but never registered.
        CHECK(sfall_gl_scr_is_loaded(&gFakePrograms[1]) == 0);
        CHECK(sfall_gl_scr_is_loaded(&gFakePrograms[1]) == 0);
        CHECK(sfall_gl_scr_is_loaded(nullptr) == 0);
    }

    sfall_gl_scr_exit();
}
