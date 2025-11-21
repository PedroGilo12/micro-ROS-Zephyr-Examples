#include "microros_thread.h"
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <sensor_msgs/msg/imu.h>
#include <rmw_microros/rmw_microros.h>
#include <microros_transports.h>

#define LOG_MODULE_NAME microros_thread
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define MICROROS_THREAD_STK_SIZE 4096
#define MICROROS_THREAD_PRIORITY 5

ZBUS_CHAN_DEFINE(
    wifi_status_channel,
    struct wifi_status_report,
    NULL,
    NULL,
    ZBUS_OBSERVERS(microros_sub),
    ZBUS_MSG_INIT(.connected = false));

ZBUS_CHAN_DEFINE(
    imu_channel,
    sensor_msgs__msg__Imu,
    NULL,
    NULL,
    ZBUS_OBSERVERS(microros_sub),
    ZBUS_MSG_INIT()
);

ZBUS_SUBSCRIBER_DEFINE(microros_sub, 2);

static rcl_publisher_t publisher;
static sensor_msgs__msg__Imu imu_msg;

void timer_callback(rcl_timer_t *timer, int64_t last_call_time)
{
    RCLC_UNUSED(last_call_time);
    if (timer == NULL) return;

    rcl_ret_t rc = rcl_publish(&publisher, &imu_msg, NULL);
    if (rc != RCL_RET_OK)
    {
        LOG_ERR("rcl_publish failed: %d", rc);
    }
}

void microros_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    const struct zbus_channel *chan;
    struct wifi_status_report msg;

    while (1)
    {
        if (zbus_sub_wait(&microros_sub, &chan, K_FOREVER) != 0) continue;
        if (chan != &wifi_status_channel) continue;
        if (zbus_chan_read(&wifi_status_channel, &msg, K_MSEC(500)) != 0) continue;
        if (!msg.connected) continue;

        strncpy(default_params.ip, CONFIG_AGENT_IP, sizeof(default_params.ip) - 1);
        default_params.ip[sizeof(default_params.ip) - 1] = '\0';

        strncpy(default_params.port, CONFIG_AGENT_PORT, sizeof(default_params.port) - 1);
        default_params.port[sizeof(default_params.port) - 1] = '\0';

        rmw_uros_set_custom_transport(
            false,
            (void *)&default_params,
            zephyr_transport_open,
            zephyr_transport_close,
            zephyr_transport_write,
            zephyr_transport_read);

        rcl_ret_t rc;
        rcl_allocator_t allocator = rcl_get_default_allocator();
        rclc_support_t support;

        rc = rclc_support_init(&support, 0, NULL, &allocator);
        if (rc != RCL_RET_OK) continue;

        rcl_node_t node;
        rc = rclc_node_init_default(&node, "zephyr_imu_publisher", "", &support);
        if (rc != RCL_RET_OK) continue;

        rc = rclc_publisher_init_default(
            &publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
            "imu_data");
        if (rc != RCL_RET_OK) continue;

        rcl_timer_t timer;
        const uint32_t timer_timeout_ms = 100;

        rc = rclc_timer_init_default(
            &timer,
            &support,
            RCL_MS_TO_NS(timer_timeout_ms),
            timer_callback);
        if (rc != RCL_RET_OK) continue;

        rclc_executor_t executor;
        rc = rclc_executor_init(&executor, &support.context, 1, &allocator);
        if (rc != RCL_RET_OK) continue;

        rc = rclc_executor_add_timer(&executor, &timer);
        if (rc != RCL_RET_OK) continue;

        memset(&imu_msg, 0, sizeof(sensor_msgs__msg__Imu));

        while (1)
        {
            rclc_executor_spin_some(&executor, RCL_MS_TO_NS(50));

            if (zbus_chan_read(&wifi_status_channel, &msg, K_NO_WAIT) == 0)
            {
                if (!msg.connected) break;
            }

            if (zbus_sub_wait(&microros_sub, &chan, K_NO_WAIT) == 0)
            {
                if (chan == &imu_channel)
                {
                    zbus_chan_read(&imu_channel, &imu_msg, K_NO_WAIT);
                }
            }

            k_sleep(K_MSEC(10));
        }
    }
}

K_THREAD_DEFINE(
    microros_thread_id,
    MICROROS_THREAD_STK_SIZE,
    microros_thread,
    NULL, NULL, NULL,
    MICROROS_THREAD_PRIORITY,
    0,
    0);
