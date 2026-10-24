#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017-2020 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE=bullhead
VENDOR=lge

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."

HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

KANG=
SECTION=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n | --no-cleanup )
                CLEAN_VENDOR=false
                ;;
        -k | --kang )
                KANG="--kang"
                ;;
        -s | --section )
                SECTION="${2}"; shift
                CLEAN_VENDOR=false
                ;;
        * )
                SRC="${1}"
                ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    SRC="adb"
fi

# Replace bytes at an offset after checking the original ones.
function patch_bytes() {
    local file="${1}" offset="${2}" expected="${3}" replacement="${4}"
    local actual
    actual=$(xxd -p -s "${offset}" -l $(( ${#expected} / 2 )) "${file}" | tr -d '\n')
    if [ "${actual}" != "${expected}" ]; then
        echo "Unexpected bytes in ${file} at ${offset}"
        exit 1
    fi
    echo "${replacement}" | xxd -r -p | dd of="${file}" bs=1 seek=$(( offset )) conv=notrunc status=none || exit 1
}

function blob_fixup() {
    case "${1}" in
        vendor/app/ims/ims.apk)
            # Android 13 removed the private System.arraycopy(byte[]) overload.
            local smali="${ANDROID_ROOT}/prebuilts/extract-tools/common/smali"
            local work="${EXTRACT_TMP_DIR}/ims"
            local old="Ljava/lang/System;->arraycopy([BI[BII)V"
            local new="Ljava/lang/System;->arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V"
            rm -rf "${work}"
            java -jar "${smali}/baksmali.jar" d "${2}" -o "${work}/smali" || exit 1
            if [ "$(grep -rhoF "${old}" "${work}/smali" | wc -l)" -ne 4 ]; then
                echo "Expected 4 legacy arraycopy calls in ${1}"
                exit 1
            fi
            grep -rlF "${old}" "${work}/smali" | xargs sed -i "s|Ljava/lang/System;->arraycopy(\[BI\[BII)V|${new}|g" || exit 1
            java -jar "${smali}/smali.jar" a -j 1 "${work}/smali" -o "${work}/classes.dex" || exit 1
            touch -t 200901010000 "${work}/classes.dex"
            zip -dq "${2}" "META-INF/*.RSA" "META-INF/*.SF" || exit 1
            zip -gjqX "${2}" "${work}/classes.dex" || exit 1
            ;;
        vendor/bin/pm-service)
            # Register on /dev/vndbinder, where its vendor clients look it up.
            "${PATCHELF}" --add-needed "libbullhead_peripheral_binder.so" "${2}" || exit 1
            # Provide the libutils symbols the current libutils no longer exports.
            "${PATCHELF}" --add-needed "libutils-v33.so" "${2}" || exit 1
            ;;
        vendor/bin/slim_daemon)
            # Allocate the Android 13 Looper size (264) instead of 8.1's 224.
            patch_bytes "${2}" 0x12a20 e00b1b32bbebff97e1031f2af60300aabcebff97 00218052
            ;;
        vendor/lib/libmmcamera2_stats_modules.so)
            # Read the autofocus sync flag before the queue may free the message.
            patch_bytes "${2}" 0x1dd56 11f093f8012809d16568 656811f092f8012808d1
            ;;
        vendor/lib/liboemcamera.so)
            # Allocate the Android 13 Looper size (248) instead of 8.1's 112.
            patch_bytes "${2}" 0x5f2a 702014f0b0f90021054614f0b4fa f820
            ;;
        vendor/lib64/libimsmedia_jni.so)
            # Give the legacy Surface 16 KiB and pull in its compatibility shim.
            patch_bytes "${2}" 0x23e4 00c781d216fbff97 000088d2
            "${PATCHELF}" --add-needed "libbullhead_ims_surface.so" "${2}" || exit 1
            ;;
        vendor/lib64/libril-qc-qmi-1.so)
            # Resolve legacy AudioSystem parameter calls before libaudioclient.
            local symbol
            for symbol in getParameters setParameters; do
                if ! grep -qF "_ZN7android11AudioSystem13${symbol}EiRKNS_7String8E" "${2}"; then
                    echo "Missing legacy AudioSystem::${symbol} import in ${1}"
                    exit 1
                fi
            done
            "${PATCHELF}" --add-needed "libbullhead_ril_audio.so" "${2}" || exit 1
            if [ "$("${PATCHELF}" --print-needed "${2}" | head -n 1)" != "libbullhead_ril_audio.so" ]; then
                echo "libbullhead_ril_audio.so must be the first dependency of ${1}"
                exit 1
            fi
            ;;
    esac
}

export PATCHELF_VERSION=0_17_2

# Initialize the helper
setup_vendor "${DEVICE}" "${VENDOR}" "${ANDROID_ROOT}" false "${CLEAN_VENDOR}"

extract "${MY_DIR}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"

"${MY_DIR}/setup-makefiles.sh"
