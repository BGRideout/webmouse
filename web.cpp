#include "web.h"
#include "web_files.h"
#include "ws.h"
#include "txt.h"
#include "config.h"

#include "stdio.h"

#include "pico/cyw43_arch.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "lwip/prot/iana.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "mbedtls/debug.h"
#include "hardware/gpio.h"

#define AP_ACTIVE_MINUTES 30
#define AP_BUTTON 16

WEB *WEB::singleton_ = nullptr;

WEB::WEB() : server_(nullptr), wifi_state_(CYW43_LINK_DOWN),
             ap_active_(0), ap_requested_(false), mdns_active_(false),
             message_callback_(nullptr), notice_callback_(nullptr)
{
}

WEB *WEB::get()
{
    if (!singleton_)
    {
        singleton_ = new WEB();
    }
    return singleton_;
}

bool WEB::init()
{
    cyw43_arch_enable_sta_mode();

    if (!connect_to_wifi())
    {
        printf("failed to connect.\n");
        return 1;
    }

#ifdef USE_HTTPS
    u16_t port = LWIP_IANA_PORT_HTTPS;
    const char *pkey;
    uint16_t    pkeylen;
    WEB_FILES::get()->get_file("newkey.pem", pkey, pkeylen);
    const char pkpass[] = "webmouse";
    uint16_t pkpasslen = sizeof(pkpass);
    const char *cert;
    uint16_t    certlen;
    WEB_FILES::get()->get_file("newcert.pem", cert, certlen);
    
    struct altcp_tls_config * conf = altcp_tls_create_config_server_privkey_cert((const u8_t *)pkey, pkeylen + 1,
                                                                                 (const u8_t *)pkpass, pkpasslen,
                                                                                 (const u8_t *)cert, certlen + 1);
    if (!conf)
    {
        printf("TLS configuration not loaded\n");
    }
    mbedtls_debug_set_threshold(2);

    altcp_allocator_t alloc = {altcp_tls_alloc, conf};
    #else
    u16_t port = LWIP_IANA_PORT_HTTP;
    struct altcp_tls_config * conf = nullptr;
    altcp_allocator_t alloc = {altcp_tcp_alloc, conf};
    #endif

    struct altcp_pcb *pcb = altcp_new_ip_type(&alloc, IPADDR_TYPE_ANY);

    err_t err = altcp_bind(pcb, IP_ANY_TYPE, port);
    if (err)
    {
        printf("failed to bind to port %u: %d\n", port, err);
        return false;
    }

    server_ = altcp_listen_with_backlog(pcb, 1);
    if (!server_)
    {
        printf("failed to listen\n");
        if (pcb)
        {
            altcp_close(pcb);
        }
        return false;
    }

    altcp_arg(server_, this);
    altcp_accept(server_, tcp_server_accept);

    mdns_resp_init();
    
    add_repeating_timer_ms(500, timer_callback, this, &timer_);
    enable_ap_button();

    return true;
}

bool WEB::connect_to_wifi()
{
    CONFIG *cfg = CONFIG::get();
    printf("Connecting to Wi-Fi on SSID '%s' ...\n", cfg->ssid());
    netif_set_hostname(wifi_netif(CYW43_ITF_STA), cfg->hostname());
    return cyw43_arch_wifi_connect_async(cfg->ssid(), cfg->password(), CYW43_AUTH_WPA2_AES_PSK) == 0;
}

