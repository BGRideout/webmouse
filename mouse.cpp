/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "hid_mouse_demo.c"

// *****************************************************************************
/* EXAMPLE_START(hid_mouse_demo): HID Mouse Classic
 *
 * @text This HID Device example demonstrates how to implement
 * an HID keyboard. Without a HAVE_BTSTACK_STDIN, a fixed demo text is sent
 * If HAVE_BTSTACK_STDIN is defined, you can type from the terminal
 */
// *****************************************************************************

#include "mouse.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

MOUSE *MOUSE::singleton_ = nullptr;

MOUSE::MOUSE() : hid_cid(0), hid_boot_device(0), dx_(0), dy_(0), buttons_(0)
{

}

MOUSE *MOUSE::get()
{
    if (!singleton_)
    {
        singleton_ = new MOUSE();
    }
    return singleton_;
}

bool MOUSE::init(async_context_t *context)
{
    //btstack_run_loop_async_context_get_instance(context);

    // allow to get found by inquiry
    gap_discoverable_control(1);
    // use Limited Discoverable Mode; Peripheral; Pointing Device as CoD
    gap_set_class_of_device(0x2580);
    // set local name to be identified - zeroes will be replaced by actual BD ADDR
    gap_set_local_name("HID Mouse Demo 00:00:00:00:00:00");
    // allow for role switch in general and sniff mode
    gap_set_default_link_policy_settings( LM_LINK_POLICY_ENABLE_ROLE_SWITCH | LM_LINK_POLICY_ENABLE_SNIFF_MODE );
    // allow for role switch on outgoing connections - this allow HID Host to become master when we re-connect to it
    gap_set_allow_role_switch(true);

    // L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // SDP Server
    sdp_init();
    memset(hid_service_buffer, 0, sizeof(hid_service_buffer));
    
    uint8_t hid_virtual_cable = 0;
    uint8_t hid_remote_wake = 1;
    uint8_t hid_reconnect_initiate = 1;
    uint8_t hid_normally_connectable = 1;

    hid_sdp_record_t hid_params = {
        // hid sevice subclass 2580 Mouse, hid counntry code 33 US
        0x2580, 33, 
        hid_virtual_cable, hid_remote_wake, 
        hid_reconnect_initiate,
        (hid_normally_connectable != 0),
        (hid_boot_device != 0), 
        0xFFFF, 0xFFFF, 3200,
        hid_descriptor_mouse_boot_mode,
        sizeof(hid_descriptor_mouse_boot_mode), 
        hid_device_name
    };
    
    hid_create_sdp_record(hid_service_buffer, 0x10001, &hid_params);

    printf("SDP service record size: %u\n", de_get_len( hid_service_buffer));
    sdp_register_service(hid_service_buffer);

    // HID Device
    hid_device_init(hid_boot_device, sizeof(hid_descriptor_mouse_boot_mode), hid_descriptor_mouse_boot_mode);
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for HID
    hid_device_register_packet_handler(&packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}


// from USB HID Specification 1.1, Appendix B.2
uint8_t MOUSE::hid_descriptor_mouse_boot_mode[50] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)

    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)

    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)

    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    //0x09, 0x38,                    //     USAGE(Wheel)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)

    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};

// HID Report sending
void MOUSE::send_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel){
    uint8_t report[] = { 0xa1, buttons, (uint8_t) dx, (uint8_t) dy /*, (uint8_t)wheel */};
    hid_device_send_interrupt_message(hid_cid, &report[0], sizeof(report));
    printf("Mouse: %d/%d - buttons: %02x - wheel %d\n", dx, dy, buttons, wheel);
}

void MOUSE::mousing_can_send_now(void)
{
    send_report(buttons_, dx_, dy_, wheel_);
    // reset
    dx_ = 0;
    dy_ = 0;
    wheel_ = 0;
}

void MOUSE::action(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel)
{
    dx_ += dx;
    dy_ += dy;
    buttons_ = buttons;
    wheel_ += wheel;
    hid_device_request_can_send_now_event(hid_cid);
}

void MOUSE::packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size){
    UNUSED(channel);
    UNUSED(packet_size);
    MOUSE *mouse = get();
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // ssp: inform about user confirmation request
                    log_info("SSP User Confirmation Request with numeric value '%06" PRIu32 "'\n", hci_event_user_confirmation_request_get_numeric_value(packet));
                    log_info("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){
                        case HID_SUBEVENT_CONNECTION_OPENED:
                            if (hid_subevent_connection_opened_get_status(packet) != ERROR_CODE_SUCCESS) return;
                            mouse->hid_cid = hid_subevent_connection_opened_get_hid_cid(packet);
#ifdef HAVE_BTSTACK_STDIN
                            printf("HID Connected, control mouse using arrow keys for movement and 'l' and 'r' for buttons...\n");
#else
                            printf("HID Connected, simulating mouse movements...\n");
                            hid_embedded_start_mousing();
#endif
                            break;
                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            printf("HID Disconnected\n");
                            mouse->hid_cid = 0;
                            break;
                        case HID_SUBEVENT_CAN_SEND_NOW:
                            mouse->mousing_can_send_now();
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

/* LISTING_END */
/* EXAMPLE_END */


