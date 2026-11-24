#!/usr/bin/env bash
# mayhem/build.sh — builds the zynaddsubfx fuzz harnesses (sanitized core lib + rtosc) and the
# upstream unit-test suite (normal flags) that mayhem/test.sh runs. Re-runnable and offline:
# every dependency (fftw3f, mxml, liblo, zlib) comes from apt packages baked into the image;
# rtosc/tlsf are in the source tree (rtosc is a git submodule checked out by CI).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
[ -f rtosc/CMakeLists.txt ] || { echo "rtosc submodule missing (git submodule update --init)" >&2; exit 1; }

# Headless, plugin-less core build. No audio/MIDI backends, no GUI.
COMMON_CMAKE=(-DGuiModule=off -DPluginEnable=OFF -DCompileTests=ON
  -DDefaultOutput=null -DDefaultInput=null -DAlsaEnable=OFF -DJackEnable=OFF -DOssEnable=OFF
  -DPaEnable=OFF -DSndioEnable=OFF -DLashEnable=OFF -DDssiEnable=OFF)

# 1) Sanitized + DWARF-3 project build (core lib + rtosc) for the harnesses.
# UBSan shift-base is relaxed (only when sanitizers are on): the bundled TLSF allocator does
# `1 << fl` with fl == 31 on every allocation (tlsf/tlsf.c:424), which would halt every input.
# UBSan bool is relaxed too: AnalogFilter's constructor calls setfreq_and_q() before it sets
# beforeFirstTick (src/DSP/AnalogFilter.cpp:48 vs :53), so an uninitialized bool is loaded at
# :301 every time a filter is built, i.e. on every input. UBSan alignment is relaxed: MiddleWare
# hands parameter objects to the realtime side as pointers inside OSC blobs, which OSC only
# aligns to 4 bytes, so every kit/pad/sub enable (e.g. src/Misc/Part.cpp:282) is a misaligned
# 8-byte load by design. ASan and the rest of UBSan stay on and halting.
RELAX=""
[ -n "$SANITIZER_FLAGS" ] && RELAX="-fno-sanitize=shift-base,bool,alignment"
FUZZ_FLAGS="$SANITIZER_FLAGS $RELAX $DEBUG_FLAGS"
# The project code itself carries the libFuzzer coverage instrumentation (not only the harness).
LIB_FLAGS="$FUZZ_FLAGS"
[ -n "$LIB_FUZZING_ENGINE" ] && LIB_FLAGS="$FUZZ_FLAGS -fsanitize=fuzzer-no-link"
cmake -S . -B mayhem/build-fuzz "${COMMON_CMAKE[@]}" -DBuildForDebug=ON \
  -DZynBuildOptionsDebug="-O1" -DCMAKE_C_FLAGS_DEBUG="" -DCMAKE_CXX_FLAGS_DEBUG="" \
  -DCMAKE_C_FLAGS="$LIB_FLAGS" -DCMAKE_CXX_FLAGS="$LIB_FLAGS"
cmake --build mayhem/build-fuzz -j"$MAYHEM_JOBS" --target zynaddsubfx_core zynaddsubfx_nio zynaddsubfx_gui_bridge rtosc rtosc-cpp

B=mayhem/build-fuzz
INC=(-I"$SRC/src" -I"$SRC/$B/src" -I"$SRC/rtosc/include" -I"$SRC/tlsf" -I"$SRC/mayhem/harnesses")
LIBS=(-Wl,--start-group "$B/src/libzynaddsubfx_core.a" "$B/src/Nio/libzynaddsubfx_nio.a" "$B/src/libzynaddsubfx_gui_bridge.a" "$B/rtosc/librtosc-cpp.a" "$B/rtosc/librtosc.a" -Wl,--end-group
  $(pkg-config --libs fftw3f mxml liblo zlib) -lpthread -lm)

$CXX $FUZZ_FLAGS -c mayhem/lsan_off.cc -o $B/lsan_off.o
$CXX $FUZZ_FLAGS -std=c++20 "${INC[@]}" -c mayhem/harnesses/zyn_globals.cpp -o $B/zyn_globals.o
$CC $FUZZ_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o $B/standalone_main.o
for h in fuzz_master_xml fuzz_middleware_osc fuzz_microtonal fuzz_rtosc_pretty fuzz_part_xiz fuzz_rtosc_message; do
  $CXX $FUZZ_FLAGS -std=c++20 "${INC[@]}" -c "mayhem/harnesses/$h.cpp" -o "$B/$h.o"
  $CXX $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$B/$h.o" $B/lsan_off.o $B/zyn_globals.o "${LIBS[@]}" -o "/mayhem/$h"
  $CXX $FUZZ_FLAGS "$B/$h.o" $B/standalone_main.o $B/lsan_off.o $B/zyn_globals.o "${LIBS[@]}" -o "/mayhem/$h-standalone"
done

# 2) Upstream unit tests with the project's normal flags (independent build tree).
cmake -S . -B mayhem/build-test "${COMMON_CMAKE[@]}" \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build mayhem/build-test -j"$MAYHEM_JOBS"
