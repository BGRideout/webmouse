document.addEventListener('DOMContentLoaded', function()
{
  document.addEventListener('ws_state', ws_state_change);
  document.getElementById('scan').addEventListener('click', scan_wifi);
  document.getElementById('update').addEventListener('click', config_update);
  document.getElementById('ssids').addEventListener('change', ssid_select);
  openWS();
});
function ws_state_change(evt)
{
  if (evt.detail.obj['open'])
  {
    sendToWS('func=get_wifi');
  }
}
function process_ws_message(evt)
{
  let pin = '';
  try
  {
    let msg = JSON.parse(evt.data);
    console.log(msg);
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
      pin = msg['pin'];
    }
  }
  catch(e)
  {
    console.log(e);
  }
  document.getElementById('pin').innerHTML = pin;
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
  for (inp of inps)
  {
    cmd += ' ' + inp.name + '=' + inp.value;
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

