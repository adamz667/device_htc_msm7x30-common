/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "msm7x30 PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define BOOSTPULSE_ONDEMAND "/sys/devices/system/cpu/cpufreq/ondemand/boostpulse"
#define BOOSTPULSE_INTERACTIVE "/sys/devices/system/cpu/cpufreq/interactive/boostpulse"
#define SAMPLING_RATE_ONDEMAND "/sys/devices/system/cpu/cpufreq/ondemand/sampling_rate"
#define SAMPLING_RATE_SCREEN_ON "50000"
#define SAMPLING_RATE_SCREEN_OFF "500000"

struct msm7x30_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
};

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static int boostpulse_open(struct msm7x30_power_module *msm7x30)
{
    char buf[80];

    pthread_mutex_lock(&msm7x30->lock);

    if (msm7x30->boostpulse_fd < 0) {
        msm7x30->boostpulse_fd = open(BOOSTPULSE_ONDEMAND, O_WRONLY);
        if (msm7x30->boostpulse_fd < 0) {
            msm7x30->boostpulse_fd = open(BOOSTPULSE_INTERACTIVE, O_WRONLY);

            if (msm7x30->boostpulse_fd < 0 && !msm7x30->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening boostpulse: %s\n", buf);
                msm7x30->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&msm7x30->lock);
    return msm7x30->boostpulse_fd;
}

static void msm7x30_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct msm7x30_power_module *msm7x30 = (struct msm7x30_power_module *) module;
    char buf[80];
    int len;

    switch (hint) {
    case POWER_HINT_INTERACTION:
        if (boostpulse_open(msm7x30) >= 0) {
	    len = write(msm7x30->boostpulse_fd, "1", 1);

	    if (len < 0) {
	        strerror_r(errno, buf, sizeof(buf));
		    ALOGE("Error writing to boostpulse: %s\n", buf);

            pthread_mutex_lock(&msm7x30->lock);
            close(msm7x30->boostpulse_fd);
            msm7x30->boostpulse_fd = -1;
            msm7x30->boostpulse_warned = 0;
            pthread_mutex_unlock(&msm7x30->lock);
	    }
	}
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
}

static void msm7x30_power_set_interactive(struct power_module *module, int on)
{
    sysfs_write(SAMPLING_RATE_ONDEMAND,
            on ? SAMPLING_RATE_SCREEN_ON : SAMPLING_RATE_SCREEN_OFF);
}

static void msm7x30_power_init(struct power_module *module)
{
    sysfs_write(SAMPLING_RATE_ONDEMAND, SAMPLING_RATE_SCREEN_ON);
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct msm7x30_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "msm7x30 Power HAL",
            author: "The CyanogenMod Project",
            methods: &power_module_methods,
        },
       init: msm7x30_power_init,
       setInteractive: msm7x30_power_set_interactive,
       powerHint: msm7x30_power_hint,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
};

