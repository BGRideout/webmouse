#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <pfs.h>
#include <lfs.h>

#include "webmouse.h"
#include "mouse.h"
#include "web.h"
#include "web_files.h"
#include "button.h"
#include "led.h"
#include "txt.h"
#include "config.h"

#include <stdexcept>


//  File system definition
#define ROOT_OFFSET 0x110000
#ifndef PICO_FLASH_BANK_TOTAL_SIZE
#define ROOT_SIZE   (PICO_FLASH_SIZE_BYTES - ROOT_OFFSET)
#else
#define ROOT_SIZE   (PICO_FLASH_SIZE_BYTES - ROOT_OFFSET - PICO_FLASH_BANK_TOTAL_SIZE)       // Leave 8K for bluetooth
#endif

#define CERTFILENAME    "cert.pem"
#define KEYFILENAME     "key.pem"
#define PASSFILENAME    "pass.txt"

WEBMOUSE::WEBMOUSE() : bit_time_(150), wifi_ap(0), wifi_sta(-1), ble(-1)
{
    log_ = new Logger();
    log_->setDebug(0);

    apbtn_ = new Button(0, 3);
    apbtn_->setEventCallback(button_event, this);

    // Initialize LED
    led_ = new LED();
    led_->setFlashPeriod(INIT_PATTERN.count * bit_time_);
    led_->setFlashPattern(INIT_PATTERN.pattern, INIT_PATTERN.count);

    printf("web\n");
    WEB *web = WEB::get();
    web->setLogger(log_);
    web->set_notice_callback(state_callback, this);
    web->set_http_callback(http_request, this);
    web->set_message_callback(web_message, this);
    web->set_tls_callback(tls_callback);
    web->init();

    printf("mouse\n");
    MOUSE *mouse = MOUSE::get();
    mouse->set_notice_callback(state_callback, this);
    mouse->set_message_callback(mouse_message, this);
    mouse->init();
}

void WEBMOUSE::tls_callback(WEB *web, std::string &cert, std::string &pkey, std::string &pkpass)
{
#ifdef USE_HTTPS
    char    line[128];

    cert.clear();
    FILE *fd = fopen(CERTFILENAME, "r");
    if (fd)
    {
        while (fgets(line,sizeof(line), fd))
        {
            cert += line;
        }
        fclose(fd);
    }
    else
    {
        printf("Failed to open certificate file\n");
    }

    pkey.clear();
    fd = fopen(KEYFILENAME, "r");
    if (fd)
    {
        while (fgets(line,sizeof(line), fd))
        {
            pkey += line;
        }
        fclose(fd);
    }
    else
    {
        printf("Failed to open private key file\n");
    }

    pkpass.clear();
    fd = fopen(PASSFILENAME, "r");
    if (fd)
    {
        if (fgets(line,sizeof(line), fd))
        {
            pkpass += line;
        }
        fclose(fd);
    }
    else
    {
        printf("Failed to open passphrase file\n");
    }
#endif
}

void WEBMOUSE::run()
{
    CONFIG *cfg = CONFIG::get();
    WEB *web = WEB::get();
    if (strlen(cfg->ssid()) > 0)
    {
        web->connect_to_wifi(cfg->hostname(), cfg->ssid(), cfg->password());
    }

    MOUSE::get()->run();
}

void WEBMOUSE::send_state(ClientHandle client)
{
    std::string msg("{\"ap\":\"<wifi_ap>\", \"wifi\":\"<wifi_sta>\", \"mouse\":\"<ble>\"}");
    TXT::substitute(msg, "<wifi_ap>", std::to_string(wifi_ap));
    TXT::substitute(msg, "<wifi_sta>", std::to_string(wifi_sta));
    TXT::substitute(msg, "<ble>", std::to_string(ble));
    if (client != 0)
    {
        WEB::get()->send_message(client, msg);
    }
    else
    {
        WEB::get()->broadcast_websocket(msg);
    }
}

