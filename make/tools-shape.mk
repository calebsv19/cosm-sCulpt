shape-sanity: $(SHAPE_SANITY_BIN)

$(SHAPE_SANITY_BIN): src/Tools/shape_sanity_tool.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SHAPE_DIR) $< -o $@ $(LDFLAGS)

$(BUILD_DIR)/tools/%.o: $(TOOLS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Isrc/Tools -MMD -MP -c $< -o $@

$(SHAPE_TOOL_BIN): $(SHAPE_TOOL_OBJS) $(SHAPE_TOOL_SHARED_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

shape_tool: $(SHAPE_TOOL_BIN)
	@echo "Built shape_tool successfully."

shape_pack_tool:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(SHAPE_PACK_TOOL_BIN) $(SHAPE_PACK_TOOL_SRCS) $(LDFLAGS)

shape_to_pack: shape_pack_tool
	@if [ -z "$(LAYOUT)" ] || [ -z "$(PACK)" ]; then \
		echo "usage: make shape_to_pack LAYOUT=/path/layout.json PACK=/path/output.pack [AXIS=xy|yz|xz]"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(PACK)")"
	@axis="$${AXIS:-xy}"; \
	$(SHAPE_PACK_TOOL_BIN) "$(LAYOUT)" "$(PACK)" --axis "$$axis"

shape_trace_tool:
	@mkdir -p $(BIN_DIR)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -g $(SHAPE_TRACE_TOOL_INCS) -o $(SHAPE_TRACE_TOOL_BIN) $(SHAPE_TRACE_TOOL_SRCS) -lm

shape_to_trace: shape_trace_tool
	@if [ -z "$(SHAPE)" ] || [ -z "$(TRACE)" ]; then \
		echo "usage: make shape_to_trace SHAPE=/path/export_shape.json TRACE=/path/output.trace.pack"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(TRACE)")"
	@$(SHAPE_TRACE_TOOL_BIN) "$(SHAPE)" "$(TRACE)"

shape_to_trace_batch: shape_trace_tool
	@for shape in export/*.json; do \
		base=$$(basename "$$shape" .json); \
		out="export/$${base}_trace_v0.pack"; \
		echo "trace: $$shape -> $$out"; \
		$(SHAPE_TRACE_TOOL_BIN) "$$shape" "$$out" || exit 1; \
	done

-include $(SHAPE_TOOL_OBJS:.o=.d)
