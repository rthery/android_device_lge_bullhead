/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

// Flat Parcel for the 8.1 CNE blobs. They keep Parcel on the stack in a slot sized for 8.1, which the
// Android 15 Parcel overruns; this one is smaller and writes the same byte format.

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <utils/String16.h>

namespace android {

typedef int32_t status_t;

static size_t pad4(size_t len) {
    return (len + 3) & ~static_cast<size_t>(3);
}

class Parcel {
  public:
    Parcel();
    ~Parcel();

    const uint8_t* data() const;
    size_t dataSize() const;
    status_t setData(const uint8_t* buffer, size_t len);

    status_t write(const void* data, size_t len);
    status_t writeInt32(int32_t val);
    status_t writeString16(const String16& str);
    status_t writeString16(const char16_t* str, size_t len);

    status_t read(void* outData, size_t len) const;
    const void* readInplace(size_t len) const;
    int32_t readInt32() const;
    status_t readInt32(int32_t* pArg) const;
    String16 readString16() const;

  private:
    void* writeInplace(size_t len);

    uint8_t* mData;
    size_t mDataSize;
    size_t mDataCapacity;
    mutable size_t mDataPos;
};

Parcel::Parcel() : mData(nullptr), mDataSize(0), mDataCapacity(0), mDataPos(0) {}

Parcel::~Parcel() {
    free(mData);
}

const uint8_t* Parcel::data() const {
    return mData;
}

size_t Parcel::dataSize() const {
    return mDataSize > mDataPos ? mDataSize : mDataPos;
}

status_t Parcel::setData(const uint8_t* buffer, size_t len) {
    uint8_t* copy = static_cast<uint8_t*>(malloc(len ? len : 1));
    if (copy == nullptr) return -ENOMEM;
    if (len) memcpy(copy, buffer, len);
    free(mData);
    mData = copy;
    mDataSize = mDataCapacity = len;
    mDataPos = 0;
    return 0;
}

void* Parcel::writeInplace(size_t len) {
    size_t padded = pad4(len);
    if (padded < len) return nullptr;
    size_t end = mDataPos + padded;
    if (end > mDataCapacity) {
        size_t cap = end * 3 / 2;
        uint8_t* grown = static_cast<uint8_t*>(realloc(mData, cap));
        if (grown == nullptr) return nullptr;
        mData = grown;
        mDataCapacity = cap;
    }
    uint8_t* out = mData + mDataPos;
    memset(out + len, 0, padded - len);
    mDataPos = end;
    if (mDataPos > mDataSize) mDataSize = mDataPos;
    return out;
}

status_t Parcel::write(const void* data, size_t len) {
    void* out = writeInplace(len);
    if (out == nullptr) return -ENOMEM;
    memcpy(out, data, len);
    return 0;
}

status_t Parcel::writeInt32(int32_t val) {
    return write(&val, sizeof(val));
}

status_t Parcel::writeString16(const char16_t* str, size_t len) {
    if (str == nullptr) return writeInt32(-1);
    status_t err = writeInt32(static_cast<int32_t>(len));
    if (err) return err;
    char16_t* out = static_cast<char16_t*>(writeInplace((len + 1) * sizeof(char16_t)));
    if (out == nullptr) return -ENOMEM;
    memcpy(out, str, len * sizeof(char16_t));
    out[len] = 0;
    return 0;
}

status_t Parcel::writeString16(const String16& str) {
    return writeString16(str.c_str(), str.size());
}

const void* Parcel::readInplace(size_t len) const {
    size_t padded = pad4(len);
    if (padded < len || mDataPos + padded < mDataPos || mDataPos + padded > mDataSize) return nullptr;
    const void* in = mData + mDataPos;
    mDataPos += padded;
    return in;
}

status_t Parcel::read(void* outData, size_t len) const {
    const void* in = readInplace(len);
    if (in == nullptr) return -ENODATA;
    memcpy(outData, in, len);
    return 0;
}

status_t Parcel::readInt32(int32_t* pArg) const {
    return read(pArg, sizeof(*pArg));
}

int32_t Parcel::readInt32() const {
    int32_t val = 0;
    readInt32(&val);
    return val;
}

String16 Parcel::readString16() const {
    int32_t len = readInt32();
    if (len < 0 || len >= INT32_MAX / 2) return String16();
    const char16_t* str = static_cast<const char16_t*>(readInplace((len + 1) * sizeof(char16_t)));
    if (str == nullptr || str[len] != 0) return String16();
    return String16(str, len);
}

}  // namespace android
