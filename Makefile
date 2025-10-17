# ------------------------------------------------------------------------------
# 编译器及编译选项
# ------------------------------------------------------------------------------
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -pthread
INCLUDES = -I./src

# ------------------------------------------------------------------------------
# 目录与目标定义
# ------------------------------------------------------------------------------
SRCDIR = src
OBJDIR = obj
TESTDIR = tests
TARGET = disk_sim

# 压测配置（可通过环境变量覆盖）
STRESS_DISK ?= stress_test.img
STRESS_DISK_SIZE ?= 512
STRESS_DURATION ?= 60
STRESS_FILES ?= 64
STRESS_THREADS ?= 8
STRESS_WRITE_SIZE ?= 4096
STRESS_MONITOR ?= 30
STRESS_WORKSPACE ?= /stress_auto

# ------------------------------------------------------------------------------
# 源文件与目标文件
# ------------------------------------------------------------------------------
# 自动查找所有 .cpp 源文件
CORE_SOURCES = $(wildcard $(SRCDIR)/core/*.cpp)
CLI_SOURCES = $(wildcard $(SRCDIR)/cli/*.cpp)
UTILS_SOURCES = $(wildcard $(SRCDIR)/utils/*.cpp)
THREADING_SOURCES = $(wildcard $(SRCDIR)/threading/*.cpp)
APP_SOURCE = $(SRCDIR)/app.cpp
MAIN_SOURCE = $(SRCDIR)/main.cpp

# 根据源文件生成对应的 .o 目标文件路径
CORE_OBJECTS = $(CORE_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
CLI_OBJECTS = $(CLI_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
UTILS_OBJECTS = $(UTILS_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
THREADING_OBJECTS = $(THREADING_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
APP_OBJECT = $(APP_SOURCE:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# 汇总所有目标文件
ALL_OBJECTS = $(CORE_OBJECTS) $(CLI_OBJECTS) $(UTILS_OBJECTS) $(THREADING_OBJECTS) $(APP_OBJECT) $(MAIN_OBJECT)

# ==============================================================================
# 主要构建目标
# ==============================================================================

# 默认目标：构建主程序
all: $(TARGET)

# 链接所有目标文件生成最终可执行文件
$(TARGET): $(ALL_OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

# 通用编译规则：将 .cpp 文件编译为 .o 文件
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 创建存放目标文件的目录
$(OBJDIR):
	@mkdir -p $(OBJDIR)/core $(OBJDIR)/cli $(OBJDIR)/utils $(OBJDIR)/threading

# ==============================================================================
# 测试目标
# ==============================================================================

test-functionality: $(TARGET)
	@chmod +x ./tests/test_functionality.sh
	./tests/test_functionality.sh

test-multithreaded: $(TARGET)
	@chmod +x ./tests/test_multithreaded.sh
	./tests/test_multithreaded.sh

test-thread-safety: $(TARGET)
	@chmod +x ./tests/test_thread_safety.sh
	./tests/test_thread_safety.sh

stress-test: $(TARGET)
	@BIN=./$(TARGET) \
		DISK=$(STRESS_DISK) \
		DISK_SIZE=$(STRESS_DISK_SIZE) \
		DURATION=$(STRESS_DURATION) \
		FILES=$(STRESS_FILES) \
		THREADS=$(STRESS_THREADS) \
		WRITE_SIZE=$(STRESS_WRITE_SIZE) \
		MONITOR=$(STRESS_MONITOR) \
		WORKSPACE=$(STRESS_WORKSPACE) \
		bash ./tests/run_stress_test.sh

# ==============================================================================
# 清理所有构建产物和测试文件
# ==============================================================================

clean:
	rm -rf $(OBJDIR) $(TARGET) test_disk.img disk.img release test*.img

# ==============================================================================
# 帮助与其他
# ==============================================================================

# 显示帮助信息
help:
	@echo "Available targets:"
	@echo "  all                - (默认) 构建主程序"
	@echo "  test-functionality - 运行完整功能测试脚本"
	@echo "  test-multithreaded - 运行多线程功能测试脚本"
	@echo "  test-thread-safety - 运行线程安全性专项测试"
	@echo "  clean              - 清理构建产物"
	@echo "  help               - 显示此帮助信息"

# 声明伪目标，这些目标不代表实际文件
.PHONY: all test-functionality test-multithreaded test-thread-safety stress-test clean help