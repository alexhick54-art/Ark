# Ark build.
#   make gen EXEFS=path/to/exefs     translate the game's code to C (needs Unicorn's Python bindings; tools/setup_unicorn.sh)
#   make -j objs                     compile everything under generated/ into build/obj/
#   make -j ark                      runtime + game objects -> build/ark (Linux; Vulkan renderer, SDL3 window)
#        HEADLESS=1                  ... without window or renderer (NVN calls are logged and return 0)
#        DEPS=<prefix>               where SDL3 / Vulkan / shaderc / VMA are, if not system-wide
#                                    (tools/deps/build_linux_deps.sh builds them)
#   make mkromfs                     build/mkromfs: RomFS image from a romfs folder
#   make test-lift EXEFS=...         differential test of the translator against Unicorn
EXEFS    ?= exefs
GEN      ?= generated
B        ?= build
# make predefines CC=cc; the generated code needs clang (musttail), so default to it unless CC is given.
ifeq ($(origin CC),default)
CC       := clang
endif
ifeq ($(origin CXX),default)
CXX      := clang++
endif
ARCH     := $(shell uname -m)
ifeq ($(filter arm64 aarch64,$(ARCH)),)
SIMD     := c
MFLAGS   := -mcx16 -mavx2
else
SIMD     := asm
MFLAGS   :=
endif
GENFLAGS := -O1 $(MFLAGS) -fno-strict-aliasing -Wno-return-type -Wno-unused-label -fvisibility=hidden -Iruntime/include

GEN_SRC  := $(wildcard $(GEN)/*/*.c)
GEN_OBJ  := $(patsubst $(GEN)/%.c,$(B)/obj/%.o,$(GEN_SRC))

RT_SRC   := $(wildcard runtime/*.c) runtime/platform/linux.c $(wildcard runtime/services/*.c) $(wildcard runtime/hle/*.c)
HEADLESS ?= 0
ifeq ($(HEADLESS),1)
# no renderer, no SDL: the NVN layer, input and the menu hooks stay out
RT_SRC   := $(filter-out runtime/ui_none.c runtime/hle/input.c runtime/hle/bindings.c,$(RT_SRC))
RTDEFS   := -DARK_HEADLESS -Iruntime/headless
LDLIBS   := -lpthread -lm -ldl
else
RT_SRC   += $(wildcard runtime/nvn/*.c) $(wildcard runtime/nvn/maxwell/*.c) $(wildcard runtime/nvn/vulkan/*.c)
RT_CXX   := $(wildcard runtime/nvn/vulkan/*.cpp)
RTDEFS   := $(if $(DEPS),-I$(DEPS)/include) -Iruntime/nvn -I$(B)/inc -Ithird_party/smaa/Textures
LDLIBS   := $(if $(DEPS),-L$(DEPS)/lib -Wl,-rpath,$(DEPS)/lib) -lSDL3 -lvulkan -lshaderc_shared -lstdc++ -lpthread -lm -ldl
endif
RT_OBJ   := $(patsubst runtime/%.c,$(B)/rt/%.o,$(RT_SRC)) $(patsubst runtime/%.cpp,$(B)/rt/%.o,$(RT_CXX))
RTFLAGS  := -O1 -g -std=gnu11 $(MFLAGS) -fno-strict-aliasing -Iruntime/include -Iruntime $(RTDEFS) \
            -Wall -Wno-unused-function -Wno-format
RT_HDRS  := $(wildcard runtime/*.h runtime/nvn/*.h runtime/nvn/*/*.h) runtime/include/cpu.h

.PHONY: gen objs ark mkromfs test-lift clean
gen:
	python3 tools/gen_all.py $(EXEFS) $(GEN) --simd $(SIMD)

objs: $(GEN_OBJ)
	@echo "compiled $(words $(GEN_OBJ)) files"

$(B)/obj/%.o: $(GEN)/%.c runtime/include/cpu.h
	@mkdir -p $(dir $@)
	@$(CC) $(GENFLAGS) -c $< -o $@

$(B)/rt/%.o: runtime/%.c $(RT_HDRS)
	@mkdir -p $(dir $@)
	$(CC) $(RTFLAGS) -c $< -o $@

$(B)/rt/%.o: runtime/%.cpp $(RT_HDRS)
	@mkdir -p $(dir $@)
	$(CXX) -O2 -g -std=c++17 $(MFLAGS) $(RTDEFS) -Iruntime -c $< -o $@

# SMAA's reference shader source as a C string (runtime/nvn/smaa_data.c)
$(B)/inc/smaa_hlsl.inc: third_party/smaa/SMAA.hlsl
	@mkdir -p $(dir $@)
	LC_ALL=C tr -d '\r' < $< | LC_ALL=C tr '\200-\377' '?' | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' > $@
$(B)/rt/nvn/smaa_data.o: $(B)/inc/smaa_hlsl.inc

ark: $(B)/ark
$(B)/ark: $(RT_OBJ) $(GEN_OBJ)
	$(CC) -o $@ $(RT_OBJ) $(GEN_OBJ) $(LDLIBS) -rdynamic

mkromfs: $(B)/mkromfs
$(B)/mkromfs: tools/mkromfs/mkromfs.c runtime/romfs_build.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -Iruntime -Iruntime/include -o $@ $^

test-lift:
	cd tools/lifter && python3 test_lift.py ../../$(EXEFS)/main 0x7000000000 20000 1

clean:
	rm -rf $(B)
