VULKAN_ROLLOUT_DIR := $(abspath $(BUILD_DIR)/vulkan-rollout)
VULKAN_ROLLOUT_INITIAL_CAPTURE := $(VULKAN_ROLLOUT_DIR)/line-drawing-initial.bmp
VULKAN_ROLLOUT_RESIZED_CAPTURE := $(VULKAN_ROLLOUT_DIR)/line-drawing-resized.bmp
VULKAN_ROLLOUT_LOG := $(VULKAN_ROLLOUT_DIR)/line-drawing-vulkan.log

vulkan-rollout-contract:
	@python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)" \
		--canonical-shared-root "$(SHARED_WORKSPACE_DIR)"

vulkan-rollout-self-test: $(APP_TARGET) vulkan-rollout-contract
	@mkdir -p "$(VULKAN_ROLLOUT_DIR)"
	@python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)" \
		--canonical-shared-root "$(SHARED_WORKSPACE_DIR)" \
		--app "$(abspath $(APP_TARGET))" \
		--initial-capture "$(VULKAN_ROLLOUT_INITIAL_CAPTURE)" \
		--resized-capture "$(VULKAN_ROLLOUT_RESIZED_CAPTURE)" \
		--log "$(VULKAN_ROLLOUT_LOG)" --minimum-scale 1.5