void WEBMOUSE::send_title(ClientHandle client)
{
    std::string msg("{\"title\":\"<title>\"}");
    TXT::substitute(msg, "<title>", CONFIG::get()->title());
    if (client != 0)
    {
        WEB::get()->send_message(client, msg);
    }
    else
    {
        WEB::get()->broadcast_websocket(msg);
    }
}

void WEBMOUSE::send_wifi(ClientHandle client)
{
    WEB *web = WEB::get();
    std::string msg = "{\"func\": \"wifi_resp\", \"host\": \"" + web->hostname() +
              "\", \"ssid\": \"" + web->wifi_ssid() + "\", \"ip\": \"" + web->ip_addr() +
              "\", \"http\": \"" + std::string(web->is_http_listening() ? "true" : "false") +
              "\", \"https\": \"" + std::string(web->is_https_listening() ? "true" : "false") +
#ifdef USE_HTTPS
              "\", \"https_ena\": \"true\"" + 
#else
              "\", \"https_ena\": \"false\"" + 
#endif
              "}";
    if (client != 0)
    {
        WEB::get()->send_message(client, msg);
    }
    else
    {
        WEB::get()->broadcast_websocket(msg);
    }
}

void WEBMOUSE::state_callback(int state)
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
        if (change)
        {
            wifi_ap = newval;
            WEB::get()->start_http();
            log_->print("AP enabled http. HTTP: %s, HTTPS: %s\n",
                        WEB::get()->is_http_listening() ? "Y" : "N", WEB::get()->is_https_listening() ? "Y" : "N");
        }
        break;

    case WEB::AP_INACTIVE:
        newval = 0;
        change = newval != wifi_ap;
        if (change)
        {
            wifi_ap = newval;
            WEB *web = WEB::get();
            if (web->is_https_listening())
            {
                web->stop_http();
            }
        }
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
        uint32_t    count = HEADER_PATTERN.count;
        uint32_t    pattern = HEADER_PATTERN.pattern;
        if (wifi_ap)
        {
            pattern |= WIFI_AP_PATTERN.pattern << count;
            count += WIFI_AP_PATTERN.count;
        }
        if (wifi_sta)
        {
            pattern |= WIFI_STA_PATTERN.pattern << count;
            count += WIFI_STA_PATTERN.count;
        }
        if (ble)
        {
            pattern |= MOUSE_PATTERN.pattern << count;
            count += MOUSE_PATTERN.count;
        }
        led_->setFlashPeriod(count * bit_time_);
        led_->setFlashPattern(pattern, count);
        printf("LED pattern %d %d %d\n", wifi_ap, wifi_sta, ble);
        send_state();
    }
}

void WEBMOUSE::mouse_message(const std::string &msg)
{
    WEB::get()->broadcast_websocket(msg);
}

bool WEBMOUSE::http_request(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;

    log_->print_debug(1, "%d HTTP %s %s\n", client, rqst.type().c_str(), rqst.url().c_str());
    if (rqst.type() == "GET")
    {
        ret = http_get(web, client, rqst, close);
    }
    else if (rqst.type() == "POST")
    {
        ret = http_post(web, client, rqst, close);
    }
    return ret;
}

bool WEBMOUSE::http_get(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    bool ret = false;
    std::string url = rqst.path();
    if (url == "/" || url.empty()) url = "/index.html";
    const char *data;
    u16_t datalen;
    if (url.length() > 0 && WEB_FILES::get()->get_file(url.substr(1), data, datalen))
    {
        web->send_data(client, data, datalen, WEB::STAT);
        close = false;
        ret = true;
    }
    return ret;
}

