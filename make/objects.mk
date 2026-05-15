APP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ALL_SRCS))
NON_APP_OBJS := $(filter-out $(OBJ_DIR)/src/main.o,$(APP_OBJS))
TEST_OBJS := $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS))

SHARED_THEME_FONT_ADAPTER_TEST_BIN := $(BUILD_DIR)/tests/shared_theme_font_adapter_test
INPUT_POLICY_TEST_BIN := $(BUILD_DIR)/tests/input_policy_test

SHAPE_TOOL_OBJS := $(patsubst $(TOOLS_DIR)/%.c,$(BUILD_DIR)/tools/%.o,$(SHAPE_TOOL_SRCS))
SHAPE_TOOL_SHARED_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SHAPE_TOOL_SUPPORT_SRCS))
