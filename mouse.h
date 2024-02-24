#ifndef MOUSE_H
#define MOUSE_H

#include "btstack.h"
#include <pico/async_context.h>

class MOUSE
{
private:
    btstack_packet_callback_registration_t hci_event_callback_registration;
    btstack_packet_callback_registration_t l2cap_event_callback_registration;
    btstack_packet_callback_registration_t sm_event_callback_registration;
    hci_con_handle_t con_handle;
    uint8_t protocol_mode;

    int8_t          dx_;
    int8_t          dy_;
    uint8_t         buttons_;
    int8_t          wheel_;

    uint8_t         battery_;

    static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size);
    void mousing_can_send_now(void);
    void send_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);

    static MOUSE        *singleton_;            // Singleton mouse instance pointer

    MOUSE();

public:
    static MOUSE *get();
    bool init(async_context_t *context);
    bool is_connectd() const { return con_handle != HCI_CON_HANDLE_INVALID; }

    void action(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel);
};

#endif