#!/bin/bash

set -o pipefail

EXECUTABLE="./disk_sim"
DISK_FILE="test_functionality.img"

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

run_cli_batch() {
  ((TOTAL_TESTS++))
  local description="$1"
  local expected="$2"
  local payload="$3"

  local output
  output=$(printf "%b" "$payload" | $EXECUTABLE $DISK_FILE run 2>&1)
  local exit_code=${PIPESTATUS[1]:-${PIPESTATUS[0]}}

  if [ $exit_code -eq 0 ] && [[ "$output" == *"$expected"* ]]; then
    print_result 0 "$description" "$output" "Batch Input:\n$payload"
  else
    print_result 1 "$description" "$output" "Batch Input:\n$payload"
  fi
}

assert_absent() {
  ((TOTAL_TESTS++))
  local description="$1"
  local path="$2"
  local name="$3"

  local output
  output=$($EXECUTABLE $DISK_FILE ls "$path" 2>&1)
  local exit_code=$?

  if [ $exit_code -eq 0 ] && [[ "$output" != *"$name"* ]]; then
    print_result 0 "$description" "$output" "Command: $EXECUTABLE $DISK_FILE ls $path"
  else
    print_result 1 "$description" "$output" "Command: $EXECUTABLE $DISK_FILE ls $path (expected missing: $name)"
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
}

cleanup_environment() {
  print_heading "Environment Cleanup"
  make clean >/dev/null 2>&1
  rm -f "$DISK_FILE"
  echo "Cleanup complete."
}

test_root_listing() {
  print_heading "Root Listing"
  run_expect_success "Implicit root listing" "./" $EXECUTABLE "$DISK_FILE" ls
  run_expect_success "Explicit root listing" "./" $EXECUTABLE "$DISK_FILE" ls /
}

test_basic_operations() {
  print_heading "Directory and File Operations"
  run_expect_success "Create directory /docs" "Directory created" $EXECUTABLE "$DISK_FILE" mkdir /docs
  run_expect_success "Create directory /docs/logs" "Directory created" $EXECUTABLE "$DISK_FILE" mkdir /docs/logs
  run_expect_success "Create file /docs/readme.txt" "File created" $EXECUTABLE "$DISK_FILE" touch /docs/readme.txt
  run_expect_success "Write content to readme" "Written to file" $EXECUTABLE "$DISK_FILE" echo 'Disk simulator functional test' \> /docs/readme.txt
  run_expect_success "Read readme" "Disk simulator functional test" $EXECUTABLE "$DISK_FILE" cat /docs/readme.txt
  run_expect_success "List /docs" "readme.txt" $EXECUTABLE "$DISK_FILE" ls /docs
  run_expect_success "List /docs/logs" "./" $EXECUTABLE "$DISK_FILE" ls /docs/logs
}

test_copy_and_removal() {
  print_heading "Copy and Removal"
  run_expect_success "Copy readme" "File copied" $EXECUTABLE "$DISK_FILE" copy /docs/readme.txt /docs/manual.txt
  run_expect_success "Verify manual content" "Disk simulator functional test" $EXECUTABLE "$DISK_FILE" cat /docs/manual.txt
  run_expect_success "Remove manual" "Removed" $EXECUTABLE "$DISK_FILE" rm /docs/manual.txt
  assert_absent "Manual deleted" /docs manual.txt
  run_expect_success "Remove readme" "Removed" $EXECUTABLE "$DISK_FILE" rm /docs/readme.txt
  assert_absent "Readme deleted" /docs readme.txt
}

test_cli_mode() {
  print_heading "CLI Run Mode"
  local batch="help\nmkdir /cli-suite\nls /\nrm /cli-suite\nexit\n"
  run_cli_batch "Execute batch" "cli-suite" "$batch"
}