err_t WEB::tcp_server_accept(void *arg, struct altcp_pcb *client_pcb, err_t err)
{
    WEB *web = get();
    if (err != ERR_OK || client_pcb == NULL) {
        printf("Failure in accept %d\n", err);
        return ERR_VAL;
    }
    web->clients_.insert(std::pair<struct altcp_pcb *, WEB::CLIENT>(client_pcb, WEB::CLIENT(client_pcb)));
    printf("Client connected %p (%d clients)\n", client_pcb, web->clients_.size());

    altcp_arg(client_pcb, client_pcb);
    altcp_sent(client_pcb, tcp_server_sent);
    altcp_recv(client_pcb, tcp_server_recv);
    altcp_poll(client_pcb, tcp_server_poll, 1 * 2);
    altcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

err_t WEB::tcp_server_recv(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    WEB *web = get();
    if (!p)
    {
        web->close_client(tpcb);
        return ERR_OK;
    }

    // this method is callback from lwIP, so cyw43_arch_lwip_begin is not required, however you
    // can use this method to cause an assertion in debug mode, if this method is called when
    // cyw43_arch_lwip_begin IS needed
    cyw43_arch_lwip_check();
    if (p->tot_len > 0)
    {
        // Receive the buffer
        bool ready = false;
        auto ci = web->clients_.find(tpcb);
        if (ci != web->clients_.end())
        {
            char buf[64];
            u16_t ll = 0;
            while (ll < p->tot_len)
            {
                u16_t l = pbuf_copy_partial(p, buf, sizeof(buf), ll);
                ll += l;
                ci->second.addToRqst(buf, l);
            }
        }
        altcp_recved(tpcb, p->tot_len);

        while (ci->second.rqstIsReady())
        {
            if (!ci->second.isWebSocket())
            {
                web->process_rqst(tpcb);
            }
            else
            {
                web->process_websocket(tpcb);
            }
            ci->second.resetRqst();
        }
    }
    pbuf_free(p);

    return ERR_OK;
}

err_t WEB::tcp_server_sent(void *arg, struct altcp_pcb *tpcb, u16_t len)
{
    WEB *web = get();
    web->write_next(tpcb);
    return ERR_OK;
}

err_t WEB::tcp_server_poll(void *arg, struct altcp_pcb *tpcb)
{
    WEB *web = get();
    auto ci = web->clients_.find(tpcb);
    if (ci != web->clients_.end())
    {
        //  Test for match on PCB to avoid race condition on close
        if (ci->second.pcb() == tpcb)
        {
            if (ci->second.more_to_send())
            {
                printf("Sending to %p on poll (%d clients)\n", ci->first, web->clients_.size());
                web->write_next(ci->first);
            }
        }
        else
        {
            printf("Poll on closed pcb %p\n", tpcb);
        }
    }
    return ERR_OK;
}

void WEB::tcp_server_err(void *arg, err_t err)
{
    WEB *web = get();
    altcp_pcb *client_pcb = (altcp_pcb *)arg;
    printf("Error %d on client %p\n", err, client_pcb);
    auto ci = web->clients_.find(client_pcb);
    if (ci != web->clients_.end())
    {
        web->close_client(client_pcb, true);
    }
}

err_t WEB::send_buffer(struct altcp_pcb *client_pcb, void *buffer, u16_t buflen, bool allocate)
{
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        ci->second.queue_send(buffer, buflen, allocate);
        write_next(client_pcb);
    }
    return ERR_OK;
}

err_t WEB::write_next(altcp_pcb *client_pcb)
{
    err_t err = ERR_OK;
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        u16_t nn = altcp_sndbuf(client_pcb);
        if (nn > TCP_MSS)
        {
            nn = TCP_MSS;
        }
        void *buffer;
        u16_t buflen;
        if (ci->second.get_next(nn, &buffer, &buflen))
        {
            cyw43_arch_lwip_begin();
            cyw43_arch_lwip_check();
            err = altcp_write(client_pcb, buffer, buflen, 0);
            altcp_output(client_pcb);
            cyw43_arch_lwip_end();
            if (err != ERR_OK)
            {
                printf("Failed to write %d bytes of data %d to %p\n", buflen, err, client_pcb);
                ci->second.requeue(buffer, buflen);
            }
        }

        if (ci->second.isClosed() && !ci->second.more_to_send())
        {
            close_client(client_pcb);
        }
    }
    else
    {
        printf("Unknown client %p for write\n", client_pcb);
    }
    return err;    
}

