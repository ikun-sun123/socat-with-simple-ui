# Makefile
BUILD_DIR = build
CC = D:/Dev/gcc/64bit/bin/gcc
LDFLAGS = -lws2_32

# 源文件目录列表
SRS_DIRS = . libs

# 设置源文件搜索路径
vpath %.c $(SRS_DIRS)

# 自动扫描所有目录中的 .c 文件
SRS = $(foreach dir,$(SRS_DIRS),$(wildcard $(dir)/*.c))
SRS := $(filter-out %api.c,$(SRS))  # 过滤掉所有路径下的 api.c

TARGET = main.exe
OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SRS:.c=.o)))
DLL_TARGET = api.dll

all: build_dir $(TARGET) $(DLL_TARGET)

build_dir:
	if not exist "$(BUILD_DIR)" mkdir $(BUILD_DIR)

# 编译 DLL
$(DLL_TARGET): api.c
	$(CC) -shared -o $@ $< libs/cJSON.c libs/linkedlist.h -I. -Ilibs -Wall -DBUILD_DLL

# 通用编译规则
$(BUILD_DIR)/%.o: %.c
	$(CC) $(addprefix -I,$(SRS_DIRS)) -MMD -c $< -o $@

# 链接主程序
$(TARGET): $(OBJS)
	$(CC) -mwindows -o $@ $(OBJS) $(LDFLAGS)
# 	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rmdir /s /q $(BUILD_DIR)

.PHONY: all clean