test_info_command() {
  print_heading "Information Queries"
  run_expect_success "Disk info basic" "Disk Information" $EXECUTABLE "$DISK_FILE" info
  run_expect_success "Disk info blocks" "Total Blocks" $EXECUTABLE "$DISK_FILE" info
  run_expect_success "Disk info inodes" "Total Inodes" $EXECUTABLE "$DISK_FILE" info
}

test_error_paths() {
  print_heading "Error Handling"
  run_expect_failure "Read missing file" "File not found" $EXECUTABLE "$DISK_FILE" cat /missing.txt
  run_expect_failure "Duplicate mkdir" "already exists" $EXECUTABLE "$DISK_FILE" mkdir /docs
  run_expect_failure "Duplicate touch" "File already exists" $EXECUTABLE "$DISK_FILE" touch /docs/logs
  run_expect_failure "Create dir without parent" "Parent directory not found" $EXECUTABLE "$DISK_FILE" mkdir /ghost/dir
  run_expect_failure "Create file without parent" "Parent directory not found" $EXECUTABLE "$DISK_FILE" touch /ghost/file.txt
  run_expect_failure "Remove non-empty" "Directory not empty" $EXECUTABLE "$DISK_FILE" rm /docs
  run_expect_failure "Remove missing" "File not found" $EXECUTABLE "$DISK_FILE" rm /absent
  run_expect_failure "Remove root" "Cannot remove root directory" $EXECUTABLE "$DISK_FILE" rm /
}

test_reformat() {
  print_heading "Reformat"
  run_expect_success "Format disk again" "Disk formatted successfully" $EXECUTABLE "$DISK_FILE" format
  run_expect_success "Root after format" "./" $EXECUTABLE "$DISK_FILE" ls /
}

main() {
  ensure_build
  prepare_environment

  test_root_listing
  test_basic_operations
  test_copy_and_removal
  test_cli_mode
  test_info_command
  test_error_paths
  test_reformat

  cleanup_environment

  print_heading "Test Summary"
  echo "Total tests: $TOTAL_TESTS"
  if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All functional tests passed.${NC}"
    exit 0
  else
    echo -e "${RED}$FAILED_TESTS test(s) failed.${NC}"
    exit 1
  fi
}

main
#!/bin/bash

# ==============================================================================
# 用户态文件系统综合测试脚本
#
# 目的: 验证项目是否满足 requirements.md 中的所有功能需求。
# 特性:
#   - 清晰的模块化测试功能。
#   - 对每个命令的输出进行严格验证。
#   - 使用彩色标记的 PASS/FAIL 状态消息。
#   - 包含创建、删除和错误处理的测试。
# ==============================================================================

# ------------------------------------------------------------------------------
# 配置与辅助函数
# ------------------------------------------------------------------------------
DISK_FILE="test_disk.img"
EXECUTABLE="./disk_sim"
TEST_COUNT=0
FAIL_COUNT=0

# 用于输出的颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # 无颜色

