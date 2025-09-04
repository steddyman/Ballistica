# ====== CONFIG ======
# List all base names (each must have NAME.PCX; NAME.INF is optional)
PACKS := DESIGNER BRICKS UI BATS

# Texture atlas params (can be overridden per-invocation: make MAXSIZE=2048x2048)
MAXSIZE ?= 1024x1024
FORMAT  ?= rgba8        # alternatives: rgb565, rgba5551
TRIM    ?= no           # yes|no
ROTATE  ?= no           # yes|no

PYTHON  ?= python3
TEX3DS  ?= tex3ds

# ====== DERIVED TARGETS ======
T3S := $(addsuffix .t3s,$(PACKS))
T3X := $(addsuffix .t3x,$(PACKS))
HDR := $(addsuffix _indices.h,$(PACKS))

.PHONY: all
all: $(T3X)

# Build each .t3s + header by slicing the corresponding .PCX/.INF
# The Python script opens NAME.PCX directly (case-insensitive) and optional NAME.INF.
%.t3s: %.PCX build_t3x_from_pcx.py
	$(PYTHON) build_t3x_from_pcx.py $(basename $@) \
	  --maxsize $(MAXSIZE) --format $(FORMAT) --trim $(TRIM) --rotate $(ROTATE)

# Also allow lowercase .pcx as input
%.t3s: %.pcx build_t3x_from_pcx.py
	$(PYTHON) build_t3x_from_pcx.py $(basename $@) \
	  --maxsize $(MAXSIZE) --format $(FORMAT) --trim $(TRIM) --rotate $(ROTATE)

# Produce the .t3x atlas with tex3ds
%.t3x: %.t3s
	$(TEX3DS) $<

# Convenience target to ensure headers are present (runs slicer without rebuilding .t3x)
headers: $(HDR)

# The headers are side-effects of the slicer; declare their dependency
%_indices.h: %.t3s ;

# Clean
.PHONY: clean distclean
clean:
	rm -f *.t3s *.t3x *_indices.h
	rm -rf out_slices

distclean: clean
	@true
