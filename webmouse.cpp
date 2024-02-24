#include <stdio.h>
#include <stdlib.h>
#include "btstack_event.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hal_led.h"
#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#include "mouse.h"
#include "web.h"
#include "txt.h"

#include <stdexcept>

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

// Demo Application

#ifdef HAVE_BTSTACK_STDIN

static const int MOUSE_SPEED = 8;

// On systems with STDIN, we can directly type on the console

static void stdin_process(char character)
{
    MOUSE *mouse = MOUSE::get();
    if (!mouse->is_connectd()) 
    {
        printf("Mouse not connected, ignoring '%c'\n", character);
        return;
    }

    int8_t dx = 0;
    int8_t dy = 0;
    uint8_t buttons = 0;

    switch (character){
        case 'D':
            dx -= MOUSE_SPEED;
            break;
        case 'B':
            dy += MOUSE_SPEED;
            break;
        case 'C':
            dx += MOUSE_SPEED;
            break;
        case 'A':
            dy -= MOUSE_SPEED;
            break;
        case 'l':
            buttons |= 1;
            break;
        case 'r':
            buttons |= 2;
            break;
        default:
            return;
    }
    mouse->action(dx, dy, buttons, 0);
    if (buttons != 0)
    {
        mouse->action(0, 0, 0, 0);
    }
}

#endif

void web_message(const std::string &msg)
{
    MOUSE *mouse = MOUSE::get();
    if (!mouse->is_connectd()) 
    {
        printf("Mouse not connected, ignoring\n");
        return;
    }

    int8_t dx = 0;
    int8_t dy = 0;
    uint8_t buttons = 0;
    int8_t wheel;

    std::vector<std::string> tok;
    TXT::split(msg, " ", tok);
    for (auto it = tok.cbegin(); it != tok.cend(); ++it)
    {
        std::vector<std::string> val;
        TXT::split(*it, "=", val);
        std::string name = val.at(0);
        int value = 0;
        try
        {
            value = std::stoi(val.at(1));
        }
        catch (std::invalid_argument)
        {
            printf("Invalid value %s\n", it->c_str());
        }
        catch (std::out_of_range)
        {
            printf("Invalid value %s\n", it->c_str());
        }

        if (value > 127)
        {
            value = 127;
        }
        else if (value < -127)
        {
            value = -127;
        }

        if (name == "x")
        {
            dx = value;
        }
        else if (name == "y")
        {
            dy = value;
        }
        else if (name == "l")
        {
            if (value != 0)
            {
                buttons |= 1;
            }
        }
        else if (name == "r")
        {
            if (value != 0)
            {
                buttons |= 2;
            }
        }
        else if (name == "w")
        {
            wheel = value;
        }
    }

    mouse->action(dx, dy, buttons, wheel);
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

    printf("mouse\n");
    MOUSE *mouse = MOUSE::get();
    mouse->init(nullptr);
 
#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    printf("web\n");
    WEB *web = WEB::get();
    web->init();
    web->set_message_callback(web_message);

    printf("webmouse loop\n");
    btstack_run_loop_execute();

}