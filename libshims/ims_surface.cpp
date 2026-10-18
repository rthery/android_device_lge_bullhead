#include <gui/Surface.h>
#include <new>

// patch-bullhead-imsmedia.py expands the matching blob allocation to 16 KiB.
static_assert(sizeof(android::Surface) <= 16384, "IMS Surface allocation is too small");
extern "C" __attribute__((visibility("default"))) void legacyImsSurfaceConstructor(
        android::Surface* surface, const android::sp<android::IGraphicBufferProducer>& producer,
        bool controlledByApp) asm("_ZN7android7SurfaceC1ERKNS_2spINS_22IGraphicBufferProducerEEEb");
extern "C" void legacyImsSurfaceConstructor(
        android::Surface* surface, const android::sp<android::IGraphicBufferProducer>& producer,
        bool controlledByApp) {
    new (surface) android::Surface(producer, controlledByApp, nullptr);
}
