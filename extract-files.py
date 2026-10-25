#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2016 The CyanogenMod Project
# SPDX-FileCopyrightText: 2017-2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import os

from extract_utils.elf import (
    get_file_machine_bits_libs,
    remove_libs_so_ending,
)
from extract_utils.file import File, FileArgs
from extract_utils.fixups_blob import (
    BlobFixupCtx,
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixup_remove,
    lib_fixups,
    lib_fixups_user_type,
    run_lib_fixup,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)
from extract_utils.tools import (
    android_root,
    java_path,
    patchelf_version_path_map,
)
from extract_utils.utils import (
    run_cmd,
)

PATCHELF_VERSION = '0_17_2'

namespace_imports = [
    'device/lge/bullhead',
    'hardware/qcom/wlan/legacy',
]


# Libraries the 8.1 blobs need that Soong cannot provide to them: gone
# from Android 15, system-only (including our system shims), or built by
# Make.
lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'libandroid',
        'libandroid_runtime',
        'libart',
        'libart-compiler',
        'libaudioclient',
        'libbullhead_audiosystem_shim',
        'libgps.utils',
        'libkeystore_binder',
        'libloc_core',
        'libmedia',
        'libnativehelper',
        'librmnetctl',
        'libsensor',
        'libshim_drm',
        'libshim_sensor',
        'libsoftkeymaster',
        'libstagefright',
        'libstdc++',
    ): lib_fixup_remove,
}


def patch_bytes(
    ctx: BlobFixupCtx,
    file: File,
    file_path: str,
    offset: int,
    expected: str,
    replacement: str,
    *args,
    **kwargs,
):
    expected_bytes = bytes.fromhex(expected)
    with open(file_path, 'rb+') as f:
        f.seek(offset)
        if f.read(len(expected_bytes)) != expected_bytes:
            raise ValueError(f'Unexpected bytes in {file.dst} at {offset:#x}')
        f.seek(offset)
        f.write(bytes.fromhex(replacement))


def ims_fix_arraycopy(
    ctx: BlobFixupCtx,
    file: File,
    file_path: str,
    *args,
    tmp_dir=None,
    **kwargs,
):
    # Android 13 removed the private System.arraycopy(byte[]) overload.
    smali_dir = os.path.join(android_root, 'prebuilts/extract-tools/common/smali')
    smali_out = os.path.join(tmp_dir, 'smali')
    dex = os.path.join(tmp_dir, 'classes.dex')
    old = 'Ljava/lang/System;->arraycopy([BI[BII)V'
    new = 'Ljava/lang/System;->arraycopy(Ljava/lang/Object;ILjava/lang/Object;II)V'

    run_cmd([java_path, '-jar', f'{smali_dir}/baksmali.jar', 'd', file_path, '-o', smali_out])

    count = 0
    for dir_path, _, file_names in os.walk(smali_out):
        for file_name in file_names:
            smali_path = os.path.join(dir_path, file_name)
            with open(smali_path, 'r', newline='', encoding='utf-8') as f:
                data = f.read()
            if old not in data:
                continue
            count += data.count(old)
            with open(smali_path, 'w', newline='', encoding='utf-8') as f:
                f.write(data.replace(old, new))
    if count != 4:
        raise ValueError(f'Expected 4 legacy arraycopy calls in {file.dst}, found {count}')

    run_cmd([java_path, '-jar', f'{smali_dir}/smali.jar', 'a', '-j', '1', smali_out, '-o', dex])
    # zip stores the mode, so pin it instead of inheriting the umask.
    os.chmod(dex, 0o644)
    run_cmd(['touch', '-t', '200901010000', dex])
    run_cmd(['zip', '-dq', file_path, 'META-INF/*.RSA', 'META-INF/*.SF'])
    run_cmd(['zip', '-gjqX', file_path, dex])


def check_legacy_audiosystem_imports(
    ctx: BlobFixupCtx,
    file: File,
    file_path: str,
    *args,
    **kwargs,
):
    with open(file_path, 'rb') as f:
        data = f.read()
    for symbol in ('getParameters', 'setParameters'):
        mangled = f'_ZN7android11AudioSystem13{symbol}EiRKNS_7String8E'
        if mangled.encode() not in data:
            raise ValueError(f'Missing legacy AudioSystem::{symbol} import in {file.dst}')


