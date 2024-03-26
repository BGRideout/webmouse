var x_;
var y_;
var dx_ = 0;
var dy_ = 0;
var dw_ = 0;
var l_ = 0;
var r_ = 0;
var c_ = '';
var xkshow = 0;
var xktimer = -1;
var ctrl_ = 0;
var alt_ = 0;
var msgtimer_ = -1;
var prevHeight_ = 0;
var prevWidth_ = 0;
var led_ = 0x0f;
var ping_ = true;

document.addEventListener('DOMContentLoaded', function()
{
  let ma = document.getElementById('mousearea');
  ma.addEventListener('touchstart', t_start);
  ma.addEventListener('touchmove', t_move);
  let btn = document.getElementById('left');
  btn.addEventListener('touchstart', (e) => {l_ = 1; report();});
  btn.addEventListener('touchend', (e) => {l_ = 0; report();});
  btn.addEventListener('touchcancel', (e) => {l_ = 0; report();});
  btn = document.getElementById('right');
  btn.addEventListener('touchstart', (e) => {r_ = 1; report();});
  btn.addEventListener('touchend', (e) => {r_ = 0; report();});
  btn.addEventListener('touchcancel', (e) => {r_ = 0; report();});
  let kbd = document.getElementById('kbd');
  kbd.addEventListener('keyup', (e) => {keystroke(e, 'up');});
  kbd.addEventListener('input', (e) => {keystroke(e, 'input');});
  let kbd2 = document.getElementById('kbd2');
  kbd2.addEventListener('keyup', (e) => {keystroke(e, 'up');});
  kbd2.addEventListener('input', (e) => {keystroke(e, 'input');});

  let btns = document.querySelectorAll(".extkbd button");
  for (btn of btns)
  {
    btn.addEventListener('keydown', (e) => {e.preventDefault();});
    btn.addEventListener('click', ekbtn);
  }

  btns = document.querySelectorAll(".av_control button");
  for (btn of btns)
  {
    btn.addEventListener('pointerdown', avbtndown);
    btn.addEventListener('pointerup', avbtnup);
  }

  if ('virtualKeyboard' in navigator)
  {
    let ma = document.getElementById('mousearea');
    navigator.virtualKeyboard.overlaysContent = true;
    navigator.virtualKeyboard.addEventListener('geometrychange', kbdshow);
    post_message('Virtual keyboard API exists');
  }
  else
  {
    kbd.addEventListener('focus', focused);
    kbd2.addEventListener('focus', focused);
    // kbd.addEventListener('blur', blurred);
    // let ekbd = document.getElementById('extkbd');
    // ekbd.addEventListener('focusin', focused);
    // ekbd.addEventListener('focusout', blurred);
    prevHeight_ = window.visualViewport.height;
    prevWidth_ = window.visualViewport.width;
    window.addEventListener('resize', window_resized);
  }

  document.addEventListener('ws_state', ws_state_change);
  document.addEventListener('ws_message', process_ws_message);
  openWS();

  setInterval(checkReport, 100);
  setInterval(checkConnection, 10000);
});

function t_start(e)
{
  e.preventDefault();
  let t1 = e.targetTouches[0];
  x_ = t1.pageX;
  y_ = t1.pageY;
}

function t_move(e)
{
  let t1 = e.targetTouches[0];
  if (e.targetTouches.length == 1)
  {
    dx_ += Math.round(t1.pageX - x_);
    dy_ += Math.round(t1.pageY - y_);
  }
  else
  {
    dw_ += Math.round(t1.pageY - y_);
  }
  x_ = t1.pageX;
  y_ = t1.pageY;
}

function keystroke(e, updown)
{
  if (updown !== 'input')
  {
    let cod = e.which;
    if (e.which == 229 && c_.length > 0)
    {
      cod = c_.charCodeAt(0)
    }
    let txt = 'func=keyboard c=' + cod + ' ctrl=' + ctrl_ + ' alt=' + alt_;
    sendToWS(txt);
    reset_modifiers();
  }
  else
  {
    c_ = e.data;
    e.srcElement.value = '';
  }
}

function ekbtn(evt)
{
  let code = Number(evt.srcElement.getAttribute('code'));
  let kbd = document.getElementById('kbd2');
  if (kbd.style.visibility != 'visible')
  {
    kbd = document.getElementById('kbd');
  }
  kbd.focus();
  if (code == -1)
  {
    ctrl_ ^= 1;
    show_modifiers();
  }
  else if (code == -4)
  {
    alt_ ^= 1;
    show_modifiers();
  }
  else if (code != 0)
  {
    let txt = 'func=keyboard c=' + code + ' ctrl=' + ctrl_ + ' alt=' + alt_;
    sendToWS(txt);
    reset_modifiers();
  }
}

function avbtndown(evt)
{
  console.log(evt);
  let code = evt.srcElement.getAttribute('code');
  let txt = 'func=av_control code=' + code;
  sendToWS(txt);
}

