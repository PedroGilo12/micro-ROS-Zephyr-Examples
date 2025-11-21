#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <string.h>

#include "microros_thread.h"
#include "accel_driver.h"

#define LOG_MODULE_NAME app_wifi_microros
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static struct net_mgmt_event_callback wifi_cb;
static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                               uint32_t mgmt_event, struct net_if *iface)
{
    struct wifi_status_report msg = {
        .connected = false
    };

    switch (mgmt_event) {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            msg.connected = true;
            zbus_chan_pub(&wifi_status_channel, &msg, K_NO_WAIT);
            break;
        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            msg.connected = false;
            zbus_chan_pub(&wifi_status_channel, &msg, K_NO_WAIT);
            break;
        case NET_EVENT_WIFI_IFACE_STATUS:
            break;
        default:
            break;
    }
}

int main(void)
{
    accel_driver_init();

    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT |
                                 NET_EVENT_WIFI_IFACE_STATUS);
    net_mgmt_add_event_callback(&wifi_cb);

    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params wifi_params = {
        .ssid = CONFIG_WIFI_SSID,
        .ssid_length = strlen(CONFIG_WIFI_SSID),
        .psk = CONFIG_WIFI_PASSWORD,
        .psk_length = strlen(CONFIG_WIFI_PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
