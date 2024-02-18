#include <stdio.h>
#include <stdlib.h>
#include "btstack_event.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hal_led.h"
#include "btstack.h"

#include "mouse.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet))
    {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            printf("BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            break;
        default:
            break;
    }
}


int main(int argc, const char *argv[])
{
    stdio_init_all();
    printf("webmouse\n");

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }
    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    printf("webmouse main\n");
    btstack_main(argc, argv);
    printf("webmouse loop\n");
    btstack_run_loop_execute();

}