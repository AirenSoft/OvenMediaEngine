# 현재 Makefile의 경로를 얻어옴
define get_local_path
$(strip \
	$(eval current_makefile := $$(lastword $$(MAKEFILE_LIST))) \
	$(if $(filter $(CLEAR_VARIABLES),$(current_makefile)), \
		$(error LOCAL_PATH must be set before including $$(CLEAR_VARIABLES)),
		$(patsubst %/,%,$(dir $(current_makefile))) \
 	) \
)
endef

# LOCAL_PATH 안의 파일 목록을 얻어옴
# $(call get_local_file_list,<패턴>)
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

# LOCAL_PATH 안의 소스 파일 목록을 얻어옴
define get_local_source_list
$(strip \
	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_C_EXTENSION))) \
	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_CXX_EXTENSION))) \
)
endef

# LOCAL_PATH 안의 헤더 파일 목록을 얻어옴
define get_local_header_list
$(strip \
	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
	$(call get_local_file_list,*.$(patsubst .%,%,$(CONFIG_CXX_HEADER_EXTENSION))) \
)
endef

# 하위 디렉토리의 파일 목록을 얻어옴
# $(call get_sub_file_list,<경로>,<패턴>)
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

# 하위 디렉토리의 소스 코드 목록을 얻어옴
# $(call get_sub_source_list,<경로>)
define get_sub_source_list
$(strip \
	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_C_EXTENSION))) \
	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_CXX_EXTENSION))) \
)
endef

# 하위 디렉토리의 소스 코드 목록을 얻어옴
# $(call get_sub_header_list,<경로>)
define get_sub_header_list
$(strip \
	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_C_HEADER_EXTENSION))) \
	$(call get_sub_file_list,$(1),*.$(patsubst .%,%,$(CONFIG_CXX_HEADER_EXTENSION))) \
)
endef

# 하위 디렉토리 목록을 얻어옴
# $(call get_sub_directories,<경로>)
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