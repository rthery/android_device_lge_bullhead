/*
 * SPDX-FileCopyrightText: 2026 Romain Théry
 * SPDX-License-Identifier: Apache-2.0
 */

#include <binder/ProcessState.h>

// The legacy service otherwise selects framework binder, unlike its vendor clients.
__attribute__((constructor)) static void initPeripheralBinder() {
    android::ProcessState::initWithDriver("/dev/vndbinder");
}
