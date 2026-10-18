
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <utils/Looper.h>

// The legacy liboemcamera Looper allocation is enlarged by the blob patcher.
// Keep this bound paired with scripts/patch-bullhead-camera-looper.py.
#if !defined(__LP64__)
static_assert(sizeof(android::Looper) <= 248, "Legacy camera Looper allocation too small");
#endif

namespace android {
    //android::String16::String16(char const*)
    extern "C" void _ZN7android8String16C1EPKc(void **str16P, const char *str);

    //android::String16::~String16()
    extern "C" void _ZN7android8String16D1Ev(void **str16P);

    extern "C" void _ZN7android13SensorManager16createEventQueueENS_7String8EiNS_8String16E(void **retVal, void *sensorMgr, void **str8P, int mode, void **str16P);

    extern "C" void _ZN7android13SensorManager16createEventQueueENS_7String8Ei(void **retVal, void *sensorMgr, void **str8P, int mode)
    {
        void *string;

        _ZN7android8String16C1EPKc(&string, "");
        _ZN7android13SensorManager16createEventQueueENS_7String8EiNS_8String16E(retVal, sensorMgr, str8P, mode, &string);
        _ZN7android8String16D1Ev(&string);
    }

}
