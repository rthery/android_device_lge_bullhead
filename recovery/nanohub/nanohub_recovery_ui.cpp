/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mount.h>

#include <string>
#include <vector>

#include "recovery_ui/device.h"
#include "recovery_ui/screen_ui.h"

// Read by init.bullhead.power.sh: keeps the A57 cores (cpu4-5) offline.
static const char kBlodFlag[] = "/cache/blod";
static const char kCacheDevice[] = "/dev/block/platform/soc.0/f9824900.sdhci/by-name/cache";

class Nanohub_Device : public Device
{
public:
    Nanohub_Device(ScreenRecoveryUI* ui) : Device(ui) {}
    const std::vector<std::string>& GetMenuItems() override;
    BuiltinAction InvokeMenuItem(size_t menu_position) override;
    bool PreWipeData() override;
    bool PostWipeData() override;

private:
    bool InAdvancedMenu();
    bool ReadBlodMode();
    void WriteBlodMode(bool enable);

    std::vector<std::string> menu_items_;
    bool blod_mode_ = false;
    bool keep_blod_ = false;
};

// Recovery unmounts /cache after each command; mount it around flag access.
static bool MountCache(bool* mounted_here)
{
    struct stat root, cache;

    *mounted_here = false;
    if (stat("/", &root) == 0 && stat("/cache", &cache) == 0 && root.st_dev != cache.st_dev)
        return true;
    if (mount(kCacheDevice, "/cache", "ext4", MS_NOATIME | MS_NOSUID | MS_NODEV, "") != 0)
        return false;
    *mounted_here = true;
    return true;
}

static void UnmountCache(bool mounted_here)
{
    sync();
    if (mounted_here)
        umount("/cache");
}

bool Nanohub_Device::ReadBlodMode()
{
    bool mounted_here;
    bool enabled;

    if (!MountCache(&mounted_here)) {
        printf("error: mount /cache failed: %s\n", strerror(errno));
        return false;
    }
    enabled = access(kBlodFlag, F_OK) == 0;
    UnmountCache(mounted_here);
    blod_mode_ = enabled;
    return enabled;
}

void Nanohub_Device::WriteBlodMode(bool enable)
{
    bool mounted_here;
    int ret;

    if (!MountCache(&mounted_here)) {
        GetUI()->Print("Failed to mount /cache: %s\n", strerror(errno));
        return;
    }
    if (enable) {
        ret = open(kBlodFlag, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
        if (ret >= 0)
            close(ret);
    } else {
        ret = unlink(kBlodFlag);
        if (ret != 0 && errno == ENOENT)
            ret = 0;
    }
    if (ret < 0)
        GetUI()->Print("Failed to change 4-core mode: %s\n", strerror(errno));
    else
        GetUI()->Print("4-core mode %s from the next boot.\n", enable ? "on" : "off");
    UnmountCache(mounted_here);
}

bool Nanohub_Device::InAdvancedMenu()
{
    const std::vector<std::string>& headers = Device::GetMenuHeaders();
    return !headers.empty() && headers[0] == "Advanced options";
}

const std::vector<std::string>& Nanohub_Device::GetMenuItems()
{
    menu_items_ = Device::GetMenuItems();
    if (InAdvancedMenu()) {
        menu_items_.push_back(ReadBlodMode() ? "Disable 4-core mode (BLOD)"
                                             : "Enable 4-core mode (BLOD)");
    }
    return menu_items_;
}

Device::BuiltinAction Nanohub_Device::InvokeMenuItem(size_t menu_position)
{
    if (InAdvancedMenu() && menu_position == Device::GetMenuItems().size()) {
        WriteBlodMode(!blod_mode_);
        return NO_ACTION;
    }
    return Device::InvokeMenuItem(menu_position);
}

bool Nanohub_Device::PreWipeData()
{
    keep_blod_ = ReadBlodMode();
    return true;
}

bool Nanohub_Device::PostWipeData()
{
    int fd;

    // A factory reset formats /cache; keep a BLOD phone in 4-core mode.
    if (keep_blod_)
        WriteBlodMode(true);

    fd = open("/sys/class/nanohub/nanohub/erase_shared", O_WRONLY);
    if (fd < 0) {
        printf("error: open erase_shared failed: %s\n", strerror(errno));
    } else {
        if (write(fd, "1\n", 2) != 2) {
            printf("error: write to erase_shared failed: %s\n", strerror(errno));
        } else {
            printf("Successfully erased nanoapps.\n");
        }
        close(fd);
    }

    // open/write failure caused by permissions issues would persist across
    // reboots, so always return true to prevent a factory reset failure loop.
    return true;
}

Device *make_device()
{
    return new Nanohub_Device(new ScreenRecoveryUI);
}
