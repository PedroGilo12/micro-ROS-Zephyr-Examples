#include "accel_driver.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include <sensor_msgs/msg/imu.h>

#include <time.h>  

#include <zephyr/zbus/zbus.h>
#include "microros_thread.h"


#define ACCEL_THREAD_STACK 2048
#define ACCEL_THREAD_PRIO 4

extern const struct zbus_channel imu_channel;
static const struct device *mpu6050;

static inline float val(const struct sensor_value *v)
{
    return (float)sensor_value_to_double(v);
}

static void accel_thread(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    struct sensor_value accel[3];
    struct sensor_value gyro[3];

    sensor_msgs__msg__Imu msg;
    memset(&msg, 0, sizeof(msg));

    while (1)
    {
        if (sensor_sample_fetch(mpu6050) != 0)
        {
            k_sleep(K_MSEC(100));
            continue;
        }

        if (sensor_channel_get(mpu6050, SENSOR_CHAN_ACCEL_XYZ, accel) != 0)
        {
            k_sleep(K_MSEC(100));
            continue;
        }

        if (sensor_channel_get(mpu6050, SENSOR_CHAN_GYRO_XYZ, gyro) != 0)
        {
            k_sleep(K_MSEC(100));
            continue;
        }

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);

        msg.header.stamp.sec = ts.tv_sec;
        msg.header.stamp.nanosec = ts.tv_nsec;

        msg.header.frame_id.data = "imu_link";
        msg.header.frame_id.size = strlen("imu_link");
        msg.header.frame_id.capacity = msg.header.frame_id.size + 1;

        msg.linear_acceleration.x = val(&accel[0]);
        msg.linear_acceleration.y = val(&accel[1]);
        msg.linear_acceleration.z = val(&accel[2]);

        msg.angular_velocity.x = val(&gyro[0]);
        msg.angular_velocity.y = val(&gyro[1]);
        msg.angular_velocity.z = val(&gyro[2]);

        msg.orientation.w = 1.0f;
        msg.orientation.x = 0.0f;
        msg.orientation.y = 0.0f;
        msg.orientation.z = 0.0f;

        msg.orientation_covariance[0] = 0.001;
        msg.orientation_covariance[4] = 0.001;
        msg.orientation_covariance[8] = 0.001;

        msg.angular_velocity_covariance[0] = 0.001;
        msg.angular_velocity_covariance[4] = 0.001;
        msg.angular_velocity_covariance[8] = 0.001;

        msg.linear_acceleration_covariance[0] = 0.01;
        msg.linear_acceleration_covariance[4] = 0.01;
        msg.linear_acceleration_covariance[8] = 0.01;

        zbus_chan_pub(&imu_channel, &msg, K_NO_WAIT);

        k_sleep(K_MSEC(100));
    }
}

K_THREAD_DEFINE(
    accel_thread_id,
    ACCEL_THREAD_STACK,
    accel_thread,
    NULL, NULL, NULL,
    ACCEL_THREAD_PRIO,
    0,
    0);

int accel_driver_init(void)
{
    mpu6050 = DEVICE_DT_GET_ONE(invensense_mpu6050);

    if (!device_is_ready(mpu6050))
    {
        return -1;
    }

    return 0;
}