void WEB::process_rqst(struct altcp_pcb *client_pcb)
{
    bool ok = false;
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        std::vector<std::string> lines;
        std::vector<std::string> tokens;

        TXT::split(ci->second.rqst(), "\n", lines);
        for (int ii = 0; ii < lines.size(); ii++)
        {
            TXT::trim_back(lines[ii]);
        }

        if (!ci->second.isWebSocket())
        {
            TXT::split(lines.at(0), " ", tokens);
            if (tokens.size() == 3 && tokens.at(0) == "GET")
            {
                ok = true;
                std::string url = tokens.at(1);
                if (url == "/ws/")
                {
                    open_websocket(client_pcb, lines);
                }
                else
                {
                    if (url == "/")
                    {
                        url = "index.html";
                    }
                    else if (url == "/config")
                    {
                        url = "config.html";
                    }
                    if (url.at(0) == '/')
                    {
                        url = url.substr(1);
                    }
                    const char *data;
                    u16_t datalen;
                    if (WEB_FILES::get()->get_file(url, data, datalen))
                    {
                        send_buffer(client_pcb, (void *)data, datalen, false);
                    }
                    else
                    {
                        send_buffer(client_pcb, (void *)"HTTP/1.0 404 NOT_FOUND\r\n\r\n", 26);
                    }
                }
            }
        }

        if (!ok)
        {
            send_buffer(client_pcb, (void *)"HTTP/1.0 500 Internal Server Error\r\n\r\n", 38);
        }

        if (!ci->second.isWebSocket())
        {
            close_client(client_pcb);
        }
    }
    else
    {
        process_websocket(client_pcb);
    }
}

void WEB::open_websocket(struct altcp_pcb *client_pcb, std::vector<std::string> &headers)
{
    printf("Accepting websocket connection on %p\n", client_pcb);
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        bool    hasConnection = false;
        bool    hasUpgrade = false;
        bool    hasVersion = false;
        bool    hasOrigin = false;

        std::string host;
        std::string key;

        for (auto it = headers.cbegin(); it != headers.cend(); ++it)
        {
            std::string header;
            std::string value;
            size_t ii = (*it).find(":");
            if (ii != std::string::npos)
            {
                header = (*it).substr(0, ii);
                value = (*it).substr(ii + 1);
                TXT::trim_front(value);
                if (header == "Host")
                {
                    host = value;
                }
                else if (header == "Connection")
                {
                    hasConnection = value.find("Upgrade") != std::string::npos;
                }
                else if (header == "Upgrade")
                {
                    hasUpgrade = value == "websocket";
                }
                else if (header == "Origin")
                {
                    hasOrigin = true;
                }
                else if (header == "Sec-WebSocket-Version")
                {
                    hasVersion = value == "13";
                }
                else if (header == "Sec-WebSocket-Key")
                {
                    key = value;
                }
            }
        }

        if (hasConnection && hasOrigin && hasUpgrade && hasVersion && host.length() > 0)
        {
            key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            unsigned char sha1[20];
            mbedtls_sha1((const unsigned char *)key.c_str(), key.length(), sha1);
            unsigned char b64[64];
            size_t b64ll;
            mbedtls_base64_encode(b64, sizeof(b64), &b64ll, sha1, sizeof(sha1));

            static char resp[]=
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: ";

            static char crlfcrlf[] = "\r\n\r\n";

            send_buffer(client_pcb, resp, strlen(resp), false);
            send_buffer(client_pcb, b64, b64ll);
            send_buffer(client_pcb, crlfcrlf, 4);

            ci->second.setWebSocket();
            ci->second.clearRqst();
        }
        else
        {
            printf("Bad websocket request from %p\n", client_pcb);
        }
    }
}

void WEB::process_websocket(struct altcp_pcb *client_pcb)
{
    std::string func;
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        uint8_t opc = ci->second.wshdr().meta.bits.OPCODE;
        std::string payload = ci->second.rqst().substr(ci->second.wshdr().start, ci->second.wshdr().length);
        switch (opc)
        {
        case WEBSOCKET_OPCODE_TEXT:
            if (payload.substr(0, 5) == "func=")
            {
                std::size_t ii = payload.find_first_of(" ");
                if (ii == std::string::npos)
                {
                    func = func = payload.substr(5);
                }
                else
                {
                    func = payload.substr(5, ii - 5);
                }
            }

            if (func == "get_wifi")
            {
                get_wifi(client_pcb);
            }
            else if (func == "scan_wifi")
            {
                printf("Scan WiFi\n");
                scan_wifi(client_pcb);
            }
            else if (func == "config_update")
            {
                update_wifi(payload);
            }
            else if (message_callback_)
            {
                message_callback_(payload);
            }
            break;

        case WEBSOCKET_OPCODE_PING:
            send_websocket(client_pcb, WEBSOCKET_OPCODE_PONG, payload);
            break;

        case WEBSOCKET_OPCODE_CLOSE:
            send_websocket(client_pcb, WEBSOCKET_OPCODE_CLOSE, payload);
            close_client(client_pcb);
            break;

        default:
            printf("Unhandled websocket opcode %d from %p\n", opc, client_pcb);
        }
    }
}

