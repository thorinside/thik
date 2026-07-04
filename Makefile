PLUGIN_NAME = thik_osc
SOURCES = thik_osc.cpp
LEVEL_TEST = build/rms_consistency

UNAME_S := $(shell uname -s)
TARGET ?= hardware
API_DIR ?= distingNT_API
HOST_CXX ?= c++
HOST_CXXFLAGS = -std=c++11 -O3 -ffast-math -Wall -Wextra

ifeq ($(TARGET),hardware)
    CXX = arm-none-eabi-c++
    NM = arm-none-eabi-nm
    CFLAGS = -std=c++11 \
             -mcpu=cortex-m7 \
             -mfpu=fpv5-d16 \
             -mfloat-abi=hard \
             -mthumb \
             -Os \
             -fPIC \
             -fno-rtti \
             -fno-exceptions \
             -Wall
    INCLUDES = -I. -I$(API_DIR)/include
    OUTPUT_DIR = plugins
    OUTPUT = $(OUTPUT_DIR)/$(PLUGIN_NAME).o
    SIZE_CMD = arm-none-eabi-size -A $(OUTPUT)

else ifeq ($(TARGET),test)
    NM = nm

    ifeq ($(UNAME_S),Darwin)
        CXX = clang++
        CFLAGS = -std=c++11 -fPIC -O3 -ffast-math -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -dynamiclib -undefined dynamic_lookup
        EXT = dylib
    endif

    ifeq ($(UNAME_S),Linux)
        CXX = g++
        CFLAGS = -std=c++11 -fPIC -O3 -ffast-math -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -shared
        EXT = so
    endif

    ifeq ($(OS),Windows_NT)
        CXX = g++
        CFLAGS = -std=c++11 -fPIC -O3 -ffast-math -Wall -fno-rtti -fno-exceptions
        LDFLAGS = -shared
        EXT = dll
    endif

    INCLUDES = -I. -I$(API_DIR)/include
    OUTPUT_DIR = plugins
    OUTPUT = $(OUTPUT_DIR)/$(PLUGIN_NAME).$(EXT)
    SIZE_CMD = ls -lh $(OUTPUT)
endif

all: $(OUTPUT)

ifeq ($(TARGET),hardware)
$(OUTPUT): $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
	@echo "Built: $@"
	@$(SIZE_CMD)

else ifeq ($(TARGET),test)
$(OUTPUT): $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@ $(SOURCES)
	@echo "Built: $@"
endif

hardware:
	@$(MAKE) TARGET=hardware

test:
	@$(MAKE) TARGET=test

both: hardware test

check: $(OUTPUT)
	@undefs="$$($(NM) $(OUTPUT) | awk '$$1 == "U" && $$2 != "NT_globals" && $$2 != "_NT_globals" && $$2 != "_GLOBAL_OFFSET_TABLE_" && $$2 != "sinf" && $$2 != "cosf" && $$2 != "sqrtf" { print }')"; \
	if [ -n "$$undefs" ]; then \
		echo "Unexpected undefined symbols:"; \
		echo "$$undefs"; \
		exit 1; \
	fi; \
	$(NM) $(OUTPUT) | grep ' U ' || true

size: $(OUTPUT)
	@$(SIZE_CMD)

$(LEVEL_TEST): tests/rms_consistency.cpp $(SOURCES)
	@mkdir -p build
	$(HOST_CXX) $(HOST_CXXFLAGS) $(INCLUDES) -o $@ $<

level-test: $(LEVEL_TEST)
	@$(LEVEL_TEST)

verify:
	@$(MAKE) clean
	@$(MAKE) both
	@$(MAKE) TARGET=hardware check
	@$(MAKE) TARGET=test check
	@$(MAKE) TARGET=hardware size
	@$(MAKE) TARGET=test size
	@$(MAKE) level-test

clean:
	rm -rf build $(OUTPUT_DIR)

.PHONY: all hardware test both check size level-test verify clean
