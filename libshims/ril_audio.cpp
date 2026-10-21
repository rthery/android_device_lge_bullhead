/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

// Legacy QCRIL runs on vendor binder. Forward its global HAL parameters over
// hardware binder instead of waiting for framework AudioFlinger on that bus.
#define LOG_TAG "bullhead_ril_audio"
#include <android/hardware/audio/6.0/IDevicesFactory.h>
#include <log/log.h>
#include <utils/String8.h>
#include <cerrno>
#include <mutex>
#include <string>
#include <vector>

using namespace android;
using namespace android::hardware;
using namespace android::hardware::audio::V6_0;
namespace {
std::mutex lock;
sp<IDevice> device;

sp<IDevice> primary() {
    if (device != nullptr) return device;
    auto factory = IDevicesFactory::tryGetService();
    if (factory == nullptr) return nullptr;
    auto transport = factory->openDevice("primary",
        [](Result result, const sp<IDevice>& opened) {
            if (result == Result::OK) device = opened;
        });
    if (!transport.isOk()) device.clear();
    return device;
}

std::vector<std::string> split(const String8& input) {
    std::vector<std::string> parts;
    std::string text(input.c_str());
    size_t begin = 0;
    while (begin < text.size()) {
        size_t end = text.find(';', begin);
        if (end == std::string::npos) end = text.size();
        if (end > begin) parts.push_back(text.substr(begin, end - begin));
        begin = end + 1;
    }
    return parts;
}

int status(Result result) {
    switch (result) {
        case Result::OK: return 0;
        case Result::INVALID_ARGUMENTS: return -EINVAL;
        case Result::NOT_SUPPORTED: return -ENOSYS;
        case Result::NOT_INITIALIZED: return -ENODEV;
        default: return -EIO;
    }
}
}

String8 rilGetParameters(int, const String8&)
    asm("_ZN7android11AudioSystem13getParametersEiRKNS_7String8E");
String8 rilGetParameters(int io, const String8& keys) {
    if (io != 0) return String8();
    std::lock_guard<std::mutex> guard(lock);
    auto hal = primary();
    if (hal == nullptr) { ALOGE("Primary audio HAL unavailable"); return String8(); }
    auto parts = split(keys);
    hidl_vec<hidl_string> request;
    request.resize(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) request[i] = parts[i];
    String8 answer;
    auto transport = hal->getParameters({}, request,
        [&answer](Result result, const hidl_vec<ParameterValue>& values) {
            if (result != Result::OK) { ALOGE("getParameters failed: %d", int(result)); return; }
            std::string joined;
            for (const auto& value : values) {
                if (!joined.empty()) joined += ';';
                joined += value.key.c_str();
                joined += '=';
                joined += value.value.c_str();
            }
            answer = String8(joined.c_str());
        });
    if (!transport.isOk()) { device.clear(); return String8(); }
    ALOGI("Forwarded RIL audio parameter query");
    return answer;
}

int rilSetParameters(int, const String8&)
    asm("_ZN7android11AudioSystem13setParametersEiRKNS_7String8E");
int rilSetParameters(int io, const String8& pairs) {
    if (io != 0) return -ENOSYS;
    std::lock_guard<std::mutex> guard(lock);
    auto hal = primary();
    if (hal == nullptr) return -ENODEV;
    auto parts = split(pairs);
    hidl_vec<ParameterValue> request;
    request.resize(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        auto equal = parts[i].find('=');
        if (equal == std::string::npos) return -EINVAL;
        request[i].key = parts[i].substr(0, equal);
        request[i].value = parts[i].substr(equal + 1);
    }
    auto result = hal->setParameters({}, request);
    if (!result.isOk()) { device.clear(); return -EIO; }
    int error = status(result);
    ALOGI("Forwarded RIL audio parameters: status %d", error);
    return error;
}
