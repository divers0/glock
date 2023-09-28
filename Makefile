COMPILER      := cc
TARGET_DIR    := target
TARGET_NAME   := glock
TARGET_PATH   := $(TARGET_DIR)/$(TARGET_NAME)

CONFIG_DIR         := ${HOME}/.config/glock
ALARM_FILE_NAME    := alarm.mp3
ALARM_FILE_PATH    := $(CONFIG_DIR)/$(ALARM_FILE_NAME)
SETTINGS_FILE_PATH := $(CONFIG_DIR)/settings

CFLAGS := -Wall -Werror -Wextra -Wpedantic
LIBS   := -lraylib -lm

SOURCE := glock.c


all: compile

setup_config_dir:
	@if ! [ -d $(CONFIG_DIR) ]; then\
		mkdir -p $(CONFIG_DIR);\
    fi
	@if ! [ -f $(ALARM_FILE_PATH) ]; then\
		cp extra/$(ALARM_FILE_NAME) $(CONFIG_DIR);\
    fi
	@if ! [ -f $(SETTINGS_FILE_PATH) ]; then\
		echo "$(ALARM_FILE_PATH)" > $(SETTINGS_FILE_PATH);\
    fi

compile: setup_config_dir
	mkdir -p $(TARGET_DIR)
	$(COMPILER) $(SOURCE) $(LIBS) $(CFLAGS) -o $(TARGET_PATH)

run: compile
	$(TARGET_DIR)/$(TARGET_NAME)
