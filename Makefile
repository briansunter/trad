PROJECT := trad
BUILD_DIR := build
WEB_DIR := dist
CECS_DIR ?= ../c-ecs
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
SRC := src/main.c $(GEN_C)

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

.PHONY: all native web run serve clean assets

all: native web

assets: $(GEN_H) $(GEN_C)

$(GEN_H) $(GEN_C): tools/obj_to_c.py
	$(PYTHON) tools/obj_to_c.py \
		--src-dir "$(ASSET_OBJ_DIR)" \
		--out-h "$(GEN_H)" \
		--out-c "$(GEN_C)" \
		$(MESH_MODELS)

native: $(BUILD_DIR)/$(PROJECT)

$(BUILD_DIR)/$(PROJECT): $(SRC) $(GEN_H)
	mkdir -p $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) $(NATIVE_CFLAGS) $(NATIVE_BACKEND_CFLAGS) $(SRC) -o $@ $(NATIVE_LDLIBS) -lm

web: $(WEB_DIR)/index.html

$(WEB_DIR)/index.html: $(SRC) $(GEN_H) web/shell.html
	mkdir -p $(WEB_DIR)
	$(EMCC) $(COMMON_CFLAGS) $(WEB_CFLAGS) $(SRC) -o $@ \
		--shell-file web/shell.html \
		-sUSE_WEBGL2=1 -sFULL_ES3=1 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sNO_FILESYSTEM=1
	$(PYTHON) -c "from pathlib import Path; p=Path('$@'); s=p.read_text(); s=s.replace('src=\"index.js\"', 'src=\"index.js?v=controls11\"'); p.write_text(s)"

run: native
	$(BUILD_DIR)/$(PROJECT)

serve: web
	$(PYTHON) tools/serve_no_cache.py --dir $(WEB_DIR) --port 8000

clean:
	rm -rf $(BUILD_DIR) $(WEB_DIR) $(GEN_H) $(GEN_C)
