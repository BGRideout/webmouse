//                  *****  Webmouse Class  *****

#ifndef WEBMOUSE_H
#define WEBMOUSE_H

#include <stdint.h>
#include <string>
#include "web.h"
#include "button.h"

class LED;
class Logger;
class Button;

class WEBMOUSE
{
private:

    LED             *led_;                  // Indicator LED
    int             bit_time_;              // Flash bit time

    struct PATTERN                          // LED patterns
    {
        uint32_t    count;
        uint32_t    pattern;
    };

    struct PATTERN INIT_PATTERN {2, 0x1};
    struct PATTERN HEADER_PATTERN {3, 0x7};
    struct PATTERN WIFI_AP_PATTERN {4, 0x7};
    struct PATTERN WIFI_STA_PATTERN {6, 0x17};
    struct PATTERN MOUSE_PATTERN {8, 0x57};

    Button          *apbtn_;                // Button to start AP mode
    Logger          *log_;                  // Logger

    int wifi_ap;                            // Access point status
    int wifi_sta;                           // WiFI station status
    int ble;                                // Bluetooth status

    void mouse_init();
    void web_init();

    void send_state(ClientHandle client=0);
    void send_title(ClientHandle client=0);
    void send_wifi(ClientHandle client=0);

    void state_callback(int state);
    static void state_callback(int state, void *udata)
        {WEBMOUSE *self = static_cast<WEBMOUSE *>(udata); self->state_callback(state);}

    void mouse_message(const std::string &msg);
    static void mouse_message(const std::string &msg, void *user_data)
        {WEBMOUSE *self = static_cast<WEBMOUSE *>(user_data); self->mouse_message(msg);}

    bool http_request(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close);
    static bool http_request(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close, void *user_data)
        {WEBMOUSE *self = static_cast<WEBMOUSE *>(user_data); return self->http_request(web, client, rqst, close);}

    bool http_get(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close);
    bool http_post(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close);
    
    void web_message(WEB *web, ClientHandle client, const std::string &msg);
    static void web_message(WEB *web, ClientHandle client, const std::string &msg, void *user_data)
        {WEBMOUSE *self = static_cast<WEBMOUSE *>(user_data); self->web_message(web, client, msg);}

    bool scan_complete(WEB *web, ClientHandle client, const WiFiScanData &data);
    static bool scan_complete(WEB *web, ClientHandle client, const WiFiScanData &data, void *user_data)
        {WEBMOUSE *self = static_cast<WEBMOUSE *>(user_data); return self->scan_complete(web, client, data);}
    
    static void button_event(struct Button::ButtonEvent &ev, void *user_data);

    static bool tls_callback(WEB *web, std::string &cert, std::string &pkey, std::string &pkpass);

public:
    WEBMOUSE();

    void run();
};

#endif