bool WEBMOUSE::http_post(WEB *web, ClientHandle client, HTTPRequest &rqst, bool &close)
{
    if (rqst.path() == "/config.html")
    {
        rqst.printPostData();

        std::string hostname = rqst.postValue("hostname");
        std::string ssid = rqst.postValue("ssid");
        std::string pwd = rqst.postValue("pwd");
        std::string title = rqst.postValue("title");

        bool change = false;
        CONFIG *cfg = CONFIG::get();
        if (cfg->hostname() != hostname)
        {
            cfg->set_hostname(hostname.c_str());
            change = true;
        }
        if (pwd.empty()) pwd = cfg->password();
        if (cfg->ssid() != ssid || cfg->password() != pwd)
        {
            cfg->set_wifi_credentials(ssid.c_str(), pwd.c_str());
            change = true;
        }
        if (cfg->title() != title)
        {
            cfg->set_title(title.c_str());
            send_title();
        }
        if (change && !ssid.empty())
        {
            WEB::get()->update_wifi(hostname, ssid, pwd);
        }

        change = false;
        std::string certfile = rqst.postValue("cert.filename");
        if (!certfile.empty())
        {
            FILE *fd = fopen(CERTFILENAME, "w");
            fputs(rqst.postValue("cert"), fd);
            fclose(fd);
            change = true;
        }

        std::string keyfile = rqst.postValue("key.filename");
        if (!keyfile.empty())
        {
            FILE *fd = fopen(KEYFILENAME, "w");
            fputs(rqst.postValue("key"), fd);
            fclose(fd);

            fd = fopen(PASSFILENAME, "w");
            fputs(rqst.postValue("pass"), fd);
            fclose(fd);
            change = true;
        }

#ifdef USE_HTTPS
        if (change)
        {
            log_->print("Changing TLS parameters\n");
            if (web->is_https_listening())
            {
                web->stop_https();
            }
            if (web->start_https())
            {
                log_->print("Started HTTPS\n");
                if (!web->ap_active())
                {
                    web->stop_http();
                }
            }
            else
            {
                log_->print("Failed to start HTTPS\n");
                web->start_http();
            }
        }
#endif

        return http_get(web, client, rqst, close);
    }
    return false;
}

void WEBMOUSE::web_message(WEB *web, ClientHandle client, const std::string &msg)
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

    log_->print_debug(1, "%d WS %s\n", client, msg.c_str());
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
        send_state(client);
        MOUSE::get()->send_led_status();
    }
    else if (func == "get_title")
    {
        send_title(client);
    }
    else if (func == "get_wifi")
    {
        send_wifi(client);
    }
    else if (func == "scan_wifi")
    {
        WEB::get()->scan_wifi(client, scan_complete, this);
    }
}

bool WEBMOUSE::scan_complete(WEB *web, ClientHandle client, const WiFiScanData &data)
{
    std::string resp("{\"ssids\":\"<option>-- Choose WiFi --</option>");
    for (auto it = data.cbegin(); it != data.cend(); ++it)
    {
        if (!it->first.empty())
        {
            resp += "<option>" + it->first + "</option>";
        }
    }
    resp += "\"}";
    return web->send_message(client, resp.c_str());
}

void WEBMOUSE::button_event(struct Button::ButtonEvent &ev, void *user_data)
{
    if (ev.action == Button::Button_Clicked)
    {
        static_cast<WEBMOUSE *>(user_data)->log_->print("Start WiFi AP for 30 minutes\n");
        WEB::get()->enable_ap(30, "webmouse");
    }
}

int main(int argc, const char *argv[])
{
    stdio_init_all();
    printf("webmouse\n");

    struct pfs_pfs *pfs;
    struct lfs_config cfg;
    ffs_pico_createcfg (&cfg, ROOT_OFFSET, ROOT_SIZE);
    pfs = pfs_ffs_create (&cfg);
    pfs_mount (pfs, "/");

    printf("Filesystem mounted\n");

    CONFIG::get()->init();

    if (cyw43_arch_init()) {
        printf("failed to initialise cyw43_arch\n");
        return -1;
    }

    WEBMOUSE *webmouse = new WEBMOUSE();

    printf("webmouse loop\n");
    webmouse->run();
}