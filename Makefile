PROJECT := trad
BUILD_DIR := build
WEB_DIR := dist
CECS_DIR ?= vendor/c-ecs
SOKOL_DIR := vendor/sokol
ASSET_OBJ_DIR := assets/vendor/kenney-space-kit/Models/OBJ format

MESH_MODELS := \
	craft_racer craft_speederA craft_speederB craft_speederC craft_speederD \
	craft_cargoA craft_cargoB craft_miner \
	meteor_detailed meteor meteor_half rock_largeA rocks_smallA craterLarge \
	terrain_roadStraight terrain_roadCorner terrain_roadCross \
	platform_long platform_large turret_single satelliteDish_detailed \
	hangar_smallA gate_complex machine_generatorLarge

GEN_H := src/generated_assets.h
GEN_C := src/generated_assets.c
GAME_H := src/game.h
GAME_C := src/game.c
SRC := src/main.c $(GAME_C) $(GEN_C)

CC ?= cc
EMCC ?= emcc
PYTHON ?= python3

WARNINGS := -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter
INCLUDES := -Isrc -I$(SOKOL_DIR) -I$(CECS_DIR)
COMMON_CFLAGS := -std=c99 -O2 $(WARNINGS) $(INCLUDES)
NATIVE_CFLAGS ?=
WEB_CFLAGS ?=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	NATIVE_BACKEND_CFLAGS := -x objective-c
	NATIVE_LDLIBS := -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
else
	NATIVE_BACKEND_CFLAGS :=
	NATIVE_LDLIBS := -lX11 -lXi -lXcursor -lGL -ldl -lpthread
endif

TEST_DIR := tests
TEST_SRC := $(TEST_DIR)/test_main.c $(GAME_C) $(GEN_C)
TEST_BIN := $(BUILD_DIR)/test_main
COVERAGE_DIR := $(BUILD_DIR)/coverage

.PHONY: all native web run serve clean assets test coverage

all: native web

assets: $(GEN_H) $(GEN_C)

$(GEN_H) $(GEN_C): tools/obj_to_c.py
	$(PYTHON) tools/obj_to_c.py \
		--src-dir "$(ASSET_OBJ_DIR)" \
		--out-h "$(GEN_H)" \
		--out-c "$(GEN_C)" \
		$(MESH_MODELS)

native: $(BUILD_DIR)/$(PROJECT)

$(BUILD_DIR)/$(PROJECT): $(SRC) $(GEN_H) $(GAME_H)
	mkdir -p $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) $(NATIVE_CFLAGS) $(NATIVE_BACKEND_CFLAGS) $(SRC) -o $@ $(NATIVE_LDLIBS) -lm

web: $(WEB_DIR)/index.html

$(WEB_DIR)/index.html: $(SRC) $(GEN_H) $(GAME_H) web/shell.html
	mkdir -p $(WEB_DIR)
	$(EMCC) $(COMMON_CFLAGS) $(WEB_CFLAGS) $(SRC) -o $@ \
		--shell-file web/shell.html \
		-sUSE_WEBGL2=1 -sFULL_ES3=1 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sNO_FILESYSTEM=1
	$(PYTHON) -c "from pathlib import Path; p=Path('$@'); s=p.read_text(); s=s.replace('src=\"index.js\"', 'src=\"index.js?v=drag5\"'); p.write_text(s)"

run: native
	$(BUILD_DIR)/$(PROJECT)

serve: web
	$(PYTHON) tools/serve_no_cache.py --dir $(WEB_DIR) --port 8000

test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(GEN_H) $(GAME_H)
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c99 -O0 -g $(WARNINGS) -Isrc -I$(CECS_DIR) $(TEST_SRC) -o $@ -lm

COVERAGE_CFLAGS := -std=c99 -O0 -g $(WARNINGS) -Isrc -I$(CECS_DIR) --coverage
COVERAGE_OBJS := $(COVERAGE_DIR)/game.o $(COVERAGE_DIR)/generated_assets.o $(COVERAGE_DIR)/test_main.o

$(COVERAGE_DIR)/game.o: $(GAME_C) $(GAME_H) $(GEN_H)
	mkdir -p $(COVERAGE_DIR)
	$(CC) $(COVERAGE_CFLAGS) -c $(GAME_C) -o $@

$(COVERAGE_DIR)/generated_assets.o: $(GEN_C) $(GEN_H)
	mkdir -p $(COVERAGE_DIR)
	$(CC) $(COVERAGE_CFLAGS) -c $(GEN_C) -o $@

$(COVERAGE_DIR)/test_main.o: $(TEST_DIR)/test_main.c $(GAME_H) $(GEN_H)
	mkdir -p $(COVERAGE_DIR)
	$(CC) $(COVERAGE_CFLAGS) -c $(TEST_DIR)/test_main.c -o $@

$(COVERAGE_DIR)/test_main: $(COVERAGE_OBJS)
	$(CC) --coverage $(COVERAGE_OBJS) -o $@ -lm

coverage: $(COVERAGE_DIR)/test_main
	cd $(COVERAGE_DIR) && ./test_main
	gcov -o $(COVERAGE_DIR) $(GAME_C) > $(COVERAGE_DIR)/gcov_report.txt
	mv game.c.gcov $(COVERAGE_DIR)/ 2>/dev/null || true
	@echo ""
	@echo "===== Coverage summary for game.c ====="
	@grep -A 1 "game\.c" $(COVERAGE_DIR)/gcov_report.txt || true

clean:
	rm -rf $(BUILD_DIR) $(WEB_DIR) $(GEN_H) $(GEN_C)