void WEB::send_websocket(struct altcp_pcb *client_pcb, enum WebSocketOpCode opc, const std::string &payload, bool mask)
{
    std::string msg;
    WS::BuildPacket(opc, payload, msg, mask);
    send_buffer(client_pcb, (void *)msg.c_str(), msg.length());
}

void WEB::broadcast_websocket(const std::string &txt)
{
    for (auto it = clients_.begin(); it != clients_.end(); ++it)
    {
        if (it->second.isWebSocket())
        {
            send_websocket(it->first, WEBSOCKET_OPCODE_TEXT, txt);
        }
    }
}

void WEB::close_client(struct altcp_pcb *client_pcb, bool isClosed)
{
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        if (!isClosed)
        {
            ci->second.setClosed();
            if (!ci->second.more_to_send())
            {
                altcp_close(client_pcb);
                printf("Closed %s %p. client count = %d\n", (ci->second.isWebSocket() ? "ws" : "http"), client_pcb, clients_.size() - 1);
                clients_.erase(ci);
            }
            else
            {
                printf("Waiting to close %s %p\n", (ci->second.isWebSocket() ? "ws" : "http"), client_pcb);
            }
        }
        else
        {
            printf("Closing %s %p for error\n", (ci->second.isWebSocket() ? "ws" : "http"), client_pcb);
            ci->second.setClosed();
            clients_.erase(ci);
        }
    }
    else
    {
        if (!isClosed)
        {
            altcp_close(client_pcb);
        }   
    }
}

void WEB::check_wifi()
{
    netif *ni = wifi_netif(CYW43_ITF_STA);
    int sts = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (sts != wifi_state_ || !ip_addr_eq(&ni->ip_addr, &wifi_addr_))
    {
        wifi_state_ = sts;
        wifi_addr_ = ni->ip_addr;

        switch (wifi_state_)
        {
        case CYW43_LINK_JOIN:
        case CYW43_LINK_NOIP:
            // Progress - no action needed
            send_notice(STA_INITIALIZING);
            break;

        case CYW43_LINK_UP:
            //  Connected
            if (mdns_active_)
            {
                mdns_resp_remove_netif(ni);
            }
            mdns_resp_add_netif(ni, CONFIG::get()->hostname());
            mdns_resp_announce(ni);
            mdns_active_ = true;
            get_wifi(nullptr);
            printf("Connected to WiFi with IP address %s\n", ip4addr_ntoa(netif_ip4_addr(ni)));
            send_notice(STA_CONNECTED);
            break;

        case CYW43_LINK_DOWN:
        case CYW43_LINK_FAIL:
        case CYW43_LINK_NONET:
            //  Not connected
            printf("WiFi disconnected. status = %s\n", (wifi_state_ == CYW43_LINK_DOWN) ? "link down" :
                                                       (wifi_state_ == CYW43_LINK_FAIL) ? "link failed" :
                                                       "No network found");
            send_notice(STA_DISCONNECTED);
            break;

        case CYW43_LINK_BADAUTH:
            //  Need intervention to connect
            printf("WiFi authentication failed\n");
            send_notice(STA_DISCONNECTED);
            break;
        }
    }

    if (ap_active_ > 0)
    {
        ap_active_ -= 1;
        if (ap_active_ == 0)
        {
            stop_ap();
        }
    }
    if (ap_requested_)
    {
        ap_requested_ = false;
        start_ap();
    }
}

void WEB::get_wifi(struct altcp_pcb *client_pcb)
{
    CONFIG *cfg = CONFIG::get();
    std::string wifi("{\"host\":\"<h>\", \"ssid\":\"<s>\", \"ip\":\"<a>\"}");
    TXT::substitute(wifi, "<h>", cfg->hostname());
    TXT::substitute(wifi, "<s>", cfg->ssid());
    TXT::substitute(wifi, "<a>", ip4addr_ntoa(netif_ip4_addr(wifi_netif(CYW43_ITF_STA))));
    if (client_pcb)
    {
        send_websocket(client_pcb, WEBSOCKET_OPCODE_TEXT, wifi);
    }
    else
    {
        broadcast_websocket(wifi);
    }
    scan_wifi(client_pcb);
}

