#Makefile
# Development environment: Windows10 + MinGW64(v8.1.0)
SHELL = cmd.exe

GCC = D:\\Dev\\gcc\\64bit\\bin\\gcc.exe
BUILD_DIR = build
INC_PATH = . libs
ARGS = -lws2_32

SRS_FILES = $(foreach dir,$(INC_PATH),$(wildcard $(dir)/*.c))
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(SRS_FILES:.c=.o)))
TARGET = $(BUILD_DIR)/application.exe

vpath %.c $(INC_PATH)

all: build_dir $(TARGET)

build_dir:
	if not exist "$(BUILD_DIR)" mkdir $(BUILD_DIR)

$(TARGET): $(OBJECTS)
# 	$(GCC) -o $(TARGET) $(OBJECTS) $(ARGS)
	$(GCC) -mwindows -o $(TARGET) $(OBJECTS) $(ARGS)

$(BUILD_DIR)/%.o: %.c
	$(GCC) -MMD -c $< $(addprefix -I,$(INC_PATH)) -o $@

clean:
	if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	if exist release rmdir /s /q release

release: clean all
	if not exist release mkdir release
	copy $(TARGET) release\$(notdir $(TARGET))
	xcopy web_root release\web_root /E /I /H /Y
	xcopy bin release\bin /E /I /H /Y

.PHONY: all clean release