# Ark - milestone 1 build: translate the game's code to C and compile it.
#   make gen EXEFS=path/to/exefs     translate (needs Unicorn's Python bindings; see tools/setup_unicorn.sh)
#   make -j objs                     compile everything under generated/ into build/obj/
#   make test-lift EXEFS=...         differential test of the translator against Unicorn
EXEFS    ?= exefs
GEN      ?= generated
B        ?= build
CC       ?= clang
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

.PHONY: gen objs test-lift clean
gen:
	python3 tools/gen_all.py $(EXEFS) $(GEN) --simd $(SIMD)

objs: $(GEN_OBJ)
	@echo "compiled $(words $(GEN_OBJ)) files"

$(B)/obj/%.o: $(GEN)/%.c runtime/include/cpu.h
	@mkdir -p $(dir $@)
	@$(CC) $(GENFLAGS) -c $< -o $@

test-lift:
	cd tools/lifter && python3 test_lift.py ../../$(EXEFS)/main 0x7000000000 20000 1

clean:
	rm -rf $(B)
