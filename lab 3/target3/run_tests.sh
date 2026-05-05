#!/bin/bash
# Run all attack lab phases
# Usage: ./run_tests.sh [phase_number]
# Run on a Linux x86-64 machine with the -q flag (no server contact)

cd "$(dirname "$0")"

run_phase() {
    local phase=$1
    local binary=$2
    echo "========== Phase $phase =========="
    ./hex2raw < "phase${phase}.txt" | ./${binary} -q
    echo ""
}

if [ -n "$1" ]; then
    case $1 in
        1|2|3) run_phase $1 ctarget ;;
        4|5)   run_phase $1 rtarget ;;
        *)     echo "Usage: $0 [1-5]" ;;
    esac
else
    for p in 1 2 3; do run_phase $p ctarget; done
    for p in 4 5; do run_phase $p rtarget; done
fi
