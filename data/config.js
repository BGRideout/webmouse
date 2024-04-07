var led_ = 0x0f;
var ping_ = true;

document.addEventListener('DOMContentLoaded', function()
{
  document.addEventListener('ws_state', ws_state_change);
  document.addEventListener('ws_message', process_ws_message);
  document.getElementById('scan').addEventListener('click', scan_wifi);
  document.getElementById('update').addEventListener('click', config_update);
  document.getElementById('title_update').addEventListener('click', config_update);
  document.getElementById('ssids').addEventListener('change', ssid_select);
  openWS();
  setInterval(checkConnection, 10000);
});

function ws_state_change(evt)
{
  if (evt.detail.obj['open'])
  {
    sendToWS('func=get_state');
    sendToWS('func=get_wifi');
    sendToWS('func=get_title');
    led_ &= ~8;
  }
  else
  {
    led_ |= 8;
  }
  show_led();
}

function process_ws_message(evt)
{
  try
  {
    let msg = JSON.parse(evt.detail.message);
    if (Object.hasOwn(msg, 'host'))
    {
      document.getElementById('hostname').value = msg['host'];
      document.getElementById('ssid').value = msg['ssid'];
      document.getElementById('ip').innerHTML = msg['ip'];
    }
    if (Object.hasOwn(msg, 'ssids'))
    {
      document.getElementById('ssids').innerHTML = msg['ssids'];
    }
    if (Object.hasOwn(msg, 'pin'))
    {
      let pin = msg['pin'];
      document.getElementById('pin').innerHTML = pin;
    }
    if (Object.hasOwn(msg, 'mouse'))
    {
      if (Number(msg['mouse']) == 0) {led_ &= ~4;} else {led_ |= 4;}
      if (Number(msg['wifi']) == 0) {led_ &= ~2;} else {led_ |= 2;}
      if (Number(msg['ap']) == 0) {led_ &= ~1;} else {led_ |= 1;}
      show_led();
    }
    if (Object.hasOwn(msg, 'title'))
    {
      document.getElementById('title').value = msg['title'];
    }
}
  catch(e)
  {
    console.log(e);
  }
  ping_ = true;
}

function scan_wifi()
{
  document.getElementById('ssids').innerHTML = '<option>-- Scanning --</option>';
  sendToWS('func=scan_wifi')
}

function config_update()
{
  document.getElementById('ip').innerHTML = '';
  let cmd = 'func=config_update';
  let inps = document.querySelectorAll('input');
  for (let inp of inps)
  {
    cmd += ' ' + inp.name + '=' + encodeURI(inp.value);
    console.log(cmd);
  }
  sendToWS(cmd);
}
function ssid_select()
{
  let sel = document.getElementById('ssids');
  let idx = sel.selectedIndex;
  if (idx > 0)
  {
    let ssid = document.getElementById('ssid');
    ssid.value = sel.options[idx].value;
  }
}

function show_led()
{
  let color = 'red';
  if ((led_ & 8) == 0)
  {
    color = 'orange';
    if ((led_ & 2) == 0)
    {
      color = 'yellow';
      if ((led_ & 1) == 0)
      {
        color = "lightgreen"
      }
    }
  }

  let html =  "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
  "<circle cx='12' cy='12' r='9' fill='" + color + "' stroke='white' stroke-width='3' /></svg>";

  let led = document.getElementById("led1");
  led.innerHTML = html;

  color = 'red';
  if ((led_ & 8) == 0)
  {
    color = 'blue';
    if ((led_ & 4) == 0)
    {
      color = "lightgreen"
    }
  }

  html =  "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
  "<circle cx='12' cy='12' r='9' fill='" + color + "' stroke='white' stroke-width='3' /></svg>";

  led = document.getElementById("led2");
  led.innerHTML = html;
}

function checkConnection()
{
  if (isWSOpen())
  {
    if (ping_)
    {
      ping_ = false;
      sendToWS('func=get_state')
    }
    else
    {
      closeWS();
      openWS();
      ping_ = true;
    }
  }
}