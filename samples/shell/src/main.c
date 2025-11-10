#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <microros_transports.h>

#include <zephyr/shell/shell.h>

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){LOG_ERR("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);for(;;){};}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){LOG_ERR("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}
#define WIFI_CONNECTED_EVENT BIT(0)
#define LED0_NODE DT_ALIAS(led0)
#define POST_WIFI_STACK_SIZE 8192

LOG_MODULE_REGISTER(my_wifi_app);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static struct k_event wifi_event;

static rcl_publisher_t publisher;
static std_msgs__msg__Int32 msg;

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	RCLC_UNUSED(last_call_time);
	if (timer != NULL) {
		RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
		msg.data++;
	}
}

static int cmd_socket_set_ip(const struct shell *sh, size_t argc, char **argv)
{
    const char *ip_str = argv[1];

    strncpy(default_params.ip, ip_str, sizeof(default_params.ip) - 1);
    default_params.ip[sizeof(default_params.ip) - 1] = '\0';

    shell_print(sh, "micro-ROS agent IP set to: %s", default_params.ip);

    return 0;
}

static int cmd_socket_set_port(const struct shell *sh, size_t argc, char **argv)
{
    const char *port_str = argv[1];

    strncpy(default_params.port, port_str, sizeof(default_params.port) - 1);
    default_params.port[sizeof(default_params.port) - 1] = '\0';

    shell_print(sh, "micro-ROS agent port set to: %s", default_params.port);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_socket_cfg,
    SHELL_CMD_ARG(set_ip, NULL,
        "Set the IP address. Usage: set_ip <ip_addr>",
        cmd_socket_set_ip, 2, 0),
    SHELL_CMD_ARG(set_port, NULL,
        "Set the port. Usage: set_port <port>",
        cmd_socket_set_port, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(socket_cfg,   
                   &sub_socket_cfg,  
                   "Commands to configure the socket", 
                   NULL);

void post_wifi_thread(void *arg1, void *arg2, void *arg3)
{
    int ret;
    bool led_state = true;
    uint32_t events;
    rcl_ret_t rcl_err;

    while (1) {
        events = k_event_wait(&wifi_event, WIFI_CONNECTED_EVENT, false, K_FOREVER);

        if (events & WIFI_CONNECTED_EVENT) {
            LOG_INF("Wi-Fi connected!");

            rmw_uros_set_custom_transport(
                false,
                (void *)&default_params,
                zephyr_transport_open,
                zephyr_transport_close,
                zephyr_transport_write,
                zephyr_transport_read
            );

            if (!gpio_is_ready_dt(&led)) {
                LOG_ERR("LED not ready!");
                continue;
            }

            ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
            if (ret < 0) {
                LOG_ERR("Error setting up LED: %d", ret);
                continue;
            }

            rcl_allocator_t allocator = rcl_get_default_allocator();
            rclc_support_t support;
            
            rcl_err = rclc_support_init(&support, 0, NULL, &allocator);
            if((rcl_err != RCL_RET_OK)){
                LOG_ERR("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)rcl_err);
            }

            rcl_node_t node;
            RCCHECK(rclc_node_init_default(&node, "zephyr_int32_publisher", "", &support));

            ret = gpio_pin_toggle_dt(&led);
            if (ret < 0) {
                LOG_ERR("Error blinking LED: %d", ret);
            }

            RCCHECK(rclc_publisher_init_default(
                &publisher,
                &node,
                ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
                "zephyr_int32_publisher"));

            rcl_timer_t timer;
            const unsigned int timer_timeout = 1000;
            RCCHECK(rclc_timer_init_default(
                &timer,
                &support,
                RCL_MS_TO_NS(timer_timeout),
                timer_callback));

            rclc_executor_t executor;
            RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
            RCCHECK(rclc_executor_add_timer(&executor, &timer));

            msg.data = 0;
            
            while(1){
                ret = gpio_pin_toggle_dt(&led);
                if (ret < 0) {
                    return 0;
                }

                led_state = !led_state;

                rclc_executor_spin_some(&executor, 100);
                k_sleep(K_MSEC(100));
            }
        }
    }
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                               uint32_t mgmt_event, struct net_if *iface)
{
    switch (mgmt_event) {
        case NET_EVENT_WIFI_CONNECT_RESULT:
            LOG_INF("Wi-Fi started successfully!");
            k_event_post(&wifi_event, WIFI_CONNECTED_EVENT);
            break;

        case NET_EVENT_WIFI_DISCONNECT_RESULT:
            LOG_INF("Callback: Wi-Fi disconnected!");
            break;

        default:
            break;
    }
}

static struct net_mgmt_event_callback wifi_cb;

K_THREAD_STACK_DEFINE(post_wifi_stack, POST_WIFI_STACK_SIZE);
struct k_thread post_wifi_thread_data;

int main(void)
{
    k_event_init(&wifi_event);

    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);

    LOG_INF("Shell ready. Use 'wifi connect <SSID> <PASS>' to connect.");

    k_thread_create(&post_wifi_thread_data, post_wifi_stack,
                    POST_WIFI_STACK_SIZE,
                    post_wifi_thread,
                    NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
