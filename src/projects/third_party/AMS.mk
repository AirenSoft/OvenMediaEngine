LOCAL_PATH := $(call get_local_path)

# 현재 디렉토리의 AMS-*.mk 호출
AMS_FILE_LIST := $(call get_local_file_list,AMS-*.mk)
$(foreach file,$(AMS_FILE_LIST),$(eval include $(LOCAL_PATH)/$(file)))

