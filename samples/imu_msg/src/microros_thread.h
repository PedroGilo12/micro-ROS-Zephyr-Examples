#ifndef MICROROS_THREAD_H
#define MICROROS_THREAD_H

#include <zephyr/zbus/zbus.h>

struct wifi_status_report {
    bool connected;
};

ZBUS_CHAN_DECLARE(wifi_status_channel);
ZBUS_CHAN_DECLARE(imu_channel);

#endif /* MICROROS_THREAD_H */