#ifndef MOUSE_H
#define MOUSE_H

#include "btstack.h"
#include <pico/async_context.h>

#include <list>
#include <string>

class MOUSE
{
private:
    btstack_packet_callback_registration_t hci_event_callback_registration;
    btstack_packet_callback_registration_t l2cap_event_callback_registration;
    btstack_packet_callback_registration_t sm_event_callback_registration;
    hci_con_handle_t con_handle;
    uint8_t protocol_mode;

    uint8_t         battery_;

    class REPORT
    {
    private:
        enum _type {RPT_MOUSE, RPT_KEYSTROKE, RPT_KEYUP} type_;
        int8_t          dx_;
        int8_t          dy_;
        uint8_t         buttons_;
        int8_t          wheel_;
        uint8_t         keycode_;

    public:
        REPORT() : type_(RPT_KEYUP), dx_(0), dy_(0), buttons_(0), wheel_(0), keycode_(0) {}
        REPORT(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel) : type_(RPT_MOUSE), dx_(dx), dy_(dy), buttons_(buttons), wheel_(wheel), keycode_(0) {}
        REPORT(uint8_t keycode, uint8_t modifier) : type_(RPT_KEYSTROKE), dx_(0), dy_(0), buttons_(modifier), wheel_(0), keycode_(keycode) {}

        bool add_mouse(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel);
        bool is_finished();
        bool get_report(uint16_t &report_id, uint8_t *buffer, size_t buflen, uint16_t &rptsize);
    };
    std::list<REPORT> reports_;

    static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t packet_size);
    void mousing_can_send_now(void);
    void send_report(uint16_t report_id, uint8_t *buffer, uint16_t bufsiz);

    hids_device_report_t storage_[6];
#define NUM_REPORTS (sizeof(storage_) / sizeof(storage_[0]))

    void (*message_callback_)(const std::string &msg);
    void send_web_message(const std::string &key, const std::string &value);

    static MOUSE        *singleton_;            // Singleton mouse instance pointer

    MOUSE();

public:
    static MOUSE *get();
    bool init(async_context_t *context);
    bool is_connectd() const { return con_handle != HCI_CON_HANDLE_INVALID; }

    void action(int8_t dx, int8_t dy, uint8_t buttons, int8_t wheel);
    void keystroke(uint8_t ch);

    void set_message_callback(void(*cb)(const std::string &msg)) { message_callback_ = cb; }
};

#endif