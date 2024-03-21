#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "mouse.h"
#include "web.h"
#include "txt.h"
#include "config.h"
#include "led.h"

#include <stdexcept>

#define INIT_PATTERN {0, 0, 0, 0, 0, 1}
#define HEADER_PATTERN {1, 1, 1}
#define WIFI_AP_PATTERN {1, 1, 1, 0}
#define WIFI_STA_PATTERN {1, 1, 1, 0, 1, 0}
#define MOUSE_PATTERN {1, 1, 1, 0, 1, 0, 1, 0}

static int wifi_ap = 0;
static int wifi_sta = -1;
static int ble = -1;

void send_state()
{
    std::string msg("{\"ap\":\"<wifi_ap>\", \"wifi\":\"<wifi_sta>\", \"mouse\":\"<ble>\"}");
    TXT::substitute(msg, "<wifi_ap>", std::to_string(wifi_ap));
    TXT::substitute(msg, "<wifi_sta>", std::to_string(wifi_sta));
    TXT::substitute(msg, "<ble>", std::to_string(ble));
    WEB::get()->broadcast_websocket(msg);
}

void state_callback(int state)
{
    bool change = false;
    int newval;
    switch (state)
    {
    case WEB::STA_INITIALIZING:
    case WEB::STA_DISCONNECTED:
        newval = 1;
        change = newval != wifi_sta;
        if (change) wifi_sta = newval;
        break;
    
    case WEB::STA_CONNECTED:
        newval = 0;
        change = newval != wifi_sta;
        if (change) wifi_sta = newval;
        break;

    case WEB::AP_ACTIVE:
        newval = 1;
        change = newval != wifi_ap;
        if (change) wifi_ap = newval;
        break;

    case WEB::AP_INACTIVE:
        newval = 0;
        change = newval != wifi_ap;
        if (change) wifi_ap = newval;
        break;

    case MOUSE::MOUSE_ACTIVE:
        newval = 0;
        change = newval != ble;
        if (change) ble = newval;
        break;

    case MOUSE::MOUSE_INACTIVE:
        newval = 1;
        change = newval != ble;
        if (change) ble = newval;
        break;

    default:
        break;
    }

    if (change)
    {
        LED *led = LED::get();
        led->begin_pattern_update();
        led->add_to_pattern(HEADER_PATTERN);
        if (wifi_ap) led->add_to_pattern(WIFI_AP_PATTERN);
        if (wifi_sta) led->add_to_pattern(WIFI_STA_PATTERN);
        if (ble) led->add_to_pattern(MOUSE_PATTERN);
        led->end_pattern_update();
        printf("LED pattern %d %d %d\n", wifi_ap, wifi_sta, ble);
        send_state();
    }
}

void mouse_message(const std::string &msg)
{
    WEB::get()->broadcast_websocket(msg);
}

void web_message(const std::string &msg)
{
    std::string func;
    std::string code;

    int8_t dx = 0;
    int8_t dy = 0;
    uint8_t buttons = 0;
    int8_t wheel;

    uint8_t ch = 0;
    uint8_t ctrl = 0;
    uint8_t alt = 0;
    uint8_t shift = 0;

    std::vector<std::string> tok;
    TXT::split(msg, " ", tok);
    for (auto it = tok.cbegin(); it != tok.cend(); ++it)
    {
        std::vector<std::string> val;
        TXT::split(*it, "=", val);
        std::string name = val.at(0);
        if (name == "func")
        {
            func = val.at(1);
        }
        else if (func == "mouse" || func == "keyboard")
        {
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
            int rawvalue = value;

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
            else if (name == "c")
            {
                if (rawvalue < 0xff)
                {
                    ch = rawvalue;
                }
            }
            else if (name == "ctrl")
            {
                ctrl = value;
            }
            else if (name == "alt")
            {
                alt = value;
            }
            else if (name == "shift")
            {
                shift = value;
            }
        }
        else if (func == "av_control")
        {
            if (name == "code")
            {
                code = val.at(1);
            }
        }
    }

    if (func == "mouse")
    {
        MOUSE::get()->action(dx, dy, buttons, wheel);
    }
    else if (func == "keyboard")
    {
        MOUSE::get()->keystroke(ch, ctrl, alt, shift);
    }
    else if (func == "av_control")
    {
        MOUSE::get()->av_control(code);
    }
    else if (func == "get_state")
    {
        send_state();
        MOUSE::get()->send_led_status();
    } 
}

int main(int argc, const char *argv[])
{
    stdio_init_all();
    printf("webmouse\n");

    CONFIG::get()->init();

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }

    // Initialize LED
    LED::get()->set_flash(INIT_PATTERN);

    printf("mouse\n");
    MOUSE *mouse = MOUSE::get();
    mouse->set_notice_callback(state_callback);
    mouse->set_message_callback(mouse_message);
    mouse->init();

    printf("web\n");
    WEB *web = WEB::get();
    web->set_notice_callback(state_callback);
    web->set_message_callback(web_message);
    web->init();

    printf("webmouse loop\n");
    mouse->run();
}