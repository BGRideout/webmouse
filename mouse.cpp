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
#include "keycode.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "btstack.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"

#include "mouse_att.h"
#include "hci_dump_embedded_stdout.h"

#define MOUSE_REPORT_ID 1
#define KEYBOARD_REPORT_ID 2

MOUSE *MOUSE::singleton_ = nullptr;

MOUSE::MOUSE() : con_handle(HCI_CON_HANDLE_INVALID), protocol_mode(1), battery_(100)
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
    // setup l2cap and
    l2cap_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    // sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // setup battery service
    battery_service_server_init(battery_);

    // setup device information service
    device_information_service_server_init();

    // setup HID Device service
    // from USB HID Specification 1.1, Appendix B.2
    static uint8_t hid_descriptor_mouse[] =
    {
        //  Mouse
        0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
        0x09, 0x02,                    // USAGE (Mouse)
        0xa1, 0x01,                    // COLLECTION (Application)

        0x85, MOUSE_REPORT_ID,         // Report ID
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
        0x09, 0x38,                    //     USAGE(Wheel)
        0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
        0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
        0x75, 0x08,                    //     REPORT_SIZE (8)
        0x95, 0x03,                    //     REPORT_COUNT (3)
        0x81, 0x06,                    //     INPUT (Data,Var,Rel)

        0xc0,                          //   END_COLLECTION
        0xc0,                          // END_COLLECTION

        //  Keyboard
        0x05, 0x01,                    // Usage Page (Generic Desktop)
        0x09, 0x06,                    // Usage (Keyboard)
        0xa1, 0x01,                    // Collection (Application)

        0x85,  KEYBOARD_REPORT_ID,     // Report ID

        // Modifier byte

        0x75, 0x01,                    //   Report Size (1)
        0x95, 0x08,                    //   Report Count (8)
        0x05, 0x07,                    //   Usage Page (Key codes)
        0x19, 0xe0,                    //   Usage Minimum (Keyboard LeftControl)
        0x29, 0xe7,                    //   Usage Maxium (Keyboard Right GUI)
        0x15, 0x00,                    //   Logical Minimum (0)
        0x25, 0x01,                    //   Logical Maximum (1)
        0x81, 0x02,                    //   Input (Data, Variable, Absolute)

        // Reserved byte

        0x75, 0x01,                    //   Report Size (1)
        0x95, 0x08,                    //   Report Count (8)
        0x81, 0x03,                    //   Input (Constant, Variable, Absolute)

        // LED report + padding

        0x95, 0x05,                    //   Report Count (5)
        0x75, 0x01,                    //   Report Size (1)
        0x05, 0x08,                    //   Usage Page (LEDs)
        0x19, 0x01,                    //   Usage Minimum (Num Lock)
        0x29, 0x05,                    //   Usage Maxium (Kana)
        0x91, 0x02,                    //   Output (Data, Variable, Absolute)

        0x95, 0x01,                    //   Report Count (1)
        0x75, 0x03,                    //   Report Size (3)
        0x91, 0x03,                    //   Output (Constant, Variable, Absolute)

        // Keycodes

        0x95, 0x06,                    //   Report Count (6)
        0x75, 0x08,                    //   Report Size (8)
        0x15, 0x00,                    //   Logical Minimum (0)
        0x25, 0xff,                    //   Logical Maximum (1)
        0x05, 0x07,                    //   Usage Page (Key codes)
        0x19, 0x00,                    //   Usage Minimum (Reserved (no event indicated))
        0x29, 0xff,                    //   Usage Maxium (Reserved)
        0x81, 0x00,                    //   Input (Data, Array)

        0xc0,                          // End collection
    };

    //hci_dump_init(hci_dump_embedded_stdout_get_instance());
    //att_dump_attributes();
    memset(storage_, 0, sizeof(storage_));
    hids_device_init_with_storage(0, hid_descriptor_mouse, sizeof(hid_descriptor_mouse), NUM_REPORTS, storage_);
    for(int ii = 0; ii < NUM_REPORTS; ii++)
    {
        printf("%d type: %d id: %d size: %d\n", ii, storage_[ii].type, storage_[ii].id, storage_[ii].size);
    }
 
    // setup advertisements
    static uint8_t adv_data[] =
    {
        // Flags general discoverable, BR/EDR not supported
        0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
        // Name
        0x0a, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'W', 'e', 'b', ' ', 'M', 'o', 'u', 's', 'e',
        // 16-bit Service UUIDs
        0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
        // Appearance HID - Mouse (Category 15, Sub-Category 2)
        0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC2, 0x03,
        // Appearance HID - Keyboard (Category 15, Sub-Category 1)
        //0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC1, 0x03,
};
    uint8_t adv_data_len = sizeof(adv_data);

    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // register for events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for connection parameter updates
    l2cap_event_callback_registration.callback = &packet_handler;
    l2cap_add_event_handler(&l2cap_event_callback_registration);

    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hids_device_register_packet_handler(packet_handler);

    hci_power_control(HCI_POWER_ON);
    return 0;
}




