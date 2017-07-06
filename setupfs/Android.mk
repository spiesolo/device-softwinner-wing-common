LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := setupfs
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := setupfs.c
#LOCAL_SHARED_LIBRARIES := libcutils
include $(BUILD_EXECUTABLE)