def check_first_needed(
    ctx: BlobFixupCtx,
    file: File,
    file_path: str,
    lib: str,
    *args,
    **kwargs,
):
    patchelf = patchelf_version_path_map[PATCHELF_VERSION]
    needed = run_cmd([patchelf, '--print-needed', file_path]).splitlines()
    if not needed or needed[0] != lib:
        raise ValueError(f'{lib} must be the first dependency of {file.dst}')


blob_fixups: blob_fixups_user_type = {
    'vendor/app/ims/ims.apk': blob_fixup()
        .call(ims_fix_arraycopy),
    ('vendor/bin/ATFWD-daemon', 'vendor/lib64/libcne.so'): blob_fixup()
        # strdup8to16, gone from libcutils.
        .patchelf_version(PATCHELF_VERSION)
        .add_needed('libshim_dpmframework.so'),
    'vendor/bin/hw/android.hardware.drm@1.0-service.widevine': blob_fixup()
        # LogMessage constructor without a tag.
        .patchelf_version(PATCHELF_VERSION)
        .add_needed('libshim_drm.so'),
    'vendor/bin/mm-qcamera-daemon': blob_fixup()
        # Tolerate pthread_mutex_destroy on an already destroyed mutex.
        .patchelf_version(PATCHELF_VERSION)
        .add_needed('libc_mutexdestroy_shim.so'),
    'vendor/bin/pm-service': blob_fixup()
        # Register on /dev/vndbinder, where its vendor clients look it up.
        .patchelf_version(PATCHELF_VERSION)
        .add_needed('libbullhead_peripheral_binder.so')
        # Provide the libutils symbols the current libutils no longer exports.
        .add_needed('libutils-v33.so'),
    'vendor/bin/slim_daemon': blob_fixup()
        # Allocate the Android 13 Looper size (264) instead of 8.1's 224.
        .call(patch_bytes, 0x12A20, 'e00b1b32bbebff97e1031f2af60300aabcebff97', '00218052', need_tmp_dir=False)
        # Legacy SensorManager::createEventQueue(String8, int).
        .patchelf_version(PATCHELF_VERSION)
        .add_needed('libshim_sensor.so'),
    'vendor/lib/libmmcamera2_stats_modules.so': blob_fixup()
        # Read the autofocus sync flag before the queue may free the message.
        .call(patch_bytes, 0x1DD56, '11f093f8012809d16568', '656811f092f8012808d1', need_tmp_dir=False),
    'vendor/lib/liboemcamera.so': blob_fixup()
        # Allocate the Android 13 Looper size (248) instead of 8.1's 112.
        .call(patch_bytes, 0x5F2A, '702014f0b0f90021054614f0b4fa', 'f820', need_tmp_dir=False)
        # Legacy SensorManager::createEventQueue(String8, int).
        .patchelf_version(PATCHELF_VERSION)
        .add_needed('libshim_sensor.so'),
    (
        'vendor/lib/hw/gatekeeper.msm8992.so',
        'vendor/lib/lib-dplmedia.so',
        'vendor/lib/lib-ims-rcscmjni.so',
        'vendor/lib/lib-imsSDP.so',
        'vendor/lib/lib-imsdpl.so',
        'vendor/lib/lib-imsqimf.so',
        'vendor/lib/lib-imsrcs.so',
        'vendor/lib/lib-imsrcscm.so',
        'vendor/lib/lib-imsrcscmclient.so',
        'vendor/lib/lib-imsrcscmservice.so',
        'vendor/lib/lib-imss.so',
        'vendor/lib/lib-imsvt.so',
        'vendor/lib/lib-imsxml.so',
        'vendor/lib/lib-rcsimssjni.so',
        'vendor/lib/lib-rcsjni.so',
        'vendor/lib/lib-rtpcommon.so',
        'vendor/lib/lib-rtpcore.so',
        'vendor/lib/lib-rtpdaemoninterface.so',
        'vendor/lib/lib-rtpsl.so',
        'vendor/lib/libQSEEComAPI.so',
        'vendor/lib/libadm.so',
        'vendor/lib/libadpcmdec.so',
        'vendor/lib/libadsprpc.so',
        'vendor/lib/libcneapiclient.so',
        'vendor/lib/libconfigdb.so',
        'vendor/lib/libdrmfs.so',
        'vendor/lib/libdrmtime.so',
        'vendor/lib/libdsi_netctrl.so',
        'vendor/lib/libdsutils.so',
        'vendor/lib/libfastcvopt.so',
        'vendor/lib/liblistensoundmodel2.so',
        'vendor/lib/liblqe.so',
        'vendor/lib/libmdsprpc.so',
        'vendor/lib/libmmcamera2_frame_algorithm.so',
        'vendor/lib/libmmcamera2_is.so',
        'vendor/lib/libmmcamera2_stats_algorithm.so',
        'vendor/lib/libmmcamera_pdaf.so',
        'vendor/lib/libmmcamera_pdafcamif.so',
        'vendor/lib/libmmcamera_tintless_algo.so',
        'vendor/lib/libmmcamera_tintless_bg_pca_algo.so',
        'vendor/lib/libnetmgr.so',
        'vendor/lib/libqdi.so',
        'vendor/lib/libqdp.so',
        'vendor/lib/librpmb.so',
        'vendor/lib/libssd.so',
        'vendor/lib/libtzdrmgenprov.so',
        'vendor/lib/libvcel.so',
        'vendor/lib/libvss_nv_core.so',
        'vendor/lib/libvss_nv_idl.so',
        'vendor/lib/libvss_nv_iface.so',
    ): blob_fixup()
        # The vendor libc stub has no LIBC_PRIVATE; bind to the default versions.
        .patchelf_version(PATCHELF_VERSION)
        .clear_symbol_version('__aeabi_atexit')
        .clear_symbol_version('__aeabi_memcpy')
        .clear_symbol_version('__aeabi_memmove')
        .clear_symbol_version('__aeabi_memset')
        .clear_symbol_version('__gnu_Unwind_Find_exidx'),
    'vendor/lib64/libimsmedia_jni.so': blob_fixup()
        # Give the legacy Surface 16 KiB and pull in its compatibility shim.
        .patchelf_version(PATCHELF_VERSION)
        .call(patch_bytes, 0x23E4, '00c781d216fbff97', '000088d2', need_tmp_dir=False)
        .add_needed('libbullhead_ims_surface.so'),
    'vendor/lib64/libril-qc-qmi-1.so': blob_fixup()
        # Resolve legacy AudioSystem parameter calls before libaudioclient.
        .patchelf_version(PATCHELF_VERSION)
        .call(check_legacy_audiosystem_imports, need_tmp_dir=False)
        # AudioSystem::setErrorCallback, now addErrorCallback.
        .add_needed('libbullhead_audiosystem_shim.so')
        .add_needed('libbullhead_ril_audio.so')
        .call(check_first_needed, 'libbullhead_ril_audio.so', need_tmp_dir=False),
}  # fmt: skip

