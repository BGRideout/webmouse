var x_;
var y_;
var dx_ = 0;
var dy_ = 0;
var dw_ = 0;
var l_ = 0;
var r_ = 0;
var c_ = '';
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
  //kbd.addEventListener('keydown', (e) => {keystroke(e, 'down');});
  kbd.addEventListener('keyup', (e) => {keystroke(e, 'up');});
  //kbd.addEventListener('keypress', (e) => {keystroke(e, 'press');});
  kbd.addEventListener('input', (e) => {keystroke(e, 'input');});
  openWS();
  setInterval(checkReport, 100);
});
function t_start(e)
{
  e.preventDefault();
  console.log(e);
  let t1 = e.targetTouches[0];
  x_ = t1.pageX;
  y_ = t1.pageY;
}
function t_move(e)
{
  console.log(e);
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
    let txt = 'func=keyboard c=' + cod;
    sendToWS(txt);
  }
  else
  {
    c_ = e.data;
    e.srcElement.value = '';
  }
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
function process_ws_message(evt)
{
}