// HID Report sending
void MOUSE::send_report(uint16_t report_id, uint8_t *buffer, uint16_t bufsiz)
{
    uint8_t sts = -1;
    switch (protocol_mode)
    {
        case 0:
            if (bufsiz == 4)
            {
                sts = hids_device_send_boot_mouse_input_report(con_handle, buffer, bufsiz - 1);
            }
            else
            {
                sts = hids_device_send_boot_keyboard_input_report(con_handle, buffer, bufsiz);
            }
            break;
        case 1:
            sts = hids_device_send_input_report_for_id(con_handle, report_id, buffer, bufsiz);
            break;
        default:
            break;
    }
    printf("protocol: %d sts: %d report(%d):", protocol_mode, sts, report_id);
    for (int ii = 0; ii < bufsiz; ii++)
    {
        printf(" %2.2x", buffer[ii]);
    }
    printf("\n");
}

void MOUSE::mousing_can_send_now(void)
{
    if (reports_.size() > 0)
    {
        uint16_t report_id;
        uint8_t  buffer[8];
        uint16_t buflen;
        if (reports_.front().get_report(report_id, buffer, sizeof(buffer), buflen))
        {
            send_report(report_id, buffer, buflen);
            if (reports_.front().is_finished())
            {
                reports_.pop_front();
            }
        }
        else
        {
            printf("Invalid report in queue\n");
            reports_.pop_front();
        }
    }
    if (reports_.size() > 0)
    {
        hids_device_request_can_send_now_event(con_handle);
    }
}

void MOUSE::action(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel)
{
    if (reports_.size() == 0 || !reports_.back().add_mouse(dx, dy, buttons, wheel))
    {
        reports_.emplace_back(REPORT(dx, dy, buttons, wheel));
    }
    hids_device_request_can_send_now_event(con_handle);
}

void MOUSE::keystroke(uint8_t ch)
{
    uint8_t keycode;
    uint8_t modifier;
    if (KEYCODE::get_code_and_modifier(ch, keycode, modifier))
    {
        reports_.emplace_back(REPORT(keycode, modifier));
        hids_device_request_can_send_now_event(con_handle);
    }
    else
    {
        printf("Unsupported character %2.2x\n", ch);
    }
}

