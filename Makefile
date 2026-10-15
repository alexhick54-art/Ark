# Ark build.
#   make gen EXEFS=path/to/exefs     translate the game's code to C (needs Unicorn's Python bindings; tools/setup_unicorn.sh)
#   make -j objs                     compile everything under generated/ into build/obj/
#   make -j ark                      runtime + game objects -> build/ark (Linux, headless for now)
#   make mkromfs                     build/mkromfs: RomFS image from a romfs folder
#   make test-lift EXEFS=...         differential test of the translator against Unicorn
EXEFS    ?= exefs
GEN      ?= generated
B        ?= build
# make predefines CC=cc; the generated code needs clang (musttail), so default to it unless CC is given.
ifeq ($(origin CC),default)
CC       := clang
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
RT_OBJ   := $(patsubst runtime/%.c,$(B)/rt/%.o,$(RT_SRC))
RTFLAGS  := -O1 -g -std=gnu11 $(MFLAGS) -fno-strict-aliasing -DARK_HEADLESS -Iruntime/include -Iruntime -Iruntime/headless \
            -Wall -Wno-unused-function -Wno-format
LDLIBS   := -lpthread -lm -ldl

.PHONY: gen objs ark mkromfs test-lift clean
gen:
	python3 tools/gen_all.py $(EXEFS) $(GEN) --simd $(SIMD)

objs: $(GEN_OBJ)
	@echo "compiled $(words $(GEN_OBJ)) files"

$(B)/obj/%.o: $(GEN)/%.c runtime/include/cpu.h
	@mkdir -p $(dir $@)
	@$(CC) $(GENFLAGS) -c $< -o $@

$(B)/rt/%.o: runtime/%.c $(wildcard runtime/*.h) runtime/include/cpu.h
	@mkdir -p $(dir $@)
	$(CC) $(RTFLAGS) -c $< -o $@

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
