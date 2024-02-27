#include "web.h"
#include "ws.h"
#include "txt.h"

#include "stdio.h"

#include "pico/cyw43_arch.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include "lwip/apps/mdns.h"

WEB *WEB::singleton_ = nullptr;

WEB::WEB() : server_(nullptr), message_callback_(nullptr)
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

#define WIFI_SSID "VodafoneMobileWiFi-B56878"
#define WIFI_PASSWORD "9647950309"

bool WEB::init()
{
    cyw43_arch_enable_sta_mode();

    printf("Connecting to Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("failed to connect.\n");
        return 1;
    }
    else
    {
        printf("Connected as %s.\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    }

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);

    err_t err = tcp_bind(pcb, NULL, 80);
    if (err)
    {
        printf("failed to bind to port %u\n", 80);
        return false;
    }

    server_ = tcp_listen_with_backlog(pcb, 1);
    if (!server_)
    {
        printf("failed to listen\n");
        if (pcb)
        {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(server_, this);
    tcp_accept(server_, tcp_server_accept);

#if LWIP_MDNS_RESPONDER
    mdns_resp_init();
    mdns_resp_add_netif(netif_default, "webmouse");
    //mdns_resp_add_service(netif_default, "webmouse", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    mdns_resp_announce(netif_default);
#endif
    
    return true;
}

err_t WEB::tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    WEB *web = (WEB *)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        printf("Failure in accept\n");
        return ERR_VAL;
    }
    web->clients_[client_pcb] = WEB::CLIENT();
    printf("Client connected %p (%d clients)\n", client_pcb, web->clients_.size());

    tcp_arg(client_pcb, web);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    //tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    //tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

err_t WEB::tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    WEB *web = (WEB *)arg;
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
        tcp_recved(tpcb, p->tot_len);

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

err_t WEB::tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    WEB *web = (WEB *)arg;
    web->write_next(tpcb);
    return ERR_OK;
}

err_t WEB::send_buffer(struct tcp_pcb *client_pcb, void *buffer, u16_t buflen, bool allocate)
{
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        ci->second.queue_send(buffer, buflen, allocate);
        write_next(client_pcb);
    }
    return ERR_OK;
}

err_t WEB::write_next(tcp_pcb *client_pcb)
{
    err_t err = ERR_OK;
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        u16_t nn = tcp_sndbuf(client_pcb);
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
            err = tcp_write(client_pcb, buffer, buflen, 0);
            tcp_output(client_pcb);
            cyw43_arch_lwip_end();
            if (err != ERR_OK)
            {
                printf("Failed to write data %d\n", err);
            }
        }

        if (ci->second.isClosed() && !ci->second.more_to_send())
        {
            close_client(client_pcb);
        }
    }
    else
    {
        printf("Unknown client %p for web %p (%p)\n", client_pcb, this, WEB::get());
    }
    return err;    
}