void MOUSE::packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size)
{
    UNUSED(channel);
    UNUSED(packet_size);
    uint16_t conn_interval;

    if (packet_type != HCI_EVENT_PACKET) return;

    MOUSE *mouse = get();
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            mouse->con_handle = HCI_CON_HANDLE_INVALID;
            printf("Disconnected\n");
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %" PRIu32 "\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %" PRIu32 "\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            printf("L2CAP Connection Parameter Update Complete, response: %x\n", l2cap_event_connection_parameter_update_response_get_result(packet));
            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                    printf("LE Connection Complete:\n");
                    printf("- Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    printf("- Connection Latency: %u\n", hci_subevent_le_connection_complete_get_conn_latency(packet));
                    break;
                case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                    // print connection parameters (without using float operations)
                    conn_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                    printf("LE Connection Update:\n");
                    printf("- Connection Interval: %u.%02u ms\n", conn_interval * 125 / 100, 25 * (conn_interval & 3));
                    printf("- Connection Latency: %u\n", hci_subevent_le_connection_update_complete_get_conn_latency(packet));
                    break;
                default:
                    break;
            }
            break;  
        case HCI_EVENT_HIDS_META:
            switch (hci_event_hids_meta_get_subevent_code(packet)){
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    mouse->con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
                    printf("Report Characteristic Subscribed %u handle %x\n", hids_subevent_input_report_enable_get_enable(packet), mouse->con_handle);

                    // request connection param update via L2CAP following Apple Bluetooth Design Guidelines
                    // gap_request_connection_parameter_update(con_handle, 12, 12, 4, 100);    // 15 ms, 4, 1s

                    // directly update connection params via HCI following Apple Bluetooth Design Guidelines
                    // gap_update_connection_parameters(con_handle, 12, 12, 4, 100);    // 60-75 ms, 4, 1s

                    break;
                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    mouse->con_handle = hids_subevent_boot_keyboard_input_report_enable_get_con_handle(packet);
                    printf("Boot Keyboard Characteristic Subscribed %u\n", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_BOOT_MOUSE_INPUT_REPORT_ENABLE:
                    mouse->con_handle = hids_subevent_boot_mouse_input_report_enable_get_con_handle(packet);
                    printf("Boot Mouse Characteristic Subscribed %u\n", hids_subevent_boot_mouse_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    mouse->protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                    printf("Protocol Mode: %s mode\n", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    mouse->mousing_can_send_now();
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

bool MOUSE::REPORT::add_mouse(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel)
{
    bool ret = (type_ == RPT_MOUSE) && (buttons == buttons_) && (dx + dx_ < 127) && (dy + dy_ < 127) && (wheel + wheel_ < 127);
    if (ret)
    {
        dx_ += dx;
        dy_ += dy;
        wheel_ += wheel;
    }
    return ret;
}

bool MOUSE::REPORT::is_finished()
{
    bool ret = true;
    if (type_ == RPT_KEYSTROKE)
    {
        type_ = RPT_KEYUP;
        ret = false;
    }
    return ret;
}

bool MOUSE::REPORT::get_report(uint16_t &report_id, uint8_t *buffer, size_t buflen, uint16_t &rptsize)
{
    bool ret = false;
    report_id = 0;
    rptsize = 0;
    int ii = 0;
    if (type_ == RPT_MOUSE)
    {
        if (buflen >= 4)
        {
            report_id = MOUSE_REPORT_ID;
            buffer[ii++] = buttons_;
            buffer[ii++] = (uint8_t)dx_;
            buffer[ii++] = (uint8_t)dy_;
            buffer[ii++] = (uint8_t)(wheel_ / 16);
            rptsize = 4;
            ret = true;
        }
    }
    else if (type_ == RPT_KEYSTROKE)
    {
        if (buflen >= 8)
        {
            report_id = KEYBOARD_REPORT_ID;
            buffer[ii++] = buttons_;
            buffer[ii++] = 0;
            buffer[ii++] = keycode_;
            rptsize = 8;
            ret = true;
        }
    }
    else if (type_ == RPT_KEYUP)
    {
        if (buflen >= 8)
        {
            report_id = KEYBOARD_REPORT_ID;
            rptsize = 8;
            ret = true;
        }
    }
    for (; ii < buflen; ii++)
    {
        buffer[ii] = 0;
    }
    return ret;
}
