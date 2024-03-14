// Websocket variables
var ws = undefined;             // WebSocket object
var timer_ = undefined;         // Reconnect timer
var conchk_ = undefined;        // Connection check timer
var opened_ = false;            // Connection opened flag
var closed_ = true;             // Websocket closed flag
var suspended_ = false;         // I/O suspended flag

function openWS()
{
    if ('WebSocket' in window)
    {
        closed_ = false;
        if (typeof ws === 'object')
        {
            console.log('Closing old connection');
            ws.close();
            ws = undefined;
        }
        setWSOpened(false);
        let url = 'ws://' + location.host + '/ws/';
        if (location.protocol == 'https:')
        {
            url = 'wss://' + location.host + '/ws/';
        }
        console.log(url);
        ws = new WebSocket(url);
        if (conchk_ === undefined)
        {
            conchk_ = setTimeout(checkOpenState, 250);
        }

        ws.onopen = function(ev)
        {
            console.log('ws open state ' + ws.readyState);
            checkOpenState();
        };

        ws.onclose = function()
        {
            console.log('ws closed');
            setWSOpened(false);
            ws = undefined;
            if (conchk_ !== undefined)
            {
                clearTimeout(conchk_);
                conchk_ = undefined;
            }
        };

        ws.onmessage = function(evt)
        {
            let obj = new Object;
            obj['open'] = opened_;
            const mevt = new CustomEvent('ws_message', { detail: { message: evt.data } });
            document.dispatchEvent(mevt);
        };

        ws.onerror = function(error)
        {
            console.log('WS error:');
            console.error(error);
            if (ws !== undefined)
            {
                ws.close();
                setWSOpened(false);
                retryConnection();
            }
        };

        //  Set interval to reconnect and suspend when hidden
        if (typeof timer_ === 'undefined')
        {
            timer_ = setInterval(retryConnection, 10000);
            if (typeof document.hidden != 'undefined')
            {
                document.onvisibilitychange = function()
                {
                    suspended_ = document.hidden;
                    if (!suspended_)
                    {
                        retryConnection();
                    }
                    else
                    {
                        if (ws !== undefined)
                        {
                            ws.close();
                        }
                        setWSOpened(false);
                    }
                };
            }
        }
    }
}

function checkOpenState(retries = 0)
{
    clearTimeout(conchk_);
    conchk_ = undefined;

    if (typeof ws == 'object')
    {
        if (ws.readyState == WebSocket.OPEN)
        {
            if (!opened_)
            {
                setWSOpened(true);
                console.log('ws connected after ' + (retries * 250) + ' msec');
            }
        }
    }
}

function setWSOpened(state)
{
  if (state != opened_)
  {
    opened_ = state;
    let obj = new Object;
    obj['open'] = opened_;
    const evt = new CustomEvent('ws_state', { detail: { obj: obj } });
    document.dispatchEvent(evt);
  }
}

function isWSOpen()
{
    return opened_;
}

function closeWS()
{
    closed_ = true;
    if (typeof ws === 'object')
    {
        console.log('Closing websocket');
        ws.close();
        ws = undefined;
    }
    setWSOpened(false);
}

function retryConnection()
{
    if (!suspended_ && !closed_)
    {
        if (typeof ws !== 'object')
        {
            openWS();
        }
     }
}

function sendToWS(msg)
{
    if (typeof ws === 'object')
    {
        ws.send(msg);
        //console.log('Sent: ' + msg);
    }
    else
    {
        alert('No connection to remote!\\nRefresh browser and try again.');
    }
}