void WEB::update_wifi(const std::string &cmd)
{
    CONFIG *cfg = CONFIG::get();
    std::string hostname;
    std::string ssid;
    std::string password = cfg->password();

    std::vector<std::string> items;
    TXT::split(cmd, " ", items);
    for (auto it = items.cbegin(); it != items.cend(); ++it)
    {
        std::vector<std::string> item;
        TXT::split(*it, "=", item);
        std::string name = item.at(0);
        std::string value = "";
        if (item.size() > 1)
        {
            value = item.at(1);
        }
        if (name == "hostname")
        {
            hostname = value;
        }
        else if (name == "ssid")
        {
            ssid = value;
        }
        else if (name == "pwd" && value.length() > 0)
        {
            password = value;
        }
    }

    printf("Update: host=%s, ssid=%s, pw=%s\n", hostname.c_str(), ssid.c_str(), password.c_str());

    if (hostname != cfg->hostname())
    {
        cfg->set_hostname(hostname.c_str());
    }
    if (ssid != cfg->ssid() || password != cfg->password())
    {
        cfg->set_wifi_credentials(ssid.c_str(), password.c_str());
    }

    //  Restart the STA WiFi
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    connect_to_wifi();
}

void WEB::scan_wifi(struct altcp_pcb *client_pcb)
{
    if (!cyw43_wifi_scan_active(&cyw43_state))
    {
        cyw43_wifi_scan_options_t opts = {0};
        int sts = cyw43_wifi_scan(&cyw43_state, &opts, this, scan_cb);
        scans_.clear();
    }
    scans_.insert(client_pcb);
}

int WEB::scan_cb(void *arg, const cyw43_ev_scan_result_t *rslt)
{
    if (rslt)
    {
        std::string ssid;
        ssid.append((char *)rslt->ssid, rslt->ssid_len);
        get()->ssids_[ssid] = rslt->rssi;
    }
    return 0;
}

void WEB::check_scan_finished()
{
    if (scans_.size() > 0 && !cyw43_wifi_scan_active(&cyw43_state))
    {
        std::string msg("{\"ssids\":\"<option value=''>-- Choose WiFi access point --</option>");
        printf("Scan finished (%d):\n", ssids_.size());
        for (auto it = ssids_.cbegin(); it != ssids_.cend(); ++it)
        {
            printf("  %s\n", it->first.c_str());
            msg += "<option value='";
            msg += it->first.c_str();
            msg += "'>";
            msg += it->first.c_str();
            msg += "</option>";
        }
        msg += "\"}";

        for (auto it = scans_.cbegin(); it != scans_.cend(); ++it)
        {
            if (*it == nullptr)
            {
                broadcast_websocket(msg);
            }
            else if (clients_.find(*it) != clients_.end())
            {
                send_websocket(*it, WEBSOCKET_OPCODE_TEXT, msg);
            }
        }

        ssids_.clear();
        scans_.clear();
    }
}

bool WEB::timer_callback(repeating_timer_t *rt)
{
    get()->check_wifi();
    get()->check_scan_finished();
    return true;
}

void WEB::enable_ap_button()
{
    gpio_init(AP_BUTTON);
    gpio_set_dir(AP_BUTTON, false);
    gpio_pull_up(AP_BUTTON);
    gpio_set_irq_enabled_with_callback(AP_BUTTON, GPIO_IRQ_EDGE_RISE, true, &ap_button_callback);
}

void WEB::ap_button_callback(uint gpio, uint32_t event_mask)
{
    get()->ap_requested_ = true;
}

void WEB::start_ap()
{
    if (ap_active_ == 0)
    {
        printf("Starting AP webmouse\n");
        cyw43_arch_enable_ap_mode("webmouse", "12345678", CYW43_AUTH_WPA2_AES_PSK);
        netif_set_hostname(wifi_netif(CYW43_ITF_AP), "webmouse");
    
        // Start the dhcp server
        ip4_addr_t addr;
        ip4_addr_t mask;
        IP4_ADDR(ip_2_ip4(&addr), 192, 168, 4, 1);
        IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);
        dhcp_server_init(&dhcp_, &addr, &mask);
    }
    else
    {
        printf("AP is already active. Timer reset.\n");
    }
    ap_active_ = AP_ACTIVE_MINUTES * 60 * 2;
    send_notice(AP_ACTIVE);
}