void WEB::process_rqst(struct tcp_pcb *client_pcb)
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
                if (url == "/")
                {
                    send_home_page(client_pcb);
                }
                else if (url == "/webmouse.css")
                {
                    send_css_file(client_pcb);
                }
                else if (url == "/webmouse.js")
                {
                    send_js_file(client_pcb);
                }
                else if (url == "/ws/")
                {
                    open_websocket(client_pcb, lines);
                }
                else
                {
                    send_buffer(client_pcb, (void *)"HTTP/1.0 404 NOT_FOUND\r\n\r\n", 26);
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

void WEB::send_home_page(struct tcp_pcb *client_pcb)
{
    static char html[] = 
        "HTTP/1.0 200 OK\r\nContent-type: text/html\r\n\r\n"
        "<!DOCTYPE html>"
        "<html>"
        " <head>"
        "  <title>Web Mouse</title>"
        "  <meta name='viewport' content='width=device-width, initial-scale=1'>"
        "  <link rel='stylesheet' type='text/css' href='/webmouse.css' />"
        "  <script type='text/javascript' src='/webmouse.js'></script>"
        " </head>"
        " <body>"
        "  <h1>Web Mouse</h1>"
        "  <div class='mousearea' id='mousearea'>"
        "  </div>"
        "  <button type='button' class='l' id='left'>L</button>"
        "  <input type='text' id='kbd' placeholder='kb' autocorrect='off' autocapitalize='off'>"
        "  <button type='button' class='r' id='right'>R</button>"
        " </body>"
        "</html>";
    send_buffer(client_pcb, (void *)html, strlen(html), 0);
}

void WEB::send_css_file(struct tcp_pcb *client_pcb)
{
    static char css[] =
        "HTTP/1.0 200 OK\r\nContent-type: text/css\r\n\r\n"
        "body {text-align: center; font-family: sans-serif; width: 350px; margin: auto; background: lightblue;}\n"
        "h1 {margin: 0px;}\n"
        "div.mousearea {width: 100%; height: 512px; border: 1px solid black;}\n"
        "button.l {width: 40%; height:64px; align:left}\n"
        "button.r {width: 40%; height:64px; align:right}\n"
        "input {width: 5%; margin: 12px;}\n";
    send_buffer(client_pcb, (void *)css, strlen(css), 0);
}

void WEB::send_js_file(struct tcp_pcb *client_pcb)
{
    static char js[] =
        "HTTP/1.0 200 OK\r\nContent-type: text/javascript\r\n\r\n"
        "var x_;\n"
        "var y_;\n"
        "var dx_ = 0;\n"
        "var dy_ = 0;\n"
        "var dw_ = 0;\n"
        "var l_ = 0;\n"
        "var r_ = 0;\n"
        "var c_ = '';\n"
        "document.addEventListener('DOMContentLoaded', function()\n"
        "{\n"
        "  let ma = document.getElementById('mousearea');\n"
        "  ma.addEventListener('touchstart', t_start);\n"
        "  ma.addEventListener('touchmove', t_move);\n"
        "  let btn = document.getElementById('left');\n"
        "  btn.addEventListener('touchstart', (e) => {l_ = 1; report();});\n"
        "  btn.addEventListener('touchend', (e) => {l_ = 0; report();});\n"
        "  btn.addEventListener('touchcancel', (e) => {l_ = 0; report();});\n"
        "  btn = document.getElementById('right');\n"
        "  btn.addEventListener('touchstart', (e) => {r_ = 1; report();});\n"
        "  btn.addEventListener('touchend', (e) => {r_ = 0; report();});\n"
        "  btn.addEventListener('touchcancel', (e) => {r_ = 0; report();});\n"
        "  let kbd = document.getElementById('kbd');\n"
        "  //kbd.addEventListener('keydown', (e) => {keystroke(e, 'down');});\n"
        "  kbd.addEventListener('keyup', (e) => {keystroke(e, 'up');});\n"
        "  //kbd.addEventListener('keypress', (e) => {keystroke(e, 'press');});\n"
        "  kbd.addEventListener('input', (e) => {keystroke(e, 'input');});\n"
        "  openWS();\n"
        "  setInterval(checkReport, 100);"
        "});\n"
        "function t_start(e)\n"
        "{\n"
        "  e.preventDefault();\n"
        "  console.log(e);\n"
        "  let t1 = e.targetTouches[0];\n"
        "  x_ = t1.pageX;\n"
        "  y_ = t1.pageY;\n"
        "}\n"
        "function t_move(e)\n"
        "{\n"
        "  console.log(e);\n"
        "  let t1 = e.targetTouches[0];\n"
        "  if (e.targetTouches.length == 1)\n"
        "  {\n"
        "    dx_ += Math.round(t1.pageX - x_);\n"
        "    dy_ += Math.round(t1.pageY - y_);\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    dw_ += Math.round(t1.pageY - y_);\n"
        "  }\n"
        "  x_ = t1.pageX;\n"
        "  y_ = t1.pageY;\n"
        "}\n"
        "function keystroke(e, updown)\n"
        "{\n"
        "  if (updown !== 'input')\n"
        "  {\n"
        "    let cod = e.which;\n"
        "    if (e.which == 229 && c_.length > 0)\n"
        "    {\n"
        "      cod = c_.charCodeAt(0)"
        "    }\n"
        "    let txt = 'c=' + cod;\n"
        "    sendToWS(txt);\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    c_ = e.data;\n"
        "    e.srcElement.value = '';\n"
        "  }\n"
        "}\n"
        "function checkReport()\n"
        "{\n"
        "  if (dx_ != 0 || dy_ != 0 || dw_ != 0) report();\n"
        "}\n"
        "function report()\n"
        "{\n"
        "  let txt = 'x=' + dx_ + ' y=' + dy_ + ' w=' + dw_ + ' l=' + l_ + ' r=' + r_;\n"
        "  //let ma = document.getElementById('mousearea');\n"
        "  //ma.innerHTML = txt;\n"
        "  dx_ = 0;\n"
        "  dy_ = 0;\n"
        "  dw_ = 0;\n"
        "  sendToWS(txt);\n"
        "}\n"
        ""
        "// Websocket variables\n"
        "var ws = undefined;             // WebSocket object\n"
        "var timer_ = undefined;         // Reconnect timer\n"
        "var conchk_ = undefined;        // Connection check timer\n"
        "var opened_ = false;            // Connection opened flag\n"
        "var suspended_ = false;         // I/O suspended flag\n"
        "\n"
        "function openWS()\n"
        "{\n"
        "    if ('WebSocket' in window)\n"
        "    {\n"
        "        if (typeof ws === 'object')\n"
        "        {\n"
        "            console.log('Closing old connection');\n"
        "            ws.close();\n"
        "            ws = undefined;\n"
        "        }\n"
        "        opened_ = false;\n"
        "        console.log('ws://' + location.host + '/ws/');\n"
        "        ws = new WebSocket('ws://' + location.host + '/ws/');\n"
        "        if (conchk_ === undefined)\n"
        "        {\n"
        "            conchk_ = setTimeout(checkOpenState, 250);\n"
        "        }\n"
        "\n"
        "        ws.onopen = function(ev)\n"
        "        {\n"
        "            console.log('ws open state ' + ws.readyState);\n"
        "            checkOpenState();\n"
        "        };\n"
        "\n"
        "        ws.onclose = function()\n"
        "        {\n"
        "            console.log('ws closed');\n"
        "            opened_ = false;\n"
        "            ws = undefined;\n"
        "            if (conchk_ !== undefined)\n"
        "            {\n"
        "                clearTimeout(conchk_);\n"
        "                conchk_ = undefined;\n"
        "            }\n"
        "        };\n"
        "\n"
        "        ws.onmessage = function(evt)\n"
        "        {\n"
        "            process_ws_message(evt);\n"
        "        };\n"
        "\n"
        "        ws.onerror = function(error)\n"
        "        {\n"
        "            console.log('WS error:');\n"
        "            console.error(error);\n"
        "            if (ws !== undefined)\n"
        "            {\n"
        "                ws.close();\n"
        "                opened_ = false;\n"
        "                retryConnection();\n"
        "            }\n"
        "        };\n"
        "\n"
        "        //  Set interval to reconnect and suspend when hidden\n"
        "        if (typeof timer_ === 'undefined')\n"
        "        {\n"
        "            timer_ = setInterval(retryConnection, 10000);\n"
        "            if (typeof document.hidden != 'undefined')\n"
        "            {\n"
        "                document.onvisibilitychange = function()\n"
        "                {\n"
        "                    suspended_ = document.hidden;\n"
        "                    if (!suspended_)\n"
        "                    {\n"
        "                        retryConnection();\n"
        "                    }\n"
        "                    else\n"
        "                    {\n"
        "                        if (ws !== undefined)\n"
        "                        {\n"
        "                            ws.close();\n"
        "                        }\n"
        "                        opened_ = false;\n"
        "                    }\n"
        "                };\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n"
        "\n"
        "function checkOpenState(retries = 0)\n"
        "{\n"
        "    clearTimeout(conchk_);\n"
        "    conchk_ = undefined;\n"
        "\n"
        "    if (typeof ws == 'object')\n"
        "    {\n"
        "        if (ws.readyState == WebSocket.OPEN)\n"
        "        {\n"
        "            if (!opened_)\n"
        "            {\n"
        "                opened_ = true;\n"
        "                console.log('ws connected after ' + (retries * 250) + ' msec');\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n"
        "\n"
        "function retryConnection()\n"
        "{\n"
        "    if (!suspended_)\n"
        "    {\n"
        "        if (typeof ws !== 'object')\n"
        "        {\n"
        "            openWS();\n"
        "        }\n"
        "     }\n"
        "}\n"
        "\n"
        "function sendToWS(msg)\n"
        "{\n"
        "    if (typeof ws === 'object')\n"
        "    {\n"
        "        ws.send(msg);\n"
        "        console.log('Sent: ' + msg);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        alert('No connection to remote!\\nRefresh browser and try again.');\n"
        "    }\n"
        "}\n";
    send_buffer(client_pcb, (void *)js, strlen(js), 0);
}

void WEB::open_websocket(struct tcp_pcb *client_pcb, std::vector<std::string> &headers)
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

void WEB::process_websocket(struct tcp_pcb *client_pcb)
{
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        uint8_t opc = ci->second.wshdr().meta.bits.OPCODE;
        std::string payload = ci->second.rqst().substr(ci->second.wshdr().start, ci->second.wshdr().length);
        switch (opc)
        {
        case WEBSOCKET_OPCODE_TEXT:
            if (message_callback_)
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

void WEB::send_websocket(struct tcp_pcb *client_pcb, enum WebSocketOpCode opc, const std::string &payload, bool mask)
{
    std::string msg;
    WS::BuildPacket(opc, payload, msg, mask);
    send_buffer(client_pcb, (void *)msg.c_str(), msg.length());
}

void WEB::close_client(struct tcp_pcb *client_pcb, bool isClosed)
{
    auto ci = clients_.find(client_pcb);
    if (ci != clients_.end())
    {
        printf("Closing %s %p.%s\n", (ci->second.isWebSocket() ? "ws" : "http"), client_pcb, (ci->second.more_to_send() ? " waiting" : ""));
        if (!isClosed)
        {
            ci->second.setClosed();
            if (!ci->second.more_to_send())
            {
                tcp_close(client_pcb);
                clients_.erase(ci);
            }
        }
        else
        {
            clients_.erase(ci);
        }
    }
    else
    {
        if (!isClosed)
        {
            tcp_close(client_pcb);
        }   
    }
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
