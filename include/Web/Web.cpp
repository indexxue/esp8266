#include "Web.h"

#include <AppSta.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

namespace {

WebJsonFn s_statusFn = nullptr;
WebJsonFn s_frameFn = nullptr;
WebSettingsPostFn s_settingsPostFn = nullptr;

AsyncWebServer* s_server = nullptr;

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>环境监测</title>
<style>
:root{--bg:#0f1419;--card:#1a2332;--bd:#2a3648;--tx:#e8eef7;--tx2:#8b9cb3;--acc:#3d9eff;--ok:#3ecf8e;--warn:#f0c14b}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--tx);min-height:100vh;line-height:1.5}
header{padding:1rem 1.25rem;border-bottom:1px solid var(--bd);display:flex;flex-wrap:wrap;align-items:center;gap:.75rem;justify-content:space-between}
header h1{font-size:1.1rem;font-weight:600;margin:0}
.badge{font-size:.75rem;padding:.2rem .55rem;border-radius:999px;background:var(--card);border:1px solid var(--bd);color:var(--tx2)}
.badge.on{border-color:var(--ok);color:var(--ok)}
main{padding:1rem;max-width:56rem;margin:0 auto}
.grid{display:grid;gap:.75rem;grid-template-columns:repeat(auto-fill,minmax(10.5rem,1fr))}
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:1rem;display:flex;flex-direction:column;gap:.35rem;min-height:6.5rem}
.card h2{font-size:.8rem;font-weight:500;color:var(--tx2);margin:0;text-transform:none;letter-spacing:.02em}
.val{font-size:1.75rem;font-weight:700;font-variant-numeric:tabular-nums;line-height:1.2}
.val small{font-size:.95rem;font-weight:500;color:var(--tx2);margin-left:.15rem}
.sub{font-size:.75rem;color:var(--tx2);min-height:1.1em}
.meter{height:6px;background:var(--bd);border-radius:4px;overflow:hidden;margin-top:.25rem}
.meter .bar{display:block;height:100%;background:linear-gradient(90deg,var(--acc),#7ab8ff);width:0%;transition:width .35s ease}
.row-hum .meter .bar{background:linear-gradient(90deg,#5ad4a8,var(--ok))}
.row-audio .meter .bar{background:linear-gradient(90deg,#a78bfa,var(--acc))}
.row-water .meter .bar{background:linear-gradient(90deg,#38bdf8,#0ea5e9)}
.state-pill{display:inline-flex;align-items:center;gap:.35rem;font-size:.9rem;font-weight:600;margin-top:.2rem}
.state-pill .dot{width:8px;height:8px;border-radius:50%;background:var(--tx2);flex-shrink:0}
.state-pill.run .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.state-pill.stop .dot{background:var(--warn)}
details{margin-top:1.25rem;border:1px solid var(--bd);border-radius:10px;background:var(--card);padding:.5rem 1rem}
details summary{cursor:pointer;font-size:.8rem;color:var(--tx2)}
pre{margin:.5rem 0 0;font-size:11px;overflow:auto;color:var(--tx2);white-space:pre-wrap;word-break:break-all}
footer{margin-top:1rem;font-size:.72rem;color:var(--tx2);text-align:center}
.err{outline:1px solid #f87171}
.setpanel{border:1px solid var(--bd);border-radius:12px;background:var(--card);padding:1rem;margin-top:1rem}
.panelh{font-size:.85rem;margin:0 0 .75rem;color:var(--tx2);display:flex;justify-content:space-between;align-items:center;gap:.5rem;flex-wrap:wrap}
.row{display:flex;flex-wrap:wrap;gap:.65rem;align-items:flex-end;margin-bottom:.5rem}
.lbl{display:flex;flex-direction:column;font-size:.72rem;color:var(--tx2);gap:.2rem;min-width:7rem}
input[type=number]{width:5rem}
input,select{background:var(--bg);color:var(--tx);border:1px solid var(--bd);border-radius:8px;padding:.35rem .5rem;font-size:.9rem}
button{background:var(--acc);color:#081018;border:0;border-radius:8px;padding:.45rem .9rem;font-weight:600;cursor:pointer}
button:disabled{opacity:.45;cursor:not-allowed}
.msg{margin-top:.35rem;font-size:.75rem;color:var(--tx2)}
.quickline{font-size:.88rem;color:var(--tx2);padding:.35rem 0 1rem;line-height:1.55;border-bottom:1px solid var(--bd);margin-bottom:.25rem}
.quickline strong{color:var(--tx);font-weight:650;font-variant-numeric:tabular-nums}
.btn-wifi-off{font-size:.72rem;padding:.28rem .55rem;background:var(--card);color:var(--tx2);border:1px solid var(--bd);border-radius:8px;cursor:pointer;font-weight:500}
.btn-wifi-off:hover{border-color:var(--warn);color:var(--warn)}
</style>
</head>
<body>
<header>
<h1>环境监测</h1>
<div style="display:flex;flex-wrap:wrap;align-items:center;gap:.5rem">
<span id="wifiBadge" class="badge">WiFi …</span>
<button type="button" id="btnWifiDisc" class="btn-wifi-off" title="断开 STA；约 10s 内可用串口配网">断开 WiFi</button>
</div>
</header>
<main>
<p class="quickline" id="quickLine">湿度 <strong>--</strong> · 水位 <strong>--</strong> · 音量 <strong>--</strong>（等待遥测）</p>
<div class="grid" id="board"></div>
<section class="setpanel" id="setPanel">
<h2 class="panelh">设备设定 <span id="syncTag" class="badge">…</span></h2>
<div class="row">
<label class="lbl">目标湿度 %<input type="number" id="inTarget" min="0" max="100" value="50"></label>
<label class="lbl">运行<input type="checkbox" id="inOn"></label>
<label class="lbl">挡位<input type="number" id="inGear" min="0" max="255" value="0"></label>
<button type="button" id="btnApply">下发到 MCU</button>
</div>
<p class="msg" id="setMsg"></p>
</section>
<details id="rawBox">
<summary>原始帧 / JSON（调试）</summary>
<pre id="rawPre">加载中…</pre>
</details>
<footer>GET <code>/api/status</code>；POST <code>/api/settings</code> 下发后需等 STM32 回 <code>0x22</code> ACK；POST <code>/api/wifi/disconnect</code> 断开 STA（见 <code>doc/esp8266_stm32_link_proto.md</code>）。</footer>
</main>
<script>
(function(){
  function el(t,a,ch){
    const n=document.createElement(t);
    if(a)for(const k in a){
      if(k==='class')n.className=a[k];
      else if(k==='html')n.innerHTML=a[k];
      else n.setAttribute(k,a[k]);
    }
    if(ch)ch.forEach(function(c){
      if(typeof c==='string')n.appendChild(document.createTextNode(c));
      else if(c)n.appendChild(c);
    });
    return n;
  }
  function card(key,title,extraClass){
    const h2=el('h2',{},[title]);
    const val=el('div',{class:'val',id:'v-'+key},['--']);
    const sub=el('div',{class:'sub',id:'s-'+key},['']);
    const bar=el('span',{class:'bar',id:'m-'+key});
    const meter=el('div',{class:'meter'},[bar]);
    return el('article',{class:'card row-'+key+' '+(extraClass||'')},[h2,val,sub,meter]);
  }
  function pillRow(){
    const wrap=el('div',{class:'card',id:'c-device'},[]);
    wrap.appendChild(el('h2',{},['加湿器']));
    const line=el('div',{class:'state-pill stop',id:'humState'},[]);
    line.appendChild(el('span',{class:'dot'}));
    line.appendChild(el('span',{id:'humStateTxt'},['读取中…']));
    wrap.appendChild(line);
    wrap.appendChild(el('div',{class:'sub',id:'gearLine'},['挡位：--']));
    return wrap;
  }
  const board=document.getElementById('board');
  board.appendChild(card('hum','湿度','row-hum'));
  board.appendChild(card('audio','音量','row-audio'));
  board.appendChild(card('water','水位','row-water'));
  board.appendChild(pillRow());
  function pct(n,max){const x=Math.max(0,Math.min(max,Number(n)||0));return Math.round(x/max*100);}
  function setMeter(id,v,max){const e=document.getElementById('m-'+id);if(e)e.style.width=pct(v,max)+'%';}
  function setCard(id,main,unit,sub,vForMeter,maxMeter){
    const vn=document.getElementById('v-'+id),sn=document.getElementById('s-'+id);
    if(vn){vn.textContent='';vn.appendChild(document.createTextNode(main));if(unit)vn.appendChild(el('small',{},[unit]));}
    if(sn)sn.textContent=sub||'';
    if(vForMeter!=null)setMeter(id,vForMeter,maxMeter||100);
  }
  function setWifi(d){
    const b=document.getElementById('wifiBadge');
    if(!b)return;
    if(d.wifi_connected){b.className='badge on';b.textContent='WiFi 已连接 '+d.ip+' · RSSI '+d.rssi+' dBm';}
    else{b.className='badge';b.textContent='WiFi 未连接';}
  }
  function volFromSensor(s){
    if(!s)return null;
    if(s.volume_level!=null&&s.volume_level!==undefined)return s.volume_level;
    return s.audio_level;
  }
  function updateQuickLine(s){
    const el=document.getElementById('quickLine');
    if(!el)return;
    if(!s||!s.valid){
      el.innerHTML='湿度 <strong>--</strong> · 水位 <strong>--</strong> · 音量 <strong>--</strong>（等待 0x20 遥测）';
      return;
    }
    const v=volFromSensor(s);
    let html='湿度 <strong>'+s.humidity_pct+'%</strong> · 水位 <strong>'+s.water_level_pct+'%</strong> · 音量 <strong>'+String(v)+'</strong>';
    if(s.target_humidity_pct!=null&&s.target_humidity_pct!==undefined){
      html+=' · 目标湿度 <strong>'+s.target_humidity_pct+'%</strong>';
    }
    el.innerHTML=html;
  }
  function setDevice(s){
    const line=document.getElementById('humState');
    const txt=document.getElementById('humStateTxt');
    const gear=document.getElementById('gearLine');
    if(!line||!txt||!gear)return;
    if(!s||!s.valid){
      line.className='state-pill stop';
      txt.textContent=(s&&s.reason)?s.reason:'无数据';
      gear.textContent='挡位：--';
      setCard('hum','--','%','等待有效遥测帧',0,100);
      setCard('audio','--','','等待有效遥测帧',0,255);
      setCard('water','--','%','等待有效遥测帧',0,100);
      updateQuickLine(null);
      return;
    }
    const on=!!s.humidifier_on;
    line.className='state-pill '+(on?'run':'stop');
    txt.textContent=on?'工作中':'已停止';
    gear.textContent='挡位：'+String(s.gear);
    var humSub='相对湿度 0–100';
    if(s.target_humidity_pct!=null&&s.target_humidity_pct!==undefined) humSub+=' · 遥测含目标 '+s.target_humidity_pct+'%';
    setCard('hum',String(s.humidity_pct),'%',humSub,s.humidity_pct,100);
    const vol=volFromSensor(s);
    setCard('audio',String(vol),'','音量 0–255（越大越强）',vol,255);
    setCard('water',String(s.water_level_pct),'%','液位/水箱 0–100%',s.water_level_pct,100);
    updateQuickLine(s);
  }
  function applyStatusMeta(d){
    var st=d.settings||{};
    var sy=d.sync||{};
    var tgt=document.getElementById('inTarget');
    var on=document.getElementById('inOn');
    var gr=document.getElementById('inGear');
    var tag=document.getElementById('syncTag');
    var btn=document.getElementById('btnApply');
    var msg=document.getElementById('setMsg');
    if(tgt&&st.valid) tgt.value=String(st.target_humidity_pct);
    if(on&&st.valid) on.checked=!!st.humidifier_on;
    if(gr&&st.valid) gr.value=String(st.gear);
    if(btn) btn.disabled=!!(sy&&sy.pending);
    if(tag){
      if(sy&&sy.pending){ tag.textContent='等待 MCU ACK…'; tag.className='badge';}
      else if(sy&&sy.last_error){ tag.textContent=sy.last_error; tag.className='badge err';}
      else { tag.textContent='已同步'; tag.className='badge on';}
    }
    if(msg){
      if(sy&&sy.last_error && !sy.pending) msg.textContent=sy.last_error;
      else if(sy&&sy.pending&&sy.requested) msg.textContent='已请求 req_id='+sy.req_id+' ，超时见 deadline_ms';
      else msg.textContent='';
    }
  }
  async function tick(){
    try{
      const r=await fetch('/api/status',{cache:'no-store'});
      const t=await r.text();
      const d=JSON.parse(t);
      setWifi(d);
      setDevice(d.sensor);
      applyStatusMeta(d);
      document.getElementById('rawPre').textContent=JSON.stringify(d,null,2);
    }catch(e){
      document.getElementById('rawPre').textContent=String(e);
      updateQuickLine(null);
    }
  }
  document.getElementById('btnApply').addEventListener('click',async function(){
    var msg=document.getElementById('setMsg');
    try{
      var body={
        target_humidity_pct:parseInt(document.getElementById('inTarget').value,10)||0,
        humidifier_on:document.getElementById('inOn').checked,
        gear:parseInt(document.getElementById('inGear').value,10)||0
      };
      var r=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
      var j=await r.json();
      if(!j.ok) msg.textContent=j.error||'失败';
      else msg.textContent='已发送，等待 0x22 ACK…';
    }catch(e){ msg.textContent=String(e); }
  });
  document.getElementById('btnWifiDisc').addEventListener('click',async function(){
    var msg=document.getElementById('setMsg');
    try{
      var r=await fetch('/api/wifi/disconnect',{method:'POST'});
      var j=await r.json();
      if(j&&j.ok) msg.textContent='已请求断开 WiFi';
      else msg.textContent=(j&&j.error)||'断开失败';
    }catch(e){ if(msg) msg.textContent=String(e); }
  });
  setInterval(tick,1000);
  tick();
})();
</script>
</body>
</html>
)HTML";

}  // namespace

bool webIsRunning() {
  return s_server != nullptr;
}

void webBegin(uint16_t port, WebJsonFn statusJson, WebJsonFn lastFrameJson) {
  webBegin(port, statusJson, lastFrameJson, nullptr);
}

void webBegin(uint16_t port, WebJsonFn statusJson, WebJsonFn lastFrameJson, WebSettingsPostFn settingsPost) {
  if (s_server != nullptr) {
    return;
  }
  s_statusFn = statusJson;
  s_frameFn = lastFrameJson;
  s_settingsPostFn = settingsPost;

  s_server = new AsyncWebServer(port);

  s_server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  s_server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!s_statusFn) {
      request->send(500, "text/plain", "no handler");
      return;
    }
    const String j = s_statusFn();
    request->send(200, "application/json; charset=utf-8", j);
  });

  s_server->on("/api/lastframe", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!s_frameFn) {
      request->send(500, "text/plain", "no handler");
      return;
    }
    const String j = s_frameFn();
    request->send(200, "application/json; charset=utf-8", j);
  });

  if (s_settingsPostFn) {
    s_server->on(
        "/api/settings", HTTP_POST,
        [](AsyncWebServerRequest* /*request*/) {},
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
          static String body;
          if (index == 0) {
            body = "";
          }
          for (size_t i = 0; i < len; ++i) {
            body += static_cast<char>(data[i]);
          }
          if (index + len == total) {
            const String resp = s_settingsPostFn ? s_settingsPostFn(body) : String("{\"ok\":false,\"error\":\"no_handler\"}");
            request->send(200, "application/json; charset=utf-8", resp);
          }
        });
  }

  s_server->on("/api/wifi/disconnect", HTTP_POST, [](AsyncWebServerRequest* request) {
    appStaUserDisconnect();
    request->send(200, "application/json; charset=utf-8", F("{\"ok\":true,\"wifi\":\"disconnect_requested\"}"));
  });

  s_server->onNotFound([](AsyncWebServerRequest* request) { request->send(404, "text/plain", "404"); });

  s_server->begin();
}