void WEB::stop_ap()
{
    dhcp_server_deinit(&dhcp_);
    cyw43_arch_disable_ap_mode();
    printf("AP deactivated\n");
    send_notice(AP_INACTIVE);
}



WEB::CLIENT::~CLIENT()
{
    while (sendbuf_.size() > 0)
    {
        delete sendbuf_.front();
        sendbuf_.pop_front();
    }
}

void WEB::CLIENT::addToRqst(const char *str, u16_t ll)
{
    rqst_.append(str, ll);
}

bool WEB::CLIENT::rqstIsReady()
{
    bool ret = false;
    if (!isWebSocket())
    {
        ret = rqst_.find("\r\n\r\n") != std::string::npos;
    }
    else
    {
        ret = getWSMessage();
    }
    return ret;
}

bool WEB::CLIENT::getWSMessage()
{
    int sts = WS::ParsePacket(&wshdr_, rqst_);
    return sts == WEBSOCKET_SUCCESS;
}

void WEB::CLIENT::queue_send(void *buffer, u16_t buflen, bool allocate)
{
    WEB::SENDBUF *sbuf = new WEB::SENDBUF(buffer, buflen, allocate);
    sendbuf_.push_back(sbuf);
}

bool WEB::CLIENT::get_next(u16_t count, void **buffer, u16_t *buflen)
{
    bool ret = false;
    *buffer = nullptr;
    *buflen = 0;
    if (count > 0)
    {
        while (*buflen == 0 && sendbuf_.size() > 0)
        {
            if (sendbuf_.front()->to_send() > 0)
            {
                ret = sendbuf_.front()->get_next(count, buffer, buflen);
            }
            else
            {
                delete sendbuf_.front();
                sendbuf_.pop_front();
            }
        }
    }

    return ret;
}

void WEB::CLIENT::requeue(void *buffer, u16_t buflen)
{
    if (sendbuf_.size() > 0)
    {
        sendbuf_.front()->requeue(buffer, buflen);
    }
}

void WEB::CLIENT::resetRqst()
{
    if (!isWebSocket())
    {
        std::size_t ii = rqst_.find("\r\n\r\n");
        if (ii != std::string::npos)
        {
            rqst_.erase(0, ii + 4);
        }
        else
        {
            rqst_.clear();
        }
    }
    else
    {
        std::size_t ii = wshdr_.start + wshdr_.length;
        if (ii < rqst_.length())
        {
            rqst_.erase(0, ii);
        }
        else
        {
            rqst_.clear();
        }
    }
}

WEB::SENDBUF::SENDBUF(void *buf, uint32_t size, bool alloc) : buffer_((uint8_t *)buf), size_(size), sent_(0), allocated_(alloc)
{
    if (allocated_)
    {
        buffer_ = new uint8_t[size];
        memcpy(buffer_, buf, size);
    }
}

WEB::SENDBUF::~SENDBUF()
{
    if (allocated_)
    {
        delete [] buffer_;
    }
}

bool WEB::SENDBUF::get_next(u16_t count, void **buffer, u16_t *buflen)
{
    bool ret = false;
    *buffer = nullptr;
    uint32_t nn = to_send();
    if (count < nn)
    {
        nn = count;
    }
    if (nn > 0)
    {
        *buffer = &buffer_[sent_];
        ret = true;
    }
    *buflen = nn;
    sent_ += nn;

    return ret;
}

void WEB::SENDBUF::requeue(void *buffer, u16_t buflen)
{
    int32_t nn = sent_ - buflen;
    if (nn >= 0)
    {
        if (memcmp(&buffer_[nn], buffer, buflen) == 0)
        {
            sent_ = nn;
            printf("%d bytes requeued\n", buflen);
        }
        else
        {
            printf("Buffer mismatch! %d bytes not requeued\n", buflen);
        }
    }
    else
    {
        printf("%d bytes to requeue exceeds %d sent\n", buflen, sent_);
    }
}