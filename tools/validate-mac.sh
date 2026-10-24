#!/usr/bin/env bash
# Local macOS validation (D-016): macOS has no CI runner, so this script is the macOS gate.
#
#   tools/validate-mac.sh [--skip-asan]
#
# Configures, builds and tests the mac-release preset (the same test set CI runs on Windows), repeats the
# build and tests with AddressSanitizer + UndefinedBehaviorSanitizer (mac-asan), ad-hoc signs and verifies
# the .app bundle, then prints a summary block to paste into the pull request.
#
# Requirements: macOS 13+, Xcode Command Line Tools, CMake >= 3.25, and VCPKG_ROOT pointing at a
# bootstrapped vcpkg checkout. Exits non-zero if any step fails.

set -uo pipefail

skip_asan=0
for arg in "$@"; do
  case "$arg" in
    --skip-asan) skip_asan=1 ;;
    -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 2
log_dir="$root/build/validate-mac"
mkdir -p "$log_dir"
start=$(date +%s)
failed=0

die() { echo "error: $*" >&2; exit 2; }

[[ "$(uname -s)" == "Darwin" ]] || die "this script must run on macOS"
command -v cmake >/dev/null || die "cmake not found"
command -v ctest >/dev/null || die "ctest not found"
xcrun --find clang >/dev/null 2>&1 || die "Xcode Command Line Tools not found (xcode-select --install)"
[[ -n "${VCPKG_ROOT:-}" && -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]] \
  || die "VCPKG_ROOT must point at a vcpkg checkout (git clone https://github.com/microsoft/vcpkg && ./vcpkg/bootstrap-vcpkg.sh)"

jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# run_step <name> <command...>: runs a command with its output logged; returns its exit status.
run_step() {
  local name="$1"; shift
  local logfile="$log_dir/${name// /-}.log"
  echo "==> $name"
  if "$@" >"$logfile" 2>&1; then
    return 0
  fi
  echo "    FAILED (log: $logfile)"
  tail -n 30 "$logfile" | sed 's/^/    /'
  return 1
}

# test_summary <logfile>: "<passed> passed, <failed> failed, <skipped> skipped of <total>"
test_summary() {
  local logfile="$1" total failed_count skipped
  total=$(sed -n 's/.*tests failed out of \([0-9][0-9]*\).*/\1/p' "$logfile" | tail -n 1)
  failed_count=$(sed -n 's/.*tests passed, \([0-9][0-9]*\) tests* failed out of.*/\1/p' "$logfile" | tail -n 1)
  skipped=$(grep -c '(Skipped)$' "$logfile" || true)
  if [[ -z "$total" ]]; then
    echo "no results"
    return
  fi
  failed_count=${failed_count:-0}
  echo "$((total - failed_count - skipped)) passed, $failed_count failed, $skipped skipped of $total"
}

# build_and_test <preset> -> sets build_result and test_result
build_and_test() {
  local preset="$1"
  build_result="FAIL"
  test_result="not run"
  if run_step "$preset configure" cmake --preset "$preset" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
     && run_step "$preset build" cmake --build --preset "$preset" -j "$jobs"; then
    build_result="ok"
    local status=0
    run_step "$preset test" ctest --preset "$preset" -j "$jobs" || status=1
    test_result="$(test_summary "$log_dir/$preset-test.log")"
    if [[ $status -ne 0 ]]; then
      test_result="FAIL: $test_result"
      failed=1
    fi
  else
    failed=1
  fi
}

build_and_test mac-release
release_build="$build_result"
release_tests="$test_result"

if [[ $skip_asan -eq 1 ]]; then
  asan_build="skipped (--skip-asan)"
  asan_tests="skipped (--skip-asan)"
else
  build_and_test mac-asan
  asan_build="$build_result"
  asan_tests="$test_result"
fi

bundle="$root/build/mac-release/src/app/genesisEmu.app"
if [[ "$release_build" == "ok" && -d "$bundle" ]]; then
  if run_step "app bundle sign" codesign --force --deep --sign - "$bundle" \
     && run_step "app bundle verify" codesign --verify --deep --strict "$bundle" \
     && [[ -x "$bundle/Contents/MacOS/genesisEmu" && -f "$bundle/Contents/Resources/genesisEmu.icns" ]]; then
    app_bundle="ok ($bundle, ad-hoc signed)"
  else
    app_bundle="FAIL"
    failed=1
  fi
else
  app_bundle="FAIL (not built)"
  failed=1
fi

duration=$(( $(date +%s) - start ))
result="PASS"
[[ $failed -ne 0 ]] && result="FAIL"

dirty=""
[[ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ]] && dirty=" (uncommitted changes)"

cat <<EOF

Paste this block into the pull request:

\`\`\`
genesisEmu macOS validation
commit: $(git rev-parse --short HEAD 2>/dev/null || echo unknown)$dirty
macOS: $(sw_vers -productVersion) ($(sw_vers -buildVersion))
arch: $(uname -m)
xcode clang: $(xcrun clang --version | head -n 1)
release build: $release_build
release tests: $release_tests
asan/ubsan build: $asan_build
asan/ubsan tests: $asan_tests
app bundle: $app_bundle
duration: $((duration / 60))m $((duration % 60))s
result: $result
\`\`\`
EOF

[[ $failed -eq 0 ]]
