#!/usr/bin/env bash
# mayhem/test.sh — runs zynaddsubfx's own CTest suite (built by mayhem/build.sh in
# mayhem/build-test with the project's normal flags). The suite asserts values (TS_ASSERT_*:
# rendered sample values, parameter round trips, OSC replies, saved-file contents), so a
# no-op program fails it. TrailingWhitespace is excluded: it is a source-style lint over the
# git tree, not a functional test.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BT=mayhem/build-test
if [ ! -f "$BT/CTestTestfile.cmake" ]; then
  echo "test suite not built ($BT missing) — run mayhem/build.sh first" >&2
  emit_ctrf cmake-ctest 0 1 0
  exit 1
fi

# A test counts as passed only if ctest passed it AND (for the suites built on upstream's
# test-suite.h / rtosc's test.h) it printed its own "# 0 test(s) failed out of N" (N>0) summary —
# the assertion count is the behavioral evidence; an exit status alone is not.
NO_SUMMARY=" ControllerTest metadata performance PortChecker SaveOscBigFile SaveOscPresets sugar test-walker "
# PortChecker queries every port of a running zyn over liblo UDP and waits a fixed time for each
# reply, so it fails ("parameter not queryable") when it shares the CPUs with the parallel batch
# on a loaded host. It runs alone, after the batch; each run's LastTest.log is kept and parsed.
LOG=$(mktemp); trap 'rm -f "$LOG"' EXIT
TLOG="$BT/Testing/Temporary/LastTest.log"
for sel in "-j$MAYHEM_JOBS -E ^(TrailingWhitespace|PortChecker)\$" "-R ^PortChecker\$"; do
  rm -f "$TLOG"
  # shellcheck disable=SC2086
  (cd "$BT" && ctest $sel --output-on-failure) 2>&1 | tail -n 40
  [ -f "$TLOG" ] || { echo "no ctest log ($sel)" >&2; emit_ctrf cmake-ctest 0 1 0; exit 1; }
  cat "$TLOG" >> "$LOG"
done
passed=0; failed=0
while read -r name status summary; do
  if [ "$status" = P ] && { [ "$summary" = 1 ] || [[ "$NO_SUMMARY" == *" $name "* ]]; }; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1)); echo "FAIL: $name (ctest=$status, assertion-summary=$summary)"
  fi
done < <(awk '/^[0-9]+\/[0-9]+ Testing: /{name=$3; m=0}
             /^# 0 test\(s\) failed out of [1-9]/{m=1}
             /^Test Passed/{print name, "P", m}
             /^Test Failed/{print name, "F", m}' "$LOG")
emit_ctrf cmake-ctest "$passed" "$failed" 0
