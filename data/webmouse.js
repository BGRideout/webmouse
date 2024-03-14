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
var prevHeight_ = window.visualViewport.height;

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

  let btns = document.querySelectorAll(".extkbd button");
  for (btn of btns)
  {
    btn.addEventListener('keydown', (e) => {e.preventDefault();});
    btn.addEventListener('click', ekbtn);
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
    kbd.addEventListener('blur', blurred);
    let ekbd = document.getElementById('extkbd');
    ekbd.addEventListener('focusin', focused);
    ekbd.addEventListener('focusout', blurred);
    window.addEventListener('resize', window_resized);
  }
  openWS();
  setInterval(checkReport, 100);

/*   screen.orientation.lock('portrait')
  .then((val) =>
  {
    let msg = 'Orientation locked: ' + val;
    post_message(msg);
    console.log(msg);
  })
  .catch((e) => 
  {
    let msg = 'Eception setting orientation: ' + e;
    post_message(msg);
    console.log(msg);
  });
 */
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
  console.log(code);
  let kbd = document.getElementById('kbd');
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

function kbdshow(evt)
{
  let ekbd = document.getElementById('extkbd');
  const { x, y, width, height } = evt.target.boundingRect;
  if (height > 16)
  {
    ekbd.style.visibility = "visible";
    window.scrollTo(0, document.body.scrollHeight);
  }
  else
  {
    ekbd.style.visibility = 'hidden';
    reset_modifiers();
    window.scrollTo(0, 0);
  }
}

function window_resized(evt)
{
  let chg = window.visualViewport.height - prevHeight_;
  //post_message('Size change ' + chg + '  ' + prevHeight_ + ' to ' + window.visualViewport.height);
  prevHeight_ = window.visualViewport.height;
  if (chg > 40)
  {
    let ekbd = document.getElementById('extkbd');
    ekbd.style.visibility = 'hidden';
    reset_modifiers();
    window.scrollTo(0, 0);
    document.getElementById('kbd').blur();
  }
}

function focused(evt)
{
  let ekbd = document.getElementById('extkbd');
  ekbd.style.visibility = 'visible';
  xkshow += 1;
  clearTimeout(xktimer);
}

function blurred(evt)
{
  xkshow -= 1;
  if (xkshow <= 0)
  {
    xkshow = 0
    setTimeout(xktimeout, 250);
  }
}

function xktimeout()
{
  if (xkshow == 0)
  {
    let ekbd = document.getElementById('extkbd');
    ekbd.style.visibility = "hidden";
    reset_modifiers();
    window.scrollTo(0, 0);
  }
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

function process_ws_message(evt)
{
}

