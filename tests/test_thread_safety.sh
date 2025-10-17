#!/bin/bash

set -o pipefail

EXECUTABLE="./disk_sim"
DISK_FILE="test_thread_safety.img"

TOTAL_TESTS=0
FAILED_TESTS=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_heading() {
  echo -e "\n${YELLOW}===== $1 =====${NC}"
}

print_result() {
  local success=$1
  local description="$2"
  local output="$3"
  local detail="$4"

  local padded
  padded=$(printf '  - %-60s' "$description")
  local dotted=${padded// /.}
  printf "%s" "$dotted"

  if [ "$success" -eq 0 ]; then
    printf " [%bPASS%b]\n" "$GREEN" "$NC"
    [ -n "$detail" ] && echo -e "    └─ $detail"
  else
    printf " [%bFAIL%b]\n" "$RED" "$NC"
    [ -n "$detail" ] && echo -e "    ├─ $detail"
    echo -e "    └─ Output:\n$output"
    ((FAILED_TESTS++))
  fi
}

run_expect_success() {
  ((TOTAL_TESTS++))
  local description="$1"
  local expected="$2"
  shift 2
  local display="$*"

  local output
  output=$("$@" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ] && [[ "$output" == *"$expected"* ]]; then
    print_result 0 "$description" "$output" "Command: $display"
  else
    print_result 1 "$description" "$output" "Command: $display (expected substring: $expected)"
  fi
}

run_expect_failure() {
  ((TOTAL_TESTS++))
  local description="$1"
  local expected="$2"
  shift 2
  local display="$*"

  local output
  output=$("$@" 2>&1)
  local exit_code=$?

  if [ $exit_code -ne 0 ] && [[ "$output" == *"$expected"* ]]; then
    print_result 0 "$description" "$output" "Command: $display"
  else
    print_result 1 "$description" "$output" "Command: $display (expected failure substring: $expected)"
  fi
}

run_expect_exit_success() {
  ((TOTAL_TESTS++))
  local description="$1"
  shift
  local display="$*"

  local output
  output=$("$@" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ]; then
    print_result 0 "$description" "$output" "Command: $display"
  else
    print_result 1 "$description" "$output" "Command: $display (expected zero exit code)"
  fi
}

assert_contains() {
  ((TOTAL_TESTS++))
  local description="$1"
  local path="$2"
  shift 2
  local expected=("$@")

  local output
  output=$($EXECUTABLE $DISK_FILE ls "$path" 2>&1)
  local exit_code=$?
  local missing=()

  if [ $exit_code -eq 0 ]; then
    for name in "${expected[@]}"; do
      [[ "$output" == *"$name"* ]] || missing+=("$name")
    done
  else
    missing+=("ls_failed")
  fi

  if [ ${#missing[@]} -eq 0 ]; then
    print_result 0 "$description" "$output" "Command: $EXECUTABLE $DISK_FILE ls $path"
  else
    print_result 1 "$description" "$output" "Command: $EXECUTABLE $DISK_FILE ls $path (missing: ${missing[*]})"
  fi
}

assert_absent() {
  ((TOTAL_TESTS++))
  local description="$1"
  local path="$2"
  shift 2
  local forbidden=("$@")

  local output
  output=$($EXECUTABLE $DISK_FILE ls "$path" 2>&1)
  local exit_code=$?
  local present=()

  if [ $exit_code -eq 0 ]; then
    for name in "${forbidden[@]}"; do
      [[ "$output" == *"$name"* ]] && present+=("$name")
    done
  else
    present+=("ls_failed")
  fi

  if [ ${#present[@]} -eq 0 ]; then
    print_result 0 "$description" "$output" "Command: $EXECUTABLE $DISK_FILE ls $path"
  else
    print_result 1 "$description" "$output" "Command: $EXECUTABLE $DISK_FILE ls $path (unexpected: ${present[*]})"
  fi
}

ensure_build() {
  print_heading "Build"
  if ! make clean >/dev/null 2>&1 || ! make >/dev/null 2>&1; then
    echo -e "${RED}Build failed.${NC}"
    make
    exit 1
  fi
  echo "Build completed."
}

prepare_environment() {
  print_heading "Environment Setup"
  rm -f "$DISK_FILE"
  run_expect_success "Create 20MB disk" "Disk created successfully" $EXECUTABLE "$DISK_FILE" create 20
  run_expect_success "Format disk" "Disk formatted successfully" $EXECUTABLE "$DISK_FILE" format
  run_expect_success "Create test workspace" "Directory created" $EXECUTABLE "$DISK_FILE" mkdir /ts
}

cleanup_environment() {
  print_heading "Environment Cleanup"
  make clean >/dev/null 2>&1
  rm -f "$DISK_FILE"
  echo "Cleanup complete."
}

test_concurrent_creation() {
  print_heading "Concurrent Creation"
  run_expect_success "Spawn concurrent creators" "creation-batch" bash -c "($EXECUTABLE $DISK_FILE multithreaded touch /ts/a.txt & $EXECUTABLE $DISK_FILE multithreaded touch /ts/b.txt & $EXECUTABLE $DISK_FILE multithreaded mkdir /ts/sub & wait; echo creation-batch)"
  assert_contains "Artifacts exist" /ts a.txt b.txt sub
}

test_concurrent_writes() {
  print_heading "Concurrent Writes"
  run_expect_success "Seed shared file" "File created" $EXECUTABLE "$DISK_FILE" touch /ts/shared.txt
  run_expect_success "Concurrent write wave" "writers-complete" bash -c "($EXECUTABLE $DISK_FILE multithreaded echo 'Writer one' > /ts/shared.txt & $EXECUTABLE $DISK_FILE multithreaded echo 'Writer two' > /ts/shared.txt & $EXECUTABLE $DISK_FILE multithreaded echo 'Writer three' > /ts/shared.txt & wait; echo writers-complete)"
  run_expect_exit_success "Shared file readable" $EXECUTABLE "$DISK_FILE" cat /ts/shared.txt
}

test_consistency_rounds() {
  print_heading "Consistency Rounds"
  for round in $(seq 1 3); do
    run_expect_success "Round ${round} write" "Written to file" $EXECUTABLE "$DISK_FILE" multithreaded echo "Round ${round} payload" '>' /ts/shared.txt
    run_expect_success "Round ${round} read" "Round ${round} payload" $EXECUTABLE "$DISK_FILE" multithreaded cat /ts/shared.txt
  done
}

test_copy_integrity() {
  print_heading "Copy Integrity"
  run_expect_success "Normalize shared file" "Written to file" $EXECUTABLE "$DISK_FILE" echo "Thread baseline" '>' /ts/shared.txt
  run_expect_success "Parallel copies" "copy-done" bash -c "($EXECUTABLE $DISK_FILE multithreaded copy /ts/shared.txt /ts/copy1.txt & $EXECUTABLE $DISK_FILE multithreaded copy /ts/shared.txt /ts/copy2.txt & $EXECUTABLE $DISK_FILE multithreaded copy /ts/shared.txt /ts/copy3.txt & wait; echo copy-done)"
  run_expect_success "Read copy1" "Thread baseline" $EXECUTABLE "$DISK_FILE" cat /ts/copy1.txt
  run_expect_success "Read copy2" "Thread baseline" $EXECUTABLE "$DISK_FILE" cat /ts/copy2.txt
  run_expect_success "Read copy3" "Thread baseline" $EXECUTABLE "$DISK_FILE" cat /ts/copy3.txt
}

test_stress_smoke() {
  print_heading "Stress Smoke"
  run_expect_success "Execute stress command" "Test finished successfully" $EXECUTABLE "$DISK_FILE" stress --duration 2 --files 8 --threads 2 --write-size 512 --monitor 1 --workspace /stress_ts --cleanup
  assert_absent "Stress residuals cleaned" / stress_ts
}

test_cleanup_operations() {
  print_heading "Cleanup"
  run_expect_success "Remove copy1" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/copy1.txt
  run_expect_success "Remove copy2" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/copy2.txt
  run_expect_success "Remove copy3" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/copy3.txt
  run_expect_success "Remove shared" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/shared.txt
  run_expect_success "Remove a.txt" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/a.txt
  run_expect_success "Remove b.txt" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/b.txt
  run_expect_success "Remove sub dir" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /ts/sub
  assert_absent "Workspace clean" /ts copy1.txt copy2.txt copy3.txt sub shared.txt a.txt b.txt
}

main() {
  ensure_build
  prepare_environment

  test_concurrent_creation
  test_concurrent_writes
  test_consistency_rounds
  test_copy_integrity
  test_stress_smoke
  test_cleanup_operations

  cleanup_environment

  print_heading "Test Summary"
  echo "Total tests: $TOTAL_TESTS"
  if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All thread safety tests passed.${NC}"
    exit 0
  else
    echo -e "${RED}$FAILED_TESTS test(s) failed.${NC}"
    exit 1
  fi
}

main
