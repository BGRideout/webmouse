#ifndef MOUSE_H
#define MOUSE_H

#include "btstack.h"
#include <pico/async_context.h>

class MOUSE
{
private:
    uint8_t hid_service_buffer[250];
    const char hid_device_name[18] = "BTstack HID Mouse";
    btstack_packet_callback_registration_t hci_event_callback_registration;
    static uint8_t hid_descriptor_mouse_boot_mode[50];
    uint16_t hid_cid;
    int hid_boot_device;

    int8_t          dx_;
    int8_t          dy_;
    uint8_t         buttons_;

    static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size);
    void mousing_can_send_now(void);
    void send_report(uint8_t buttons, int8_t dx, int8_t dy);

    static MOUSE        *singleton_;            // Singleton mouse instance pointer

    MOUSE();

public:
    static MOUSE *get();
    bool init(async_context_t *context);
    bool is_connectd() const { return hid_cid != 0; }

    void action(int8_t dx, int8_t dy, uint8_t buttons);
};

#endif