module = ExtractUtilsModule(
    'bullhead',
    'lge',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)


def adjust_elf_checks(ctx, packages_ctx, *args, **kwargs):
    files = list(module.proprietary_files[0].file_list.package_files)
    elfs = {}
    for file in files:
        file_path = f'{packages_ctx.vendor_prop_path}/{file.dst}'
        _, bits, libs = get_file_machine_bits_libs(file_path, True)
        if libs is not None:
            elfs[file.dst] = (bits, remove_libs_so_ending(libs))

    # Blob libraries per partition and bitness.
    blob_libs = {}
    for file in files:
        if file.ext == '.so' and file.dst in elfs:
            blob_libs.setdefault(file.root, set()).add(
                (file.partition, elfs[file.dst][0])
            )

    # The lib and lib64 copies of a library share one module and its args.
    flags = {}
    for file in files:
        if file.dst not in elfs:
            continue
        bits, libs = elfs[file.dst]
        key = (file.partition, file.root)
        for lib in libs:
            providers = blob_libs.get(lib)
            if providers is not None and (file.partition, bits) not in providers:
                # Needs a blob library missing for its own partition or
                # bitness: keep it without dependencies, as before.
                flags.setdefault(key, set()).add(FileArgs.DISABLE_DEPS)
            elif run_lib_fixup(module.lib_fixups, lib, file.partition) == '':
                # check_elf_files needs every DT_NEEDED in shared_libs.
                flags.setdefault(key, set()).add(FileArgs.DISABLE_CHECKELF)

    for file in files:
        for arg in flags.get((file.partition, file.root), ()):
            file.set_arg(arg, True)


module.proprietary_files[0].add_pre_makefile_generation_fn(adjust_elf_checks)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
