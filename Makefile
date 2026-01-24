
TARGET := mcalc

BUILD_DIR := build
SRC_DIRS := src

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

PROJ_INCLUDE_DIRS := src

EXT_INCLUDE_DIRS := 
EXT_LIBS := 

INCLUDE_DIRS := $(PROJ_INCLUDE_DIRS) $(EXT_INCLUDE_DIRS)
INCLUDE_CFLAGS := $(addprefix -I,$(INCLUDE_DIRS)) $(shell sdl2-config --cflags)

LIB_LDFLAGS := $(addprefix -l,$(EXT_LIBS)) $(shell sdl2-config --libs)

CFLAGS := $(INCLUDE_CFLAGS) -MMD -MP -g -Wall -Wextra -Werror -std=c17
CPPFLAGS := $(INCLUDE_CFLAGS) -MMD -MP -g -Wall -Wextra -Werror -std=c++17
LDFLAGS := $(LIB_LDFLAGS)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c Makefile
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp Makefile
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -c $< -o $@


.PHONY: clean

clean:
	rm -r $(BUILD_DIR)


-include $(DEPS)


