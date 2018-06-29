# Build C files
ifneq ($(strip $(BUILD_C_OBJECT_FILES)),)
$(BUILD_C_OBJECT_FILES) : $(BUILD_OBJECTS_DIRECTORY)/%.o : $(BUILD_PROJECTS_DIRECTORY)/%$(CONFIG_C_EXTENSION)
	@$(TARGET_COUNTER)
	@mkdir -p `dirname $@`
	@echo $(CURRENT_PROGRESS)"$(CONFIG_BUILD_TYPE_COLOR)[$(PRIVATE_BUILD_TYPE)]$(ANSI_RESET) $(CONFIG_BUILDING_COLOR)$(PRIVATE_TARGET)$(ANSI_RESET): $(CONFIG_COMPILING_COLOR)C$(ANSI_RESET) $(CONFIG_SOURCE_FILE_COLOR)$<$(ANSI_RESET) => $(CONFIG_TARGET_FILE_COLOR)$@$(ANSI_RESET)"$(INCREASE_COUNT)
	@$(PRIVATE_CC) -MMD -c $(PRIVATE_CFLAGS) $(PRIVATE_C_INCLUDES) -o "$@" $< 
	@cp $(@:%.o=%.d) $(@:%.o=%.P)
	@sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P)
	@rm -f $(@:%.o=%.d)
	@touch "$@"
$(BUILD_C_OBJECT_FILES) : $(BUILD_PROJECTS_DIRECTORY)/AMS.mk $(LOCAL_PATH)/AMS.mk
endif

# Build C++ files
ifneq ($(strip $(BUILD_CXX_OBJECT_FILES)),)
$(BUILD_CXX_OBJECT_FILES) : $(BUILD_OBJECTS_DIRECTORY)/%.o : $(BUILD_PROJECTS_DIRECTORY)/%$(CONFIG_CXX_EXTENSION)
	@$(TARGET_COUNTER)
	@mkdir -p `dirname $@`
	@echo $(CURRENT_PROGRESS)"$(CONFIG_BUILD_TYPE_COLOR)[$(PRIVATE_BUILD_TYPE)]$(ANSI_RESET) $(CONFIG_BUILDING_COLOR)$(PRIVATE_TARGET)$(ANSI_RESET): $(CONFIG_COMPILING_COLOR)C++$(ANSI_RESET) $(CONFIG_SOURCE_FILE_COLOR)$<$(ANSI_RESET) => $(CONFIG_TARGET_FILE_COLOR)$@$(ANSI_RESET)"$(INCREASE_COUNT)
	@$(PRIVATE_CXX) -MMD -c $(PRIVATE_CXXFLAGS) $(PRIVATE_CXX_INCLUDES) -o "$@" $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P)
	@sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P)
	@rm -f $(@:%.o=%.d)
	@touch "$@"
$(BUILD_CXX_OBJECT_FILES) : $(BUILD_PROJECTS_DIRECTORY)/AMS.mk $(LOCAL_PATH)/AMS.mk
endif

