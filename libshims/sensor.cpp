
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sensor/SensorEventQueue.h>
#include <sensor/SensorManager.h>
#include <utils/Looper.h>

// blob_fixup in extract-files.sh enlarges the legacy liboemcamera and
// slim_daemon Looper allocations to these sizes.
#if !defined(__LP64__)
static_assert(sizeof(android::Looper) <= 248, "Legacy camera Looper allocation too small");
#else
static_assert(sizeof(android::Looper) <= 264, "Legacy slim_daemon Looper allocation too small");
#endif

// Legacy createEventQueue(String8, int), defined as a free function so the
// compiler applies the member call ABI (sret and this) on both arm and arm64.
android::sp<android::SensorEventQueue> legacyCreateEventQueue(
        android::SensorManager* manager, android::String8 packageName, int mode)
        __asm__("_ZN7android13SensorManager16createEventQueueENS_7String8Ei");

android::sp<android::SensorEventQueue> legacyCreateEventQueue(
        android::SensorManager* manager, android::String8 packageName, int mode) {
    return manager->createEventQueue(packageName, mode, android::String16(""));
}