function avbtnup(evt)
{
  console.log(evt);
  let txt = 'func=av_control code=none';
  sendToWS(txt);
}

function show_modifiers()
{
  let color = ['black', 'red'];
  let btn = document.getElementById('ctrlbtn');
  btn.style.color = color[ctrl_];
  btn = document.getElementById('altbtn');
  btn.style.color = color[alt_];
}

function reset_modifiers()
{
  ctrl_ = 0;
  alt_ = 0;
  show_modifiers();
}

function checkReport()
{
  if (dx_ != 0 || dy_ != 0 || dw_ != 0) report();
}

function report()
{
  let txt = 'func=mouse x=' + dx_ + ' y=' + dy_ + ' w=' + dw_ + ' l=' + l_ + ' r=' + r_;
  //let ma = document.getElementById('mousearea');
  //ma.innerHTML = txt;
  dx_ = 0;
  dy_ = 0;
  dw_ = 0;
  sendToWS(txt);
}

// Used when VirtualKeyboard interface availble (https)
function kbdshow(evt)
{
  let ekbd = document.getElementById('extkbd');
  const { x, y, width, height } = evt.target.boundingRect;
  if (height > 16)
  {
    show_ekbd();
  }
  else
  {
    hide_ekbd();
  }
}

// When no VirtualKeyboard, show extended keys on kbd focused and
// hide when window expands again and stays so for 100 ms
function focused(evt)
{
  show_ekbd();
  prevWidth_ = window.visualViewport.width;
  clearTimeout(xktimer);
}

function window_resized(evt)
{
  let w = window.visualViewport.width;
  if (w == prevWidth_)
  {
    let chg = window.visualViewport.height - prevHeight_;
    if (chg > 60)
    {
      xktimer = setTimeout(xktimeout, 100);
    }
    else
    {
      prevHeight_ = window.visualViewport.height;
    }
  }
  else
  {
    prevWidth_ = w;
    hide_ekbd();
  }
}

function xktimeout()
{
  let chg = window.visualViewport.height - prevHeight_;
  prevHeight_ = window.visualViewport.height;
  if (chg != 0)
  {
    hide_ekbd();
    document.getElementById('kbd').blur();
  }
}

function show_ekbd()
{
  let av = document.getElementById('av_control');
  av.style.display = 'none';
  let ekbd = document.getElementById('extkbd');
  ekbd.style.display = "grid";
  if (window.visualViewport.height < 400)
  {
    let kbd2 = document.getElementById('kbd2');
    kbd2.style.visibility = 'visible';
    kbd2.focus();
    ekbd.scrollIntoView({block: "bottom"});
  }
  else
  {
    window.scrollTo(0, document.body.scrollHeight);
  }
}

function hide_ekbd()
{
  let ekbd = document.getElementById('extkbd');
  ekbd.style.display = 'none';
  let av = document.getElementById('av_control');
  av.style.display = 'grid';
  let kbd2 = document.getElementById('kbd2');
  kbd2.style.visibility = 'hidden';
  reset_modifiers();
  window.scrollTo(0, 0);
}

function post_message(msg, tmo=10)
{
  clearTimeout(msgtimer_);
  let msga = document.getElementById('msgarea');
  msga.innerHTML = msg;
  if (tmo > 0)
  {
    msgtimer_ = setTimeout(post_message, tmo * 1000, '', 0);
  }
}

function ws_state_change(evt)
{
  if (evt.detail.obj['open'])
  {
    sendToWS('func=get_state');
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
    console.log(msg);
    if (Object.hasOwn(msg, 'mouse'))
    {
      if (Number(msg['mouse']) == 0) {led_ &= ~4;} else {led_ |= 4;}
      if (Number(msg['wifi']) == 0) {led_ &= ~2;} else {led_ |= 2;}
      if (Number(msg['ap']) == 0) {led_ &= ~1;} else {led_ |= 1;}
      show_led();
    }
    if (Object.hasOwn(msg, 'mute'))
    {
      let btn = document.getElementById('mutebtn');
      if (msg['mute'] == 'true')
      {
        btn.style.background = "lightpink";
      }
      else
      {
        btn.style.background = document.getElementById('left').style.background;
      }
    }
    if (Object.hasOwn(msg, 'pin'))
    {
      post_message('pin = ' + msg['pin'], 30);
    }
  }
  catch(e)
  {
    console.log(e);
  }
  ping_ = true;
}

function show_led()
{
  let color = 'red';
  if ((led_ & 8) == 0)
  {
    color = 'blue';
    if ((led_ & 4) == 0)
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
  }

  let html =  "<svg viewbox='0 0 25 25' width='25' height='25' xmlns='http://www.w3.org/2000/svg'>" +
  "<circle cx='12' cy='12' r='9' fill='" + color + "' stroke='white' stroke-width='3' /></svg>";

  let led = document.getElementById("led");
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
