LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := bullhead_gps_debug.conf
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := gps_debug.conf
LOCAL_MODULE_STEM := gps_debug.conf
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_OVERRIDES_MODULES := gps_debug.conf
include $(BUILD_PREBUILT)
