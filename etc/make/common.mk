# common.mk

SRCFILES:=$(shell find src -type f)
DEMOS:=$(notdir $(wildcard src/demo/*))
TOOLS:=$(filter-out common,$(notdir $(wildcard src/tool/*)))
OPT_AVAILABLE:=$(notdir $(wildcard src/tool/*))