# 辅助函数：运行一个测试并验证其输出
# 用法: run_test "测试描述" "预期输出" "要执行的完整命令"
run_test() {
    ((TEST_COUNT++))
    local description="$1"
    local expected_output="$2"
    local command="$3"

    local padded_desc=$(printf '  - %-55s' "$description")
    local dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"

    output=$(eval "$command" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && [[ "$output" == *"$expected_output"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $command"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    ├─ 命令: $command"
        echo -e "    ├─ 退出码: $exit_code (预期为 0)"
        echo -e "    ├─ 输出: \n$output"
        echo -e "    └─ 预期包含: $expected_output"
        ((FAIL_COUNT++))
    fi
}

# 辅助函数：测试预期会失败的命令
# 用法: run_fail_test "测试描述" "预期错误" "要执行的完整命令"
run_fail_test() {
    ((TEST_COUNT++))
    local description="$1"
    local expected_error="$2"
    local command="$3"

    local padded_desc=$(printf '  - %-55s' "$description")
    local dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"

    output=$(eval "$command" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ] && [[ "$output" == *"$expected_error"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $command"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    ├─ 命令: $command"
        echo -e "    ├─ 退出码: $exit_code (预期为非 0)"
        echo -e "    ├─ 输出: \n$output"
        echo -e "    └─ 预期包含错误: $expected_error"
        ((FAIL_COUNT++))
    fi
}

# ------------------------------------------------------------------------------
# 测试阶段
# ------------------------------------------------------------------------------

setup() {
    echo -e "\n${YELLOW}===== 环境设置 =====${NC}"
    rm -f "$DISK_FILE"
    run_test "创建 10MB 磁盘" "Disk created successfully" "$EXECUTABLE $DISK_FILE create 10"
    run_test "格式化磁盘" "Disk formatted successfully" "$EXECUTABLE $DISK_FILE format"
    echo "环境设置完成。"
}

teardown() {
    echo -e "\n${YELLOW}===== 环境清理 =====${NC}"
    make clean
    echo "清理完成。"
}

test_root_listing() {
    echo -e "\n${YELLOW}===== 测试空根目录列出 =====${NC}"
    run_test "格式化后立即执行 'ls'" "./" "$EXECUTABLE $DISK_FILE ls"
    run_test "格式化后立即执行 'ls /'" "./" "$EXECUTABLE $DISK_FILE ls /"
}

test_basic_ops() {
    echo -e "\n${YELLOW}===== 测试基本操作 (mkdir, touch, echo, ls, cat) =====${NC}"
    run_test "创建目录 '/test'" "Directory created" "$EXECUTABLE $DISK_FILE mkdir /test"
    run_test "创建文件 '/test/file.txt'" "File created" "$EXECUTABLE $DISK_FILE touch /test/file.txt"
    run_test "向文件写入 'hello'" "Written to file" "$EXECUTABLE $DISK_FILE echo 'hello' '>' /test/file.txt"
    run_test "列出 '/test' 目录内容" "file.txt" "$EXECUTABLE $DISK_FILE ls /test"
    run_test "读取文件内容" "hello" "$EXECUTABLE $DISK_FILE cat /test/file.txt"
}

test_deletion() {
    echo -e "\n${YELLOW}===== 测试删除操作 (rm) =====${NC}"
    run_test "创建用于删除的文件 '/deleteme.txt'" "File created" "$EXECUTABLE $DISK_FILE touch /deleteme.txt"
    run_test "删除文件 '/deleteme.txt'" "Removed" "$EXECUTABLE $DISK_FILE rm /deleteme.txt"

    ((TEST_COUNT++))
    local description="验证文件已被删除"
    local padded_desc=$(printf '  - %-55s' "$description")
    local dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"
    output=$($EXECUTABLE $DISK_FILE ls /)
    if [[ "$output" != *"deleteme.txt"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $EXECUTABLE $DISK_FILE ls /"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    └─ 删除检查失败. '/deleteme.txt' 仍然存在于 ls 输出中: \n$output"
        ((FAIL_COUNT++))
    fi

    run_test "创建用于删除的目录 '/deleteme_dir'" "Directory created" "$EXECUTABLE $DISK_FILE mkdir /deleteme_dir"
    run_test "删除空目录" "Removed" "$EXECUTABLE $DISK_FILE rm /deleteme_dir"

    ((TEST_COUNT++))
    description="验证目录已被删除"
    padded_desc=$(printf '  - %-55s' "$description")
    dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"
    output=$($EXECUTABLE $DISK_FILE ls /)
    if [[ "$output" != *"deleteme_dir"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $EXECUTABLE $DISK_FILE ls /"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    └─ 删除检查失败. '/deleteme_dir' 仍然存在于 ls 输出中: \n$output"
        ((FAIL_COUNT++))
    fi
}

test_info_command() {
    echo -e "\n${YELLOW}===== 测试 'info' 命令 =====${NC}"
    run_test "获取磁盘信息" "Disk Information" "$EXECUTABLE $DISK_FILE info"
    run_test "信息包含 'Total Blocks'" "Total Blocks" "$EXECUTABLE $DISK_FILE info"
    run_test "信息包含 'Total Inodes'" "Total Inodes" "$EXECUTABLE $DISK_FILE info"
}

test_error_conditions() {
    echo -e "\n${YELLOW}===== 测试异常处理 =====${NC}"
    run_fail_test "读取不存在的文件" "File not found" "$EXECUTABLE $DISK_FILE cat /no_such_file"
    run_fail_test "创建已存在的目录" "already exists" "$EXECUTABLE $DISK_FILE mkdir /test"
    run_fail_test "重复创建已存在的文件" "File already exists" "$EXECUTABLE $DISK_FILE touch /test/file.txt"
    run_fail_test "创建带不存在父目录的目录" "Parent directory not found" "$EXECUTABLE $DISK_FILE mkdir /missing/child"
    run_fail_test "在不存在的目录中创建文件" "Parent directory not found" "$EXECUTABLE $DISK_FILE touch /missing/file.txt"
    run_test "为非空目录检查创建文件" "File created" "$EXECUTABLE $DISK_FILE touch /test/another.txt"
    run_fail_test "删除非空目录" "Directory not empty" "$EXECUTABLE $DISK_FILE rm /test"
    run_fail_test "删除不存在的路径" "File not found" "$EXECUTABLE $DISK_FILE rm /no_such_path"
    run_fail_test "尝试删除根目录" "Cannot remove root directory" "$EXECUTABLE $DISK_FILE rm /"
}

test_reformat_validation() {
    echo -e "\n${YELLOW}===== 测试重新格式化行为 =====${NC}"
    run_test "重新格式化磁盘" "Disk formatted successfully" "$EXECUTABLE $DISK_FILE format"
    run_test "重新格式化后执行 'ls'" "./" "$EXECUTABLE $DISK_FILE ls"
    run_test "重新格式化后执行 'ls /'" "./" "$EXECUTABLE $DISK_FILE ls /"
}

# ------------------------------------------------------------------------------
# 主执行函数
# ------------------------------------------------------------------------------

main() {
    echo "正在编译项目..."
    make clean > /dev/null && make > /dev/null
    if [ $? -ne 0 ]; then
        echo "编译失败，正在中止测试。"
        make # Re-run make to show error output
        exit 1
    fi

    setup
    test_root_listing
    test_basic_ops
    test_deletion
    test_info_command
    test_error_conditions
    test_reformat_validation
    teardown

    echo -e "\n${YELLOW}===== 测试总结 =====${NC}"
    echo "总测试数: $TEST_COUNT"
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "${GREEN}所有测试通过！${NC}"
        exit 0
    else
        echo -e "${RED}$FAIL_COUNT 个测试失败。${NC}"
        exit 1
    fi
}

main
#!/bin/bash

# ==============================================================================
# 用户态文件系统综合测试脚本
#
# 目的: 验证项目是否满足 requirements.md 中的所有功能需求。
# 特性:
#   - 清晰的模块化测试功能。
#   - 对每个命令的输出进行严格验证。
#   - 使用彩色标记的 PASS/FAIL 状态消息。
#   - 包含创建、删除和错误处理的测试。
# ==============================================================================

# ------------------------------------------------------------------------------
# 配置与辅助函数
# ------------------------------------------------------------------------------
DISK_FILE="test_disk.img"
EXECUTABLE="./disk_sim"
TEST_COUNT=0
FAIL_COUNT=0

# 用于输出的颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # 无颜色

# 辅助函数：运行一个测试并验证其输出
# 用法: run_test "测试描述" "预期输出" "要执行的完整命令"
run_test() {
    ((TEST_COUNT++))
    local description="$1"
    local expected_output="$2"
    local command="$3"

    local padded_desc=$(printf '  - %-55s' "$description")
    local dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"

    output=$(eval "$command" 2>&1)
    local exit_code=$?

    if [ $exit_code -eq 0 ] && [[ "$output" == *"$expected_output"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $command"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    ├─ 命令: $command"
        echo -e "    ├─ 退出码: $exit_code (预期为 0)"
        echo -e "    ├─ 输出: \n$output"
        echo -e "    └─ 预期包含: $expected_output"
        run_fail_test "重复创建已存在的文件" "File already exists" "$EXECUTABLE $DISK_FILE touch /test/file.txt"
        run_fail_test "创建带不存在父目录的目录" "Parent directory not found" "$EXECUTABLE $DISK_FILE mkdir /missing/child"
        run_fail_test "在不存在的目录中创建文件" "Parent directory not found" "$EXECUTABLE $DISK_FILE touch /missing/file.txt"
        ((FAIL_COUNT++))
    fi
        run_fail_test "删除不存在的路径" "File not found" "$EXECUTABLE $DISK_FILE rm /no_such_path"
        run_fail_test "尝试删除根目录" "Cannot remove root directory" "$EXECUTABLE $DISK_FILE rm /"
}

    test_reformat_validation() {
        echo -e "\n${YELLOW}===== 测试重新格式化行为 =====${NC}"
        run_test "重新格式化磁盘" "Disk formatted successfully" "$EXECUTABLE $DISK_FILE format"
        run_test "重新格式化后执行 'ls'" "./" "$EXECUTABLE $DISK_FILE ls"
        run_test "重新格式化后执行 'ls /'" "./" "$EXECUTABLE $DISK_FILE ls /"
    }
# 辅助函数：测试预期会失败的命令
# 用法: run_fail_test "测试描述" "预期错误" "要执行的完整命令"
run_fail_test() {
    ((TEST_COUNT++))
    local description="$1"
    local expected_error="$2"
    local command="$3"

    local padded_desc=$(printf '  - %-55s' "$description")
    local dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"

    output=$(eval "$command" 2>&1)
    local exit_code=$?

    if [ $exit_code -ne 0 ] && [[ "$output" == *"$expected_error"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $command"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    ├─ 命令: $command"
        echo -e "    ├─ 退出码: $exit_code (预期为非 0)"
        echo -e "    ├─ 输出: \n$output"
        echo -e "    └─ 预期包含错误: $expected_error"
        ((FAIL_COUNT++))
    fi
}

# ------------------------------------------------------------------------------
# 测试阶段
# ------------------------------------------------------------------------------

setup() {
    echo -e "\n${YELLOW}===== 环境设置 =====${NC}"
    rm -f "$DISK_FILE"
    run_test "创建 10MB 磁盘" "Disk created successfully" "$EXECUTABLE $DISK_FILE create 10"
    run_test "格式化磁盘" "Disk formatted successfully" "$EXECUTABLE $DISK_FILE format"
    echo "环境设置完成。"
}

teardown() {
    echo -e "\n${YELLOW}===== 环境清理 =====${NC}"
    make clean
    echo "清理完成。"
}

test_basic_ops() {
    echo -e "\n${YELLOW}===== 测试空根目录列出 =====${NC}"
    run_test "格式化后立即执行 'ls'" "./" "$EXECUTABLE $DISK_FILE ls"
    run_test "格式化后立即执行 'ls /'" "./" "$EXECUTABLE $DISK_FILE ls /"
    echo -e "\n${YELLOW}===== 测试基本操作 (mkdir, touch, echo, ls, cat) =====${NC}"
    run_test "创建目录 '/test'" "Directory created" "$EXECUTABLE $DISK_FILE mkdir /test"
    run_test "创建文件 '/test/file.txt'" "File created" "$EXECUTABLE $DISK_FILE touch /test/file.txt"
    run_test "向文件写入 'hello'" "Written to file" "$EXECUTABLE $DISK_FILE echo 'hello' '>' /test/file.txt"
    run_test "列出 '/test' 目录内容" "file.txt" "$EXECUTABLE $DISK_FILE ls /test"
    run_test "读取文件内容" "hello" "$EXECUTABLE $DISK_FILE cat /test/file.txt"
}

test_deletion() {
    echo -e "\n${YELLOW}===== 测试删除操作 (rm) =====${NC}"
    run_test "创建用于删除的文件 '/deleteme.txt'" "File created" "$EXECUTABLE $DISK_FILE touch /deleteme.txt"
    run_test "删除文件 '/deleteme.txt'" "Removed" "$EXECUTABLE $DISK_FILE rm /deleteme.txt"
    
    ((TEST_COUNT++))
    local description="验证文件已被删除"
    local padded_desc=$(printf '  - %-55s' "$description")
    local dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"
    output=$($EXECUTABLE $DISK_FILE ls /)
    if [[ "$output" != *"deleteme.txt"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $EXECUTABLE $DISK_FILE ls /"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    └─ 删除检查失败. '/deleteme.txt' 仍然存在于 ls 输出中: \n$output"
        ((FAIL_COUNT++))
    fi

    run_test "创建用于删除的目录 '/deleteme_dir'" "Directory created" "$EXECUTABLE $DISK_FILE mkdir /deleteme_dir"
    run_test "删除空目录" "Removed" "$EXECUTABLE $DISK_FILE rm /deleteme_dir"

    ((TEST_COUNT++))
    local description="验证目录已被删除"
    padded_desc=$(printf '  - %-55s' "$description")
    dotted_line=${padded_desc// /.}
    printf "%s" "$dotted_line"
    output=$($EXECUTABLE $DISK_FILE ls /)
    if [[ "$output" != *"deleteme_dir"* ]]; then
        printf " [%b通过%b]\n" "$GREEN" "$NC"
        echo -e "    └─ 命令: $EXECUTABLE $DISK_FILE ls /"
    else
        printf " [%b失败%b]\n" "$RED" "$NC"
        echo -e "    └─ 删除检查失败. '/deleteme_dir' 仍然存在于 ls 输出中: \n$output"
        ((FAIL_COUNT++))
    fi
}

test_info_command() {
    echo -e "\n${YELLOW}===== 测试 'info' 命令 =====${NC}"
    run_test "获取磁盘信息" "Disk Information" "$EXECUTABLE $DISK_FILE info"
    run_test "信息包含 'Total Blocks'" "Total Blocks" "$EXECUTABLE $DISK_FILE info"
    run_test "信息包含 'Total Inodes'" "Total Inodes" "$EXECUTABLE $DISK_FILE info"
}

test_error_conditions() {
    echo -e "\n${YELLOW}===== 测试异常处理 =====${NC}"
    run_fail_test "读取不存在的文件" "File not found" "$EXECUTABLE $DISK_FILE cat /no_such_file"
    run_fail_test "创建已存在的目录" "already exists" "$EXECUTABLE $DISK_FILE mkdir /test"
    run_test "为非空目录检查创建文件" "File created" "$EXECUTABLE $DISK_FILE touch /test/another.txt"
    run_fail_test "删除非空目录" "Directory not empty" "$EXECUTABLE $DISK_FILE rm /test"
}

# ------------------------------------------------------------------------------
# 主执行函数
# ------------------------------------------------------------------------------
main() {
    echo "正在编译项目..."
    make clean > /dev/null && make > /dev/null
    if [ $? -ne 0 ]; then
        echo "编译失败，正在中止测试。"
        make # Re-run make to show error output
        exit 1
    fi

    setup
    test_root_listing
    test_basic_ops
    test_deletion
    test_info_command
    test_error_conditions
    test_reformat_validation
    teardown

    echo -e "\n${YELLOW}===== 测试总结 =====${NC}"
    echo "总测试数: $TEST_COUNT"
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "${GREEN}所有测试通过！${NC}"
        exit 0
    else
        echo -e "${RED}$FAIL_COUNT 个测试失败。${NC}"
        exit 1
    fi
}

main
