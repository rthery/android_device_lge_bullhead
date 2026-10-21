/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gui/Surface.h>
#include <new>

// blob_fixup in extract-files.sh expands the libimsmedia_jni allocation to 16 KiB.
static_assert(sizeof(android::Surface) <= 16384, "IMS Surface allocation is too small");
extern "C" __attribute__((visibility("default"))) void legacyImsSurfaceConstructor(
        android::Surface* surface, const android::sp<android::IGraphicBufferProducer>& producer,
        bool controlledByApp) asm("_ZN7android7SurfaceC1ERKNS_2spINS_22IGraphicBufferProducerEEEb");
extern "C" void legacyImsSurfaceConstructor(
        android::Surface* surface, const android::sp<android::IGraphicBufferProducer>& producer,
        bool controlledByApp) {
    new (surface) android::Surface(producer, controlledByApp, nullptr);
}
