#!/bin/sh
# Output: build/render/ (engine WAVs and logs), app/build/scenarios/ (simulator screenshots and audio).
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
FAIL=0
mkdir -p build/render
run() {
    name="$1"; shift
    if "$@" > "build/render/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (build/render/$name.log)"
        tail -15 "build/render/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run engine sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Werror -Wno-unused-function -DPLAT_HOST \
    -Iapp/dsp -Ibase/include -o build/render/render tests/host/render.c app/dsp/moss.c -lm && build/render/render build/render 30"
run stress sh -c "$CC -O1 -g -fsanitize=address,undefined -fno-sanitize=shift-base -fno-sanitize-recover=all \
    -ffp-contract=off -std=c99 -DPLAT_HOST -Iapp/dsp -Ibase/include -o build/render/stress tests/host/stress.c app/dsp/moss.c -lm \
    && build/render/stress 4"
run evolve sh -c "$CC -O2 -std=c99 -DPLAT_HOST -DMS_TEST_NO_CRACKLE -Iapp/dsp -Ibase/include \
    -o build/render/evolve tests/host/evolve.c app/dsp/moss.c -lm && build/render/evolve"
run clicks sh -c "$CC -O2 -ffp-contract=off -std=c99 -DPLAT_HOST -DMS_TEST_NO_CRACKLE -Iapp/dsp -Ibase/include \
    -o build/render/clicks tests/host/clicks.c app/dsp/moss.c -lm && build/render/clicks 0.035"
run knobs sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Werror -Wno-unused-function -DPLAT_HOST \
    -Iapp/dsp -Ibase/include -o build/render/knobs tests/host/knobs.c app/dsp/moss.c -lm && build/render/knobs"
run host-build sh build.sh --host
for s in tests/scenarios/*.txt; do
    n=$(basename "$s" .txt)
    mkdir -p "app/build/scenarios/$n"
    run "scenario-$n" app/build/host/moss_host "$s" "app/build/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
