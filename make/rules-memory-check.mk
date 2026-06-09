MEMORY_CHECK_BUILD_TOOLCHAIN := fisics
MEMORY_CHECK_FISICS_OVERLAY := physics-units,memory-check
MEMORY_CHECK_REPORT_DIR := build/memory_check
MEMORY_CHECK_STDOUT := $(MEMORY_CHECK_REPORT_DIR)/line_drawing.stdout
MEMORY_CHECK_STDERR := $(MEMORY_CHECK_REPORT_DIR)/line_drawing.stderr
MEMORY_CHECK_REPORT_POLICY ?= always
FISICS_MEMCHECK_RUNTIME ?= /Users/calebsv/Desktop/CodeWork/fisiCs/build/unsanitized/libfisics_memcheck_runtime.a
FISICS_MEMCHECK_LINK_LIBS :=

ifeq ($(BUILD_TOOLCHAIN),fisics)
ifneq ($(findstring memory-check,$(FISICS_OVERLAY)),)
FISICS_MEMCHECK_LINK_LIBS += $(FISICS_MEMCHECK_RUNTIME)
endif
endif

memory-check-build:
	@$(MAKE) BUILD_TOOLCHAIN="$(MEMORY_CHECK_BUILD_TOOLCHAIN)" FISICS_OVERLAY="$(MEMORY_CHECK_FISICS_OVERLAY)" -B "$(TEST_TARGET)"

memory-check-run: memory-check-build
	mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	set +e; FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(TEST_TARGET)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"; status=$$?; \
	echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"; \
	echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"; \
	exit $$status

memory-check-audit: memory-check-build
	mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	set +e; FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(TEST_TARGET)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"; status=$$?; \
	echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"; \
	echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"; \
	echo "memory-check summary:"; \
	grep -E "\\[fisics:memory-check\\] (summary|leak|double free|unknown pointer free)" "$(MEMORY_CHECK_STDERR)" || true; \
	exit $$status
