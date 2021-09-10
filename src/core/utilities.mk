#
# Obtain the current path of Makefile
#
define get_local_path
$(strip \
	$(eval current_makefile := $$(lastword $$(MAKEFILE_LIST))) \
	$(if $(filter $(CLEAR_VARIABLES),$(current_makefile)), \
		$(error LOCAL_PATH must be set before including $$(CLEAR_VARIABLES)),
		$(patsubst %/,%,$(dir $(current_makefile))) \
 	) \
)
endef

#
# Get a list of files in LOCAL_PATH
#
# Usage: $(call get_local_file_list,<pattern>)
# Ex.:   $(call get_local_file_list,*.cpp)
define get_local_file_list
$(strip \
	$(patsubst $(LOCAL_PATH)/%,%,
		$(if $(LOCAL_PATH), \
			$(wildcard $(LOCAL_PATH)/$(1)), \
			$(error LOCAL_PATH must be set before call get_file_list) \
		) \
	) \
)
endef

#
# Get a list of c/c++ files in LOCAL_PATH
#
# Usage: $(call get_local_source_list)
define get_local_source_list
$(strip \
	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_C_EXTENSION))) \
	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_CXX_EXTENSION))) \
)
endef

#
# Get a list of header files in LOCAL_PATH
#
# Usage: $(call get_local_header_list)
ifeq ($(CONFIG_C_HEADER_EXTENSION),$(CONFIG_CXX_HEADER_EXTENSION))
    define get_local_header_list
    $(strip \
    	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
    )
    endef
else
    define get_local_header_list
    $(strip \
    	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
    	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_CXX_HEADER_EXTENSION))) \
    )
    endef
endif

#
# Get a list of files in LOCAL_PATH/<path>
#
# Usage: $(call get_sub_file_list,<path>,<pattern>)
# Ex.:   $(call get_sub_file_list,sub_dir,*.cpp)
define get_sub_file_list
$(strip \
	$(patsubst $(LOCAL_PATH)/%,%,
		$(if $(LOCAL_PATH), \
			$(wildcard $(LOCAL_PATH)/$(1)*/$(2)), \
			$(error LOCAL_PATH must be set before call get_sub_file_list) \
		) \
	) \
)
endef

#
# Get a list of c/c++ files in LOCAL_PATH/<path>
#
# Usage: $(call get_sub_source_list,<path>)
# Ex.:   $(call get_sub_source_list,sub_dir)
define get_sub_source_list
$(strip \
	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_C_EXTENSION))) \
	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_CXX_EXTENSION))) \
)
endef

#
# Get a list of header files in LOCAL_PATH/<path>
#
# Usage: $(call get_sub_header_list,<path>)
# Ex.:   $(call get_sub_header_list,sub_dir)
ifeq ($(CONFIG_C_HEADER_EXTENSION),$(CONFIG_CXX_HEADER_EXTENSION))
    define get_sub_header_list
    $(strip \
    	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
    )
    endef
else
    define get_sub_header_list
    $(strip \
    	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
    	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_CXX_HEADER_EXTENSION))) \
    )
    endef
endif


#
# Recursively get a list of files
# 
# Warning: Recursively obtaining a list of all files is not recommended. Use only when absolutely necessary.
#
# Usage: $(call wildcard_r,<path>,<pattern>)
# Ex.:   $(call wildcard_r,.,*.cpp)
define wildcard_r
$(wildcard $(1)/$(2)) $(foreach dir,$(wildcard $(1)/*),$(call wildcard_r,$(dir),$(2)))
endef

#
# Recursively get a list of files in LOCAL_PATH/<path>
#
# Warning: Recursively obtaining a list of all files is not recommended. Use only when absolutely necessary.
#
# Usage: $(call get_sub_file_list_r,<path>,<pattern>)
# Ex.:   $(call get_sub_file_list_r,sub_dir,*.cpp)
define get_sub_file_list_r
$(strip \
	$(patsubst $(LOCAL_PATH)/%,%,
		$(if $(LOCAL_PATH), \
			$(call wildcard_r,$(LOCAL_PATH)/$(1),$(2)), \
			$(error LOCAL_PATH must be set before call get_sub_file_list_r) \
		) \
	) \
)
endef

#
# Get a list of c/c++ files in LOCAL_PATH/<path>
#
# Warning: Recursively obtaining a list of all files is not recommended. Use only when absolutely necessary.
#
# Usage: $(call get_sub_source_list_r,<path>)
# Ex.:   $(call get_sub_source_list_r,sub_dir)
define get_sub_source_list_r
$(strip \
	$(call get_sub_file_list_r,$(1),*.$(patsubst .%,%,$(CONFIG_C_EXTENSION))) \
	$(call get_sub_file_list_r,$(1),*.$(patsubst .%,%,$(CONFIG_CXX_EXTENSION))) \
)
endef

#
# Get a list of header files in LOCAL_PATH/<path>
#
# Warning: Recursively obtaining a list of all files is not recommended. Use only when absolutely necessary.
#
# Usage: $(call get_sub_header_list_r,<path>)
# Ex.:   $(call get_sub_header_list_r,sub_dir)
ifeq ($(CONFIG_C_HEADER_EXTENSION),$(CONFIG_CXX_HEADER_EXTENSION))
    define get_sub_header_list_r
    $(strip \
    	$(call get_sub_file_list_r,$(1),*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
    )
    endef
else
    define get_sub_header_list_r
    $(strip \
    	$(call get_sub_file_list_r,$(1),*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
    	$(call get_sub_file_list_r,$(1),*.$(patsubst .%,%,$(CONFIG_CXX_HEADER_EXTENSION))) \
    )
    endef
endif

#
# Get a list of sub directories in LOCAL_PATH
#
# Usage: $(call get_sub_directories,<path>)
# Ex.:   $(call get_sub_directories,.)
define get_sub_directories
$(strip \
	$(patsubst %/,%,${sort ${dir ${wildcard $(1)/*/*}}}) \
)
endef

# Add compiler/linker options into LOCAL_* variables
# $(call add_pkg_config,<LIBRARY_NAME>)
define add_pkg_config
	$(eval 
		LOCAL_CFLAGS += $(shell $(__PKG_CONFIG_PATH) pkg-config --cflags $(1))
		LOCAL_CXXFLAGS += $(shell $(__PKG_CONFIG_PATH) pkg-config --cflags $(1))
		LOCAL_LDFLAGS += $(shell $(__PKG_CONFIG_PATH) pkg-config --libs $(1))
	)
endef