# common.mk

SRCFILES:=$(shell find src -type f)
DEMOS:=$(notdir $(wildcard src/demo/*))
OPT_AVAILABLE:=$(notdir $(wildcard src/tool/*))
