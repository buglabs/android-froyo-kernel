LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(TARGET_KERNEL_IS_BUILT),true)
# Explicitly mark gdbserver as "eng" so that it doesn't
# get included in user or SDK builds. (GPL issues)
#
LOCAL_SRC_FILES := drivers/bmi/pims/vonhippel/bmi_vh.ko 
LOCAL_MODULE := modules/bmi_vh.ko
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := user eng
include $(BUILD_PREBUILT)

# Any prebuilt files with default TAGS can use the below:
prebuilt_files :=

include $(CLEAR_VARS)
LOCAL_SRC_FILES := drivers/bmi/pims/gsm/bmi_gsm.ko
LOCAL_MODULE := modules/bmi_gsm.ko
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := user eng
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := drivers/bmi/pims/lcd/bmi_lcd_core.ko
LOCAL_MODULE := modules/bmi_lcd_core.ko
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := user eng
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := drivers/bmi/pims/gps/bmi_gps.ko
LOCAL_MODULE := modules/bmi_gps.ko
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := user eng
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := drivers/bmi/pims/accenture/bmi_accnt.ko
LOCAL_MODULE := modules/bmi_accnt.ko
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := user eng
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := drivers/bmi/pims/video/bmi_video_core.ko
LOCAL_MODULE := modules/bmi_video_core.ko
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_TAGS := user eng
include $(BUILD_PREBUILT)


$(call add-prebuilt-files, EXECUTABLES, $(prebuilt_files))
endif
