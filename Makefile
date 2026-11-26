CC ?= gcc
AR ?= ar
CFLAGS ?= -O3 -Wall -Wextra -Werror

BUILD_DIR := build
CORE_OBJECT := $(BUILD_DIR)/agemir_api.o
CORE_LIBRARY := $(BUILD_DIR)/libagemir_core.a
CLI := $(BUILD_DIR)/agemir
API_SMOKE := $(BUILD_DIR)/api-smoke

.PHONY: all cli core test-api clean

all: cli core

$(BUILD_DIR):
	mkdir -p $@

$(CLI): Agemir.c agemir_api.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DOLLAMA_ENABLED=1 $< -o $@

cli: $(CLI)

$(CORE_OBJECT): agemir_api.c agemir_api.h Agemir.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DOLLAMA_ENABLED=1 -c $< -o $@

$(CORE_LIBRARY): $(CORE_OBJECT)
	$(AR) rcs $@ $<

core: $(CORE_LIBRARY)

$(API_SMOKE): tests/api_smoke.c agemir_api.h $(CORE_OBJECT) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(CORE_OBJECT) -o $@

test-api: $(API_SMOKE)
	tmp=$$(mktemp -d); ln -s $(CURDIR)/resources $$tmp/resources; (cd $$tmp && $(abspath $(API_SMOKE))); status=$$?; rm -rf $$tmp; exit $$status

clean:
	rm -rf $(BUILD_DIR)
