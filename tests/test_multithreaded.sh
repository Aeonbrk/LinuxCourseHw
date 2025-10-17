#!/bin/bash

set -o pipefail

EXECUTABLE="./disk_sim"
DISK_FILE="test_multithreaded.img"

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
  local command_display="$*"

  local output
  output=$("$@" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ] && [[ "$output" == *"$expected"* ]]; then
    print_result 0 "$description" "$output" "Command: $command_display"
  else
    print_result 1 "$description" "$output" "Command: $command_display (expected substring: $expected)"
  fi
}

run_expect_failure() {
  ((TOTAL_TESTS++))
  local description="$1"
  local expected="$2"
  shift 2
  local command_display="$*"

  local output
  output=$("$@" 2>&1)
  local exit_code=$?

  if [ $exit_code -ne 0 ] && [[ "$output" == *"$expected"* ]]; then
    print_result 0 "$description" "$output" "Command: $command_display"
  else
    print_result 1 "$description" "$output" "Command: $command_display (expected failure substring: $expected)"
  fi
}

assert_contains() {
  ((TOTAL_TESTS++))
  local description="$1"
  local path="$2"
  shift 2
  local expected_items=("$@")

  local output
  output=$($EXECUTABLE $DISK_FILE ls "$path" 2>&1)
  local exit_code=$?
  local missing=()

  if [ $exit_code -eq 0 ]; then
    for item in "${expected_items[@]}"; do
      [[ "$output" == *"$item"* ]] || missing+=("$item")
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
    for item in "${forbidden[@]}"; do
      [[ "$output" == *"$item"* ]] && present+=("$item")
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
  run_expect_success "Create 10MB disk" "Disk created successfully" $EXECUTABLE "$DISK_FILE" create 10
  run_expect_success "Format disk" "Disk formatted successfully" $EXECUTABLE "$DISK_FILE" format
  run_expect_success "Create dispatcher workspace" "Directory created" $EXECUTABLE "$DISK_FILE" mkdir /mt
}

cleanup_environment() {
  print_heading "Environment Cleanup"
  make clean >/dev/null 2>&1
  rm -f "$DISK_FILE"
  echo "Cleanup complete."
}

test_basic_multithreaded_commands() {
  print_heading "Multithreaded Basics"
  run_expect_success "List root via dispatcher" "./" $EXECUTABLE "$DISK_FILE" multithreaded ls /
  run_expect_success "Create file via dispatcher" "File created" $EXECUTABLE "$DISK_FILE" multithreaded touch /mt/alpha.txt
  run_expect_success "Write via dispatcher" "Written to file" $EXECUTABLE "$DISK_FILE" multithreaded echo "Concurrent hello" '>' /mt/alpha.txt
  run_expect_success "Read via dispatcher" "Concurrent hello" $EXECUTABLE "$DISK_FILE" multithreaded cat /mt/alpha.txt
}

test_multithreaded_copy() {
  print_heading "Copy Operations"
  run_expect_success "Seed source file" "Written to file" $EXECUTABLE "$DISK_FILE" echo "Hello from source" '>' /source.txt
  run_expect_success "Copy single-thread" "File copied" $EXECUTABLE "$DISK_FILE" copy /source.txt /dest.txt
  run_expect_success "Copy via dispatcher" "File copied" $EXECUTABLE "$DISK_FILE" multithreaded copy /source.txt /dest_mt.txt
  run_expect_success "Validate dispatcher copy" "Hello from source" $EXECUTABLE "$DISK_FILE" cat /dest_mt.txt
}

test_parallel_activity() {
  print_heading "Parallel Activity"
  run_expect_success "Spawn concurrent touches" "parallel-done" bash -c "($EXECUTABLE $DISK_FILE multithreaded touch /mt/t1.txt & $EXECUTABLE $DISK_FILE multithreaded touch /mt/t2.txt & $EXECUTABLE $DISK_FILE multithreaded touch /mt/t3.txt & wait; echo parallel-done)"
  assert_contains "All touch results present" /mt t1.txt t2.txt t3.txt

  run_expect_success "Dispatcher batch workload" "batch.txt" $EXECUTABLE "$DISK_FILE" multithreaded "touch /mt/job1.txt; touch /mt/job2.txt; echo 'batch payload' > /mt/batch.txt"
  assert_contains "Batch artifacts created" /mt job1.txt job2.txt batch.txt

  run_expect_success "Remove batch file" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/batch.txt
  run_expect_success "Remove job1" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/job1.txt
  run_expect_success "Remove job2" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/job2.txt
  assert_absent "Batch workspace clean" /mt batch.txt job1.txt job2.txt
}

test_error_paths() {
  print_heading "Dispatcher Errors"
  run_expect_failure "Reject unknown dispatcher command" "Unknown command" $EXECUTABLE "$DISK_FILE" multithreaded invalidcommand
  run_expect_failure "Missing source copy" "File not found" $EXECUTABLE "$DISK_FILE" multithreaded copy /missing.txt /nowhere.txt
}

test_stress_command() {
  print_heading "Stress Command"
  run_expect_success "Run short stress workload" "Test finished successfully" $EXECUTABLE "$DISK_FILE" stress --duration 2 --files 6 --threads 2 --write-size 512 --monitor 1 --workspace /stress_ci --cleanup
  assert_absent "Stress workspace cleaned" / stress_ci
}

test_cleanup() {
  print_heading "Dispatcher Cleanup"
  run_expect_success "Remove parallel touches" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/t1.txt
  run_expect_success "Remove parallel touches" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/t2.txt
  run_expect_success "Remove parallel touches" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/t3.txt
  run_expect_success "Remove dispatcher file" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt/alpha.txt
  run_expect_success "Remove source" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /source.txt
  run_expect_success "Remove dest" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /dest.txt
  run_expect_success "Remove dispatcher dest" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /dest_mt.txt
  run_expect_success "Remove workspace directory" "Removed" $EXECUTABLE "$DISK_FILE" multithreaded rm /mt
  assert_absent "Workspace removed" / mt
}

main() {
  ensure_build
  prepare_environment

  test_basic_multithreaded_commands
  test_multithreaded_copy
  test_parallel_activity
  test_error_paths
  test_stress_command
  test_cleanup

  cleanup_environment

  print_heading "Test Summary"
  echo "Total tests: $TOTAL_TESTS"
  if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All multithreaded tests passed.${NC}"
    exit 0
  else
    echo -e "${RED}$FAILED_TESTS test(s) failed.${NC}"
    exit 1
  fi
}

main
