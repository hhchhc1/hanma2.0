#include "smart_home_web_server.h"
#include "display/extra_screens.h"
#include "sensors/dht22.h"
#include "sensors/bh1750.h"
#include "board.h"
#include "display.h"
#include "application.h"
#include "device_state.h"

#include <cstring>
#include <ctime>
#include <mutex>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#include <esp_heap_caps.h>
#include <cJSON.h>
#include <sys/socket.h>

#include "matter_device/matter_device.h"
#include "matter_device/smart_home_matter_init.h"

#ifdef CONFIG_IDF_TARGET_ESP32P4
#include "camera/camera_display.h"
#include <esp_jpeg_enc.h>
#endif

#define TAG "SmartHomeWeb"
#define MAX_CHAT_MESSAGES 200

static httpd_handle_t s_server = NULL;
static bool s_server_running = false;

static std::mutex s_msg_mutex;
static uint32_t s_msg_id = 0;

// Room 2 sensor data (from ESP32-C3)
static float s_room2_temp = -1;
static float s_room2_humid = -1;
static float s_room2_light = -1;
static int64_t s_room2_last_update = 0;
static std::mutex s_room2_mutex;

struct ChatMessage {
    std::string role;
    std::string text;
    uint32_t id;
};
static std::vector<ChatMessage> s_messages;

static void on_chat_message(const std::string& role, const std::string& text)
{
    std::lock_guard<std::mutex> lock(s_msg_mutex);
    if (s_messages.size() >= MAX_CHAT_MESSAGES) {
        s_messages.erase(s_messages.begin());
    }
    s_messages.push_back({role, text, ++s_msg_id});
}

static const char *HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>XiaoZhi Smart Home</title>
<style>
:root{--bg:#0a0a14;--card:#14142a;--card-hover:#1c1c3a;--border:#2a2a4a;--text:#e8e8f0;--muted:#6868a0;--accent:#4fc3f7;--accent2:#7c4dff;--green:#2ecc71;--red:#e74c3c;--orange:#f39c12;--shadow:0 8px 32px rgba(0,0,0,0.4)}
body.room2{--bg:#1a0a14;--card:#2a1424;--card-hover:#3a1c34;--border:#4a2a44;--text:#f0e0e8;--muted:#a06890;--accent:#e040a0;--accent2:#ff6bc1}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;overflow-x:hidden;transition:background 0.5s ease}
.tab-bar{position:fixed;bottom:0;left:0;right:0;background:rgba(10,10,20,0.92);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);border-top:1px solid var(--border);display:flex;z-index:100;padding:4px 0 env(safe-area-inset-bottom)}
.tab-item{flex:1;display:flex;flex-direction:column;align-items:center;padding:6px 0;font-size:10px;color:var(--muted);cursor:pointer;transition:all 0.2s;border:none;background:none;outline:none;gap:1px}
.tab-item.active{color:var(--accent)}
.tab-item .tab-icon{font-size:22px;line-height:1.2;transition:transform 0.2s}
.tab-item.active .tab-icon{transform:scale(1.1)}
.tab-item .tab-label{font-size:10px;font-weight:500;letter-spacing:0.3px}
.page-wrap{max-width:480px;margin:0 auto;padding:0 0 70px 0}
.page{display:none;padding:16px 16px 0;animation:fadeSlideIn 0.35s ease}
.page.active{display:block}
@keyframes fadeSlideIn{from{opacity:0;transform:translateY(12px)}to{opacity:1;transform:translateY(0)}}
.header{display:flex;justify-content:space-between;align-items:center;margin-bottom:18px;padding-top:4px}
.header h1{font-size:22px;font-weight:700;background:linear-gradient(135deg,#e8e8f0 0%,#4fc3f7 100%);-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.status-badge{display:flex;align-items:center;gap:6px;font-size:11px;color:var(--muted);background:var(--card);border-radius:20px;padding:5px 14px;border:1px solid var(--border);white-space:nowrap}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--green);flex-shrink:0;animation:pulse 2s infinite}
@keyframes pulse{0%,100%{box-shadow:0 0 0 0 rgba(46,204,113,0.4)}50%{box-shadow:0 0 0 6px rgba(46,204,113,0)}}
.room-switch{display:flex;align-items:center;gap:10px;margin-bottom:16px;padding:8px 12px;background:var(--card);border-radius:14px;border:1px solid var(--border)}
.room-switch .rs-label{font-size:13px;font-weight:600;color:var(--text);flex:1}
.room-switch .rs-btn{flex-shrink:0;padding:8px 18px;border-radius:20px;border:none;font-size:13px;font-weight:600;cursor:pointer;transition:all 0.3s;touch-action:manipulation;background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;outline:none}
.room-switch .rs-btn:active{transform:scale(0.95)}
.room-switch .rs-btn.room1{background:linear-gradient(135deg,#4fc3f7,#1565c0);color:#fff;box-shadow:0 3px 12px rgba(79,195,247,0.3)}
.room-switch .rs-btn.room2{background:linear-gradient(135deg,#e040a0,#7b1fa2);color:#fff;box-shadow:0 3px 12px rgba(224,64,160,0.3)}
.sensors{display:flex;gap:10px;margin-bottom:16px}
.sensor-card{flex:1;background:var(--card);border-radius:16px;padding:14px 8px;text-align:center;border:1px solid var(--border);position:relative;overflow:hidden;transition:all 0.3s}
body.room2 .sensor-card{background:var(--card)}
.sensor-card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;border-radius:16px 16px 0 0}
.sensor-card:nth-child(1)::before{background:linear-gradient(90deg,#ff6b6b,#ee5a24)}
.sensor-card:nth-child(2)::before{background:linear-gradient(90deg,#4fc3f7,#1565c0)}
.sensor-card:nth-child(3)::before{background:linear-gradient(90deg,#f9a825,#ff6f00)}
.sensor-icon{width:36px;height:36px;border-radius:18px;display:flex;align-items:center;justify-content:center;margin:0 auto 8px;font-size:15px;font-weight:700;color:#fff}

.sensor-value{font-size:22px;font-weight:700;letter-spacing:-0.5px}
.sensor-unit{font-size:13px;color:var(--muted);font-weight:400}
.sensor-label{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:1.2px;margin-top:4px}
.quick-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:16px}
.quick-card{background:var(--card);border-radius:16px;padding:16px;border:1px solid var(--border);cursor:pointer;transition:all 0.25s;touch-action:manipulation;user-select:none;text-align:center;position:relative;overflow:hidden}
.quick-card:active{transform:scale(0.96)}
.quick-card .qc-icon{font-size:28px;margin-bottom:6px;display:block;transition:transform 0.3s}
.quick-card:active .qc-icon{transform:scale(1.2)}
.quick-card .qc-label{font-size:13px;font-weight:600;color:var(--text)}
.quick-card .qc-state{font-size:11px;color:var(--muted);margin-top:4px}
.quick-card .qc-badge{position:absolute;top:8px;right:8px;font-size:9px;padding:2px 8px;border-radius:10px;font-weight:600}
.quick-card.qc-fan-on{background:linear-gradient(135deg,#1565c0,#0d47a1);border-color:#1976d2;box-shadow:0 4px 20px rgba(25,118,210,0.3)}
.quick-card.qc-fan-on .qc-label,.quick-card.qc-fan-on .qc-state{color:#fff}
.quick-card.qc-light-on{background:linear-gradient(135deg,#f9a825,#f57f17);border-color:#f9a825;box-shadow:0 4px 20px rgba(249,168,37,0.3)}
.quick-card.qc-light-on .qc-label,.quick-card.qc-light-on .qc-state{color:#000}
.badge-on{background:rgba(46,204,113,0.2);color:var(--green)}
.badge-off{background:rgba(100,100,160,0.2);color:var(--muted)}
.qc-fan-on .qc-badge.badge-on,.qc-light-on .qc-badge.badge-on{background:rgba(255,255,255,0.25);color:#fff}
.mode-btn{width:100%;border:none;border-radius:14px;padding:14px;font-size:15px;font-weight:600;cursor:pointer;transition:all 0.25s;touch-action:manipulation;outline:none;text-align:center;display:flex;align-items:center;justify-content:center;gap:8px}
.mode-btn:active{transform:scale(0.97)}
.mode-manual{background:linear-gradient(135deg,#d84315,#bf360c);color:#fff;box-shadow:0 4px 20px rgba(216,67,21,0.25)}
.mode-auto{background:linear-gradient(135deg,#2e7d32,#1b5e20);color:#fff;box-shadow:0 4px 20px rgba(46,125,50,0.25)}
.section-title{font-size:14px;font-weight:600;color:var(--text);margin:18px 0 10px;display:flex;align-items:center;gap:8px}
.section-title .st-line{flex:1;height:1px;background:linear-gradient(90deg,var(--border),transparent)}

/* Device grid */
.device-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:8px}
.device-card{background:var(--card);border-radius:16px;padding:16px;border:1px solid var(--border);cursor:pointer;transition:all 0.3s;touch-action:manipulation;user-select:none;text-align:center;position:relative;overflow:hidden}
.device-card:active{transform:scale(0.96);background:var(--card-hover)}
.device-card .dc-icon{font-size:28px;margin-bottom:8px;display:block}
.device-card .dc-name{font-size:13px;font-weight:600;color:var(--text)}
.device-card .dc-desc{font-size:10px;color:var(--muted);margin-top:4px}
.device-card .dc-toggle{position:absolute;top:10px;right:10px;width:36px;height:20px;border-radius:10px;border:none;cursor:pointer;transition:all 0.3s;padding:0}
.device-card .dc-toggle.off{background:var(--border)}
.device-card .dc-toggle.on{background:var(--accent)}
.device-card .dc-toggle::after{content:'';position:absolute;top:2px;left:2px;width:16px;height:16px;border-radius:50%;background:#fff;transition:transform 0.3s}
.device-card .dc-toggle.on::after{transform:translateX(16px)}
.device-card .dc-hot{position:absolute;top:10px;right:10px;font-size:9px;padding:2px 8px;border-radius:10px;background:rgba(255,107,107,0.2);color:#ff6b6b;font-weight:600}
.device-card .dc-new{position:absolute;top:10px;right:10px;font-size:9px;padding:2px 8px;border-radius:10px;background:rgba(79,195,247,0.2);color:var(--accent);font-weight:600}
.device-card.coming-soon{opacity:0.65}
.device-card.coming-soon .dc-icon{filter:grayscale(0.3)}

/* Chat */
.chat-header{display:flex;align-items:center;gap:12px;padding:4px 0 12px}
.back-btn{background:none;border:none;color:var(--accent);font-size:26px;cursor:pointer;padding:4px;touch-action:manipulation;transition:opacity 0.2s}
.back-btn:active{opacity:0.5}
.chat-header h2{font-size:18px;font-weight:600;flex:1}
.chat-status{font-size:11px;color:var(--muted);background:var(--card);border-radius:12px;padding:4px 12px;border:1px solid var(--border)}
.chat-box{height:calc(100vh - 200px);overflow-y:auto;padding:4px 0 12px;display:flex;flex-direction:column;gap:10px;scroll-behavior:smooth}
.chat-box:empty::after{content:'Send a message to start chatting';display:block;text-align:center;color:#444;padding:40px 0;font-size:14px}
.msg{max-width:88%;padding:12px 16px;border-radius:18px;font-size:15px;line-height:1.5;word-wrap:break-word;animation:msgIn 0.3s ease}
@keyframes msgIn{from{opacity:0;transform:translateY(8px) scale(0.97)}to{opacity:1;transform:translateY(0) scale(1)}}
.msg.user{background:linear-gradient(135deg,#1565c0,#0d47a1);color:#fff;align-self:flex-end;border-bottom-right-radius:4px}
.msg.assistant{background:var(--card);color:var(--text);align-self:flex-start;border-bottom-left-radius:4px;border:1px solid var(--border)}
.msg .role-label{font-size:10px;opacity:0.5;margin-bottom:4px;font-weight:500;text-transform:uppercase;letter-spacing:0.5px}
.msg.thinking{background:var(--card);color:var(--muted);align-self:flex-start;border:1px dashed var(--border);font-style:italic}
.chat-input-wrap{display:flex;gap:8px;padding:12px 0;background:transparent;position:sticky;bottom:0}
.chat-input{flex:1;background:var(--card);border:1px solid var(--border);border-radius:24px;padding:12px 18px;font-size:15px;color:var(--text);outline:none;transition:border-color 0.3s}
.chat-input:focus{border-color:var(--accent)}
.chat-input::placeholder{color:#444}
.send-btn{background:linear-gradient(135deg,var(--accent),#0288d1);border:none;border-radius:24px;padding:12px 22px;font-size:14px;font-weight:600;color:#fff;cursor:pointer;transition:all 0.2s;touch-action:manipulation;white-space:nowrap}
.send-btn:active{transform:scale(0.95)}
.send-btn:disabled{background:var(--border);color:#555;cursor:default;transform:none}

/* Camera */
.camera-container{text-align:center;padding:8px 0}
.camera-container img{width:100%;max-width:100%;border-radius:12px;background:var(--card);display:none;border:1px solid var(--border)}
.camera-placeholder{background:var(--card);border-radius:16px;padding:60px 20px;border:1px solid var(--border);color:var(--muted);font-size:14px}
.camera-placeholder .cp-icon{font-size:48px;display:block;margin-bottom:12px}

/* Coming soon toast */
.toast{position:fixed;bottom:90px;left:50%;transform:translateX(-50%) translateY(20px);background:var(--card);border:1px solid var(--border);border-radius:14px;padding:12px 24px;font-size:14px;color:var(--text);opacity:0;transition:all 0.4s ease;pointer-events:none;z-index:200;backdrop-filter:blur(10px);white-space:nowrap}
.toast.show{opacity:1;transform:translateX(-50%) translateY(0)}

/* Weather card */
.weather-card{background:linear-gradient(135deg,#1565c0,#0d47a1);border-radius:16px;padding:16px 18px;border:1px solid rgba(255,255,255,0.1);margin-bottom:10px;text-align:center;box-shadow:0 4px 20px rgba(21,101,192,0.25)}
.weather-card .wc-text{font-size:15px;font-weight:500;color:#fff;letter-spacing:0.3px}
/* Footer */
.footer{text-align:center;font-size:10px;color:#444;padding:8px 0 4px}
</style>
</head>
<body>

<div class="page-wrap">
  <!-- ===== HOME ===== -->
  <div class="page active" id="pageHome">
    <div class="header">
      <h1><span>&#9889;</span> Smart Home Control</h1>
      <div class="status-badge"><span class="status-dot" id="connDot"></span><span id="connText">Connected</span></div>
    </div>
    <div class="room-switch"><span class="rs-label" id="roomLabel">&#127968; Room 1</span><button class="rs-btn room1" id="roomBtn">Switch</button></div>
    <div class="sensors">
      <div class="sensor-card"><div class="sensor-icon" style="background:#d84315">T</div><div class="sensor-value"><span id="tempValue">--</span><span class="sensor-unit">C</span></div><div class="sensor-label">&#127777; Temperature</div></div>
      <div class="sensor-card"><div class="sensor-icon" style="background:#1565c0">H</div><div class="sensor-value"><span id="humidValue">--</span><span class="sensor-unit">%</span></div><div class="sensor-label">&#128167; Humidity</div></div>
      <div class="sensor-card"><div class="sensor-icon" style="background:#f9a825">L</div><div class="sensor-value"><span id="lightValue">--</span><span class="sensor-unit">lux</span></div><div class="sensor-label">&#9728; Light</div></div>
    </div>
    <div class="quick-grid">
      <div class="quick-card" id="fanBtn"><span class="qc-icon">&#9881;</span><span class="qc-label">Fan</span><span class="qc-state" id="fanState">OFF</span><span class="qc-badge badge-off" id="fanBadge">OFF</span></div>
      <div class="quick-card" id="lightBtn"><span class="qc-icon">&#128161;</span><span class="qc-label">Light</span><span class="qc-state" id="lightState">OFF</span><span class="qc-badge badge-off" id="lightBadge">OFF</span></div>
    </div>
    <button class="mode-btn mode-manual" id="modeBtn"><span>&#9881;</span> MANUAL MODE</button>
    <div class="section-title"><span>Quick Actions</span><span class="st-line"></span></div>
    <div class="quick-grid">
      <div class="quick-card" id="homeAiBtn"><span class="qc-icon">&#129302;</span><span class="qc-label">AI Chat</span><span class="qc-state">Ask XiaoZhi anything</span></div>
      <div class="quick-card" id="cameraBtn" style="display:none"><span class="qc-icon">&#128247;</span><span class="qc-label">Camera</span><span class="qc-state">Live view</span></div>
      <div class="quick-card coming-soon" onclick="showToast('Music player coming soon')"><span class="qc-icon">&#127925;</span><span class="qc-label">Music</span><span class="qc-state">Coming soon</span><span class="dc-hot">NEW</span></div>
      <div class="quick-card coming-soon" onclick="showToast('Scene control coming soon')"><span class="qc-icon">&#127912;</span><span class="qc-label">Scenes</span><span class="qc-state">Coming soon</span><span class="dc-hot">NEW</span></div>
    </div>
    <div class="weather-card" id="weatherCard">
      <div class="wc-text" id="weatherText">Loading weather...</div>
    </div>
    <div class="footer">XiaoZhi Smart Home v1.9.2 &#183; <span id="lastUpdate">--</span></div>
  </div>

  <!-- ===== DEVICES ===== -->
  <div class="page" id="pageDevices">
    <div class="header"><h1>Devices</h1><div style="font-size:12px;color:var(--muted)"><span id="devCount">8</span> devices</div></div>
    <div class="device-grid">
      <div class="device-card" id="devFan"><span class="dc-toggle off" id="devFanToggle"></span><span class="dc-icon">&#9881;</span><span class="dc-name">Ceiling Fan</span><span class="dc-desc" id="devFanDesc">Living Room</span></div>
      <div class="device-card" id="devLight"><span class="dc-toggle off" id="devLightToggle"></span><span class="dc-icon">&#128161;</span><span class="dc-name">Smart Light</span><span class="dc-desc" id="devLightDesc">Living Room</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-hot">NEW</span><span class="dc-icon">&#127968;</span><span class="dc-name">Door Lock</span><span class="dc-desc">Front Door</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-hot">NEW</span><span class="dc-icon">&#127956;</span><span class="dc-name">Curtains</span><span class="dc-desc">Living Room</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-hot">NEW</span><span class="dc-icon">&#127777;</span><span class="dc-name">AC Control</span><span class="dc-desc">Bedroom</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-hot">NEW</span><span class="dc-icon">&#128266;</span><span class="dc-name">Speaker</span><span class="dc-desc">Multi-room</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-hot">NEW</span><span class="dc-icon">&#128200;</span><span class="dc-name">Energy</span><span class="dc-desc">Power Monitor</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-hot">NEW</span><span class="dc-icon">&#128737;</span><span class="dc-name">Security</span><span class="dc-desc">Alarm System</span></div>
    </div>
    <div class="footer">Tap any card to control &bull; Coming soon devices are in development</div>
  </div>

  <!-- ===== AI CHAT ===== -->
  <div class="page" id="pageChat">
    <div class="chat-header">
      <button class="back-btn" id="chatBackBtn">&#8592;</button>
      <h2>&#129302; AI XiaoZhi</h2>
      <span class="chat-status" id="chatStatus">Connected</span>
    </div>
    <div class="chat-box" id="chatBox"></div>
    <div class="chat-input-wrap">
      <input class="chat-input" id="chatInput" type="text" placeholder="Type a message..." autocomplete="off">
      <button class="send-btn" id="sendBtn">Send</button>
    </div>
  </div>

  <!-- ===== CAMERA ===== -->
  <div class="page" id="pageCamera">
    <div class="chat-header">
      <button class="back-btn" id="cameraBackBtn">&#8592;</button>
      <h2>&#128247; Camera</h2>
    </div>
    <div class="camera-container">
      <div class="camera-placeholder" id="cameraStatus"><span class="cp-icon">&#128247;</span>Starting camera...</div>
      <img id="cameraFeed">
    </div>
  </div>

  <!-- ===== SETTINGS ===== -->
  <div class="page" id="pageSettings">
    <div class="header"><h1>Settings</h1></div>
    <div class="device-grid">
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-icon">&#127919;</span><span class="dc-name">Device Info</span><span class="dc-desc">Firmware, network</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-icon">&#128259;</span><span class="dc-name">Wi-Fi</span><span class="dc-desc">Network config</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-icon">&#9881;</span><span class="dc-name">General</span><span class="dc-desc">Preferences</span></div>
      <div class="device-card coming-soon" onclick="showToast('Coming soon')"><span class="dc-icon">&#128214;</span><span class="dc-name">About</span><span class="dc-desc">XiaoZhi v1.9.2</span></div>
    </div>
    <div class="footer">More settings coming in future updates</div>
  </div>
</div>

<!-- Tab Bar -->
<div class="tab-bar">
  <button class="tab-item active" data-page="pageHome"><span class="tab-icon">&#127968;</span><span class="tab-label">Home</span></button>
  <button class="tab-item" data-page="pageDevices"><span class="tab-icon">&#128295;</span><span class="tab-label">Devices</span></button>
  <button class="tab-item" data-page="pageChat"><span class="tab-icon">&#129302;</span><span class="tab-label">AI</span></button>
  <button class="tab-item" data-page="pageCamera" id="tabCamera" style="display:none"><span class="tab-icon">&#128247;</span><span class="tab-label">Camera</span></button>
  <button class="tab-item" data-page="pageSettings"><span class="tab-icon">&#9881;</span><span class="tab-label">Settings</span></button>
</div>

<div class="toast" id="toast"></div>

<script>
let autoMode=false,deviceState='idle',lastMsgId=0,polling=true,isSending=false,hasCamera=false,currentRoom=1;

function fmt1(v){return v===null||v===undefined||v<0?'--':(typeof v=='number'?v.toFixed(1):v)}

function switchPage(id){
  document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  document.querySelectorAll('.tab-item').forEach(t=>t.classList.toggle('active',t.dataset.page===id));
  if(id==='pageChat')pollMessages();
  if(id==='pageCamera')startCamera();else stopCamera();
}

function scrollChat(){const b=document.getElementById('chatBox');b.scrollTop=b.scrollHeight}

function addMessage(role,text){
  const b=document.getElementById('chatBox');const d=document.createElement('div');
  d.className='msg '+role;
  d.innerHTML='<div class="role-label">'+(role==='user'?'You':'AI XiaoZhi')+'</div>'+escapeHtml(text);
  b.appendChild(d);scrollChat()
}

function escapeHtml(t){const d=document.createElement('div');d.textContent=t;return d.innerHTML}

function showToast(msg){
  const t=document.getElementById('toast');t.textContent=msg;t.classList.add('show');
  clearTimeout(t._hide);t._hide=setTimeout(()=>t.classList.remove('show'),2200)
}

function updateWeather(){
  fetch('/api/weather').then(r=>r.text()).then(d=>{
    document.getElementById('weatherText').textContent='Taiyuan Weather: '+(d||'--')
  }).catch(()=>{})
}
function updateSensors(){
  const url=currentRoom===1?'/api/sensors':'/api/room2';
  fetch(url).then(r=>r.json()).then(d=>{
    const t=fmt1(d.temperature),h=fmt1(d.humidity),l=d.light>=0?Math.round(d.light):0;
    document.getElementById('tempValue').textContent=t;
    document.getElementById('humidValue').textContent=h;
    document.getElementById('lightValue').textContent=l
  }).catch(()=>{})
}

function updateStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    deviceState=d.device_state||'idle';autoMode=d.auto_mode;
    const fanState=d.fan_on?'ON':'OFF',lightState=d.light_on?'ON':'OFF';
    ['fan','light'].forEach(k=>{
      const el=document.getElementById(k+'Btn');if(!el)return;
      const sd=document.getElementById(k+'State');if(sd)sd.textContent=k==='fan'?fanState:lightState;
      const bg=document.getElementById(k+'Badge');if(bg){bg.textContent=k==='fan'?fanState:lightState;bg.className='qc-badge '+(k==='fan'?d.fan_on?'badge-on':'badge-off':d.light_on?'badge-on':'badge-off')}
      el.className='quick-card'+(k==='fan'?d.fan_on?' qc-fan-on':'':d.light_on?' qc-light-on':'');
    });
    ['devFan','devLight'].forEach((id,i)=>{
      const el=document.getElementById(id);if(!el)return;
      const t=el.querySelector('.dc-toggle');if(!t)return;
      t.className='dc-toggle '+(i===0?d.fan_on?'on':'off':d.light_on?'on':'off');
      const desc=el.querySelector('.dc-desc');
      if(desc)desc.textContent=(i===0?fanState:lightState)+' \u2022 Living Room';
    });
    const mb=document.getElementById('modeBtn');
    mb.className='mode-btn '+(d.auto_mode?'mode-auto':'mode-manual');
    mb.innerHTML='<span>&#9881;</span> '+(d.auto_mode?'AUTO MODE':'MANUAL MODE');
    const cd=document.getElementById('connDot'),cs=document.getElementById('chatStatus'),ct=document.getElementById('connText');
    switch(deviceState){
      case'listening':cd.style.background='#e65100';ct.textContent='Listening';cs.textContent='Listening';break;
      case'speaking':cd.style.background='#2e7d32';ct.textContent='Speaking';cs.textContent='Speaking';break;
      case'connecting':cd.style.background='#f9a825';ct.textContent='Connecting';cs.textContent='Connecting';break;
      default:cd.style.background='#2ecc71';ct.textContent='Connected';cs.textContent='Connected'
    }
    if(currentRoom===2&&d.last_update){
      const diff=Math.round((Date.now()/1000-d.last_update));
      document.getElementById('lastUpdate').textContent='Room 2 '+(diff<60?diff+'s ago':'>1m ago');
    }else{
      document.getElementById('lastUpdate').textContent=new Date().toLocaleTimeString()
    }
  }).catch(()=>{})
}

function postCtrl(url,body,cb){
  fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
  .then(r=>r.json()).then(d=>{if(d.success&&cb)cb();updateStatus()}).catch(()=>{})
}

function pollMessages(){
  if(!polling)return;
  fetch('/api/ai/messages?after='+lastMsgId).then(r=>r.json()).then(d=>{
    if(d.messages&&d.messages.length>0){d.messages.forEach(m=>{if(m.id>lastMsgId){lastMsgId=m.id;addMessage(m.role,m.text)}});scrollChat()}
  }).catch(()=>{});setTimeout(pollMessages,1000)
}

function sendMessage(){
  const inp=document.getElementById('chatInput');const txt=inp.value.trim();
  if(!txt||isSending)return;isSending=true;
  document.getElementById('sendBtn').disabled=true;
  fetch('/api/ai/send',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({text:txt})})
  .then(r=>r.json()).then(d=>{inp.value='';isSending=false;document.getElementById('sendBtn').disabled=false;updateStatus()})
  .catch(()=>{isSending=false;document.getElementById('sendBtn').disabled=false})
}

// Camera
let cameraTimer=null;
function startCamera(){
  const status=document.getElementById('cameraStatus'),feed=document.getElementById('cameraFeed');
  if(status)status.style.display='none';
  if(feed)feed.style.display='block';
  scheduleSnap()
}
function stopCamera(){
  if(cameraTimer){clearTimeout(cameraTimer);cameraTimer=null}
  const feed=document.getElementById('cameraFeed');
  if(feed)feed.src=''
}
function scheduleSnap(){
  if(!document.getElementById('pageCamera').classList.contains('active'))return;
  const img=document.getElementById('cameraFeed');
  img.onload=function(){cameraTimer=setTimeout(scheduleSnap,250)};
  img.onerror=function(){cameraTimer=setTimeout(scheduleSnap,800)};
  img.src='/api/camera/snapshot?t='+Date.now()
}

// Check if camera is available
fetch('/api/status').then(r=>r.json()).then(()=>{}).catch(()=>{})
fetch('/api/camera/snapshot?t=0').then(r=>{
  if(r.ok||r.status!==503){hasCamera=true;
    document.getElementById('cameraBtn').style.display='block';
    document.getElementById('tabCamera').style.display='flex'
  }
}).catch(()=>{})

// Tab navigation
document.querySelectorAll('.tab-item').forEach(tab=>{
  tab.addEventListener('click',function(){switchPage(this.dataset.page)})
});

// Home controls
document.getElementById('fanBtn').addEventListener('click',function(){if(autoMode){showToast('Switch to Manual mode first');return}postCtrl('/api/fan',{on:!this.classList.contains('qc-fan-on')})});
document.getElementById('lightBtn').addEventListener('click',function(){if(autoMode){showToast('Switch to Manual mode first');return}postCtrl('/api/light',{on:!this.classList.contains('qc-light-on')})});
document.getElementById('modeBtn').addEventListener('click',function(){postCtrl('/api/mode',{auto:!autoMode})});
document.getElementById('homeAiBtn').addEventListener('click',function(){switchPage('pageChat')});
document.getElementById('roomBtn').addEventListener('click',function(){currentRoom=currentRoom===1?2:1;const lb=document.getElementById('roomLabel');lb.textContent=currentRoom===1?'\u{1F3E0} Room 1':'\u{1F3E0} Room 2';this.className='rs-btn room'+currentRoom;document.body.className=currentRoom===2?'room2':'';updateSensors()});

// Device page controls
document.getElementById('devFan').addEventListener('click',function(){if(autoMode){showToast('Switch to Manual mode first');return}const t=this.querySelector('.dc-toggle');postCtrl('/api/fan',{on:!t.classList.contains('on')})});
document.getElementById('devLight').addEventListener('click',function(){if(autoMode){showToast('Switch to Manual mode first');return}const t=this.querySelector('.dc-toggle');postCtrl('/api/light',{on:!t.classList.contains('on')})});

// Chat
document.getElementById('chatBackBtn').addEventListener('click',function(){polling=false;switchPage('pageHome');setTimeout(()=>{polling=true},100)});
document.getElementById('sendBtn').addEventListener('click',sendMessage);
document.getElementById('chatInput').addEventListener('keydown',function(e){if(e.key==='Enter')sendMessage()});

// Camera button (home page)
document.getElementById('cameraBtn').addEventListener('click',function(){switchPage('pageCamera')});
document.getElementById('cameraBackBtn').addEventListener('click',function(){stopCamera();switchPage('pageHome')});

// Count devices
document.getElementById('devCount').textContent=document.querySelectorAll('.device-card').length;

updateSensors();updateStatus();updateWeather();
setInterval(updateSensors,3000);setInterval(updateStatus,3000);setInterval(updateWeather,60000);
</script>
</body>
</html>
)rawliteral";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    httpd_resp_sendstr(req, HTML_PAGE);
    return ESP_OK;
}

static esp_err_t weather_handler(httpd_req_t *req)
{
    const char *data = get_weather_data();
    httpd_resp_set_type(req, "text/plain; charset=UTF-8");
    httpd_resp_sendstr(req, data);
    return ESP_OK;
}

static esp_err_t time_handler(httpd_req_t *req)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"ts\": %lld}", (long long)time(NULL));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t sensors_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", dht22_valid ? dht22_temperature : -1);
    cJSON_AddNumberToObject(root, "humidity", dht22_valid ? dht22_humidity : -1);
    cJSON_AddNumberToObject(root, "light", bh1750_valid ? bh1750_lux : -1);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "fan_on", smart_home_get_fan_state());
    cJSON_AddBoolToObject(root, "light_on", smart_home_get_light_state());
    cJSON_AddBoolToObject(root, "auto_mode", smart_home_get_auto_mode());

    auto &app = Application::GetInstance();
    DeviceState state = app.GetDeviceState();
    const char *state_str = "unknown";
    switch (state) {
        case kDeviceStateIdle:           state_str = "idle"; break;
        case kDeviceStateListening:      state_str = "listening"; break;
        case kDeviceStateSpeaking:       state_str = "speaking"; break;
        case kDeviceStateConnecting:     state_str = "connecting"; break;
        default:                         state_str = "idle"; break;
    }
    cJSON_AddStringToObject(root, "device_state", state_str);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t fan_handler(httpd_req_t *req)
{
    if (req->method == HTTP_OPTIONS) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *on = cJSON_GetObjectItem(root, "on");
    if (!cJSON_IsBool(on)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'on' field");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        if (!smart_home_set_fan_state(cJSON_IsTrue(on))) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": false, \"reason\": \"auto_mode_active\"}");
            return ESP_OK;
        }
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    return ESP_OK;
}

static esp_err_t light_handler(httpd_req_t *req)
{
    if (req->method == HTTP_OPTIONS) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *on = cJSON_GetObjectItem(root, "on");
    if (!cJSON_IsBool(on)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'on' field");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        if (!smart_home_set_light_state(cJSON_IsTrue(on))) {
            cJSON_Delete(root);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"success\": false, \"reason\": \"auto_mode_active\"}");
            return ESP_OK;
        }
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    return ESP_OK;
}

static esp_err_t mode_handler(httpd_req_t *req)
{
    if (req->method == HTTP_OPTIONS) {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *auto_mode = cJSON_GetObjectItem(root, "auto");
    if (!cJSON_IsBool(auto_mode)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'auto' field");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        smart_home_set_auto_mode(cJSON_IsTrue(auto_mode));
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    return ESP_OK;
}

static esp_err_t matter_descriptor_handler(httpd_req_t *req)
{
    std::string json = matter::DeviceManager::GetInstance().GenerateDescriptorJson();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json.c_str());
    return ESP_OK;
}

static esp_err_t ai_send_handler(httpd_req_t *req)
{
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (!cJSON_IsString(text) || strlen(text->valuestring) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'text' field");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    std::string msg(text->valuestring);

    on_chat_message("user", msg);

    auto &app = Application::GetInstance();
    app.SendText(msg);

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    return ESP_OK;
}

static esp_err_t ai_messages_handler(httpd_req_t *req)
{
    uint32_t after = 0;
    char query[128];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[16];
        if (httpd_query_key_value(query, "after", val, sizeof(val)) == ESP_OK) {
            after = atoi(val);
        }
    }

    std::lock_guard<std::mutex> lock(s_msg_mutex);
    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();

    for (auto &m : s_messages) {
        if (m.id > after) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", m.id);
            cJSON_AddStringToObject(item, "role", m.role.c_str());
            cJSON_AddStringToObject(item, "text", m.text.c_str());
            cJSON_AddItemToArray(arr, item);
        }
    }

    cJSON_AddItemToObject(root, "messages", arr);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

#ifdef CONFIG_IDF_TARGET_ESP32P4
// ============================================================
// Web camera snapshot — optimized for low-latency streaming
// ============================================================
#define WEBCAM_SCALE       2       // 800/2 = 400px (4x fewer pixels)
#define WEBCAM_JPEG_QUAL   60      // good quality/size balance

static uint8_t *s_webcam_rgb888 = NULL;
static uint8_t *s_webcam_jpeg   = NULL;
static int s_webcam_w = 0, s_webcam_h = 0;
static size_t s_webcam_rgb888_size = 0;
static size_t s_webcam_jpeg_size   = 0;

static esp_err_t camera_snapshot_handler(httpd_req_t *req)
{
    int w = 0, h = 0;
    if (camera_get_frame(NULL, 0, &w, &h) != ESP_OK || w <= 0 || h <= 0) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "Camera not ready");
        return ESP_OK;
    }

    size_t frame_size = (size_t)w * h * 2;
    uint8_t *frame_buf = (uint8_t*)heap_caps_malloc(frame_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!frame_buf) {
        httpd_resp_set_status(req, "500 Internal Error");
        httpd_resp_sendstr(req, "Memory error");
        return ESP_OK;
    }

    if (camera_get_frame(frame_buf, frame_size, &w, &h) != ESP_OK) {
        heap_caps_free(frame_buf);
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_sendstr(req, "Camera not ready");
        return ESP_OK;
    }

    // ---- Downscale: 800x800 → 266x266 by subsampling ----
    int small_w = w / WEBCAM_SCALE;
    int small_h = h / WEBCAM_SCALE;
    int small_pixels = small_w * small_h;

    // One-time buffer allocation (reused across requests)
    if (!s_webcam_rgb888 || s_webcam_w != small_w || s_webcam_h != small_h) {
        size_t new_rgb = (size_t)small_pixels * 3;
        size_t new_jpg = (size_t)small_pixels * 2;
        if (s_webcam_rgb888) heap_caps_free(s_webcam_rgb888);
        if (s_webcam_jpeg)   heap_caps_free(s_webcam_jpeg);
        s_webcam_rgb888 = (uint8_t*)heap_caps_malloc(new_rgb, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_webcam_jpeg   = (uint8_t*)heap_caps_malloc(new_jpg, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_webcam_rgb888_size = new_rgb;
        s_webcam_jpeg_size   = new_jpg;
        s_webcam_w = small_w;
        s_webcam_h = small_h;
    }

    if (!s_webcam_rgb888 || !s_webcam_jpeg) {
        heap_caps_free(frame_buf);
        httpd_resp_set_status(req, "500 Internal Error");
        httpd_resp_sendstr(req, "Memory error");
        return ESP_OK;
    }

    // Subsampled RGB565 → RGB888 conversion (9x fewer pixels)
    const uint16_t *src = (const uint16_t*)frame_buf;
    uint8_t *dst = s_webcam_rgb888;
    int src_stride = w;  // pixels per row in source
    for (int y = 0; y < h; y += WEBCAM_SCALE) {
        int row_start = y * src_stride;
        for (int x = 0; x < w; x += WEBCAM_SCALE) {
            uint16_t p = src[row_start + x];
            dst[0] = ((p >> 11) & 0x1F) << 3;
            dst[1] = ((p >> 5)  & 0x3F) << 2;
            dst[2] = (p & 0x1F) << 3;
            dst += 3;
        }
    }
    heap_caps_free(frame_buf);

    // JPEG encode the small image (fast — only ~70K pixels)
    jpeg_enc_handle_t jpeg_enc = NULL;
    jpeg_enc_config_t enc_cfg = {
        .width       = small_w,
        .height      = small_h,
        .src_type    = JPEG_PIXEL_FORMAT_RGB888,
        .subsampling = JPEG_SUBSAMPLE_420,
        .quality     = WEBCAM_JPEG_QUAL,
        .rotate      = JPEG_ROTATE_0D,
        .task_enable = false,
    };

    jpeg_error_t jerr = jpeg_enc_open(&enc_cfg, &jpeg_enc);
    if (jerr != JPEG_ERR_OK || !jpeg_enc) {
        httpd_resp_set_status(req, "500 Internal Error");
        httpd_resp_sendstr(req, "JPEG init failed");
        return ESP_OK;
    }

    int out_size = 0;
    jerr = jpeg_enc_process(jpeg_enc, s_webcam_rgb888, small_pixels * 3,
                            s_webcam_jpeg, s_webcam_jpeg_size, &out_size);
    jpeg_enc_close(jpeg_enc);

    if (jerr != JPEG_ERR_OK || out_size <= 0) {
        httpd_resp_set_status(req, "500 Internal Error");
        httpd_resp_sendstr(req, "JPEG encode failed");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, (const char*)s_webcam_jpeg, out_size);
    return ESP_OK;
}
#endif

static esp_err_t room2_sensors_post_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *temp = cJSON_GetObjectItem(root, "temperature");
    cJSON *humid = cJSON_GetObjectItem(root, "humidity");
    cJSON *light = cJSON_GetObjectItem(root, "light");

    {
        std::lock_guard<std::mutex> lock(s_room2_mutex);
        if (cJSON_IsNumber(temp)) s_room2_temp = temp->valuedouble;
        if (cJSON_IsNumber(humid)) s_room2_humid = humid->valuedouble;
        if (cJSON_IsNumber(light)) s_room2_light = light->valuedouble;
        s_room2_last_update = esp_timer_get_time() / 1000000;
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"success\": true}");
    ESP_LOGI(TAG, "Room2 sensor data received: temp=%.1f, humid=%.1f, light=%.0f",
             temp ? temp->valuedouble : -1,
             humid ? humid->valuedouble : -1,
             light ? light->valuedouble : -1);
    return ESP_OK;
}

static esp_err_t room2_sensors_get_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    {
        std::lock_guard<std::mutex> lock(s_room2_mutex);
        cJSON_AddNumberToObject(root, "temperature", s_room2_temp);
        cJSON_AddNumberToObject(root, "humidity", s_room2_humid);
        cJSON_AddNumberToObject(root, "light", s_room2_light);
        cJSON_AddNumberToObject(root, "last_update", s_room2_last_update);
    }
    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    cJSON_free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

void smart_home_web_server_configure_ap(void)
{
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap_netif) {
        ESP_LOGE(TAG, "Failed to get AP netif handle");
        return;
    }

    esp_netif_ip_info_t ip_info;
    ip_info.gw.addr = 0x0104A8C0;
    ip_info.ip.addr = 0x0104A8C0;
    ip_info.netmask.addr = 0x00FFFFFF;
    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_dhcps_start(ap_netif);

    wifi_config_t ap_config = {};
    strcpy((char *)ap_config.ap.ssid, "Xiaozhi-SmartHome");
    ap_config.ap.ssid_len = 0;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.beacon_interval = 100;

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set AP config, err=%d", ret);
        return;
    }

    ESP_LOGI(TAG, "SoftAP configured: SSID=Xiaozhi-SmartHome, IP=192.168.4.1");
}

static void start_web_server(void)
{
    // 初始化 Matter 设备树（一次性）
    smarthome_matter_init();
    ESP_LOGI(TAG, "Matter device tree initialized");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 16;
    config.stack_size = 16384;
    config.lru_purge_enable = true;

    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_root);

    httpd_uri_t uri_sensors = {.uri = "/api/sensors", .method = HTTP_GET, .handler = sensors_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_sensors);

    httpd_uri_t uri_weather = {.uri = "/api/weather", .method = HTTP_GET, .handler = weather_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_weather);

    httpd_uri_t uri_time = {.uri = "/api/time", .method = HTTP_GET, .handler = time_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_time);

    httpd_uri_t uri_status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_status);

    httpd_uri_t uri_fan = {.uri = "/api/fan", .method = HTTP_POST, .handler = fan_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_fan);

    httpd_uri_t uri_light = {.uri = "/api/light", .method = HTTP_POST, .handler = light_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_light);

    httpd_uri_t uri_mode = {.uri = "/api/mode", .method = HTTP_POST, .handler = mode_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_mode);

    httpd_uri_t uri_matter = {.uri = "/api/matter/descriptor", .method = HTTP_GET, .handler = matter_descriptor_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_matter);

    httpd_uri_t uri_ai_send = {.uri = "/api/ai/send", .method = HTTP_POST, .handler = ai_send_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_ai_send);

    httpd_uri_t uri_ai_msgs = {.uri = "/api/ai/messages", .method = HTTP_GET, .handler = ai_messages_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_ai_msgs);

#ifdef CONFIG_IDF_TARGET_ESP32P4
    httpd_uri_t uri_camera = {.uri = "/api/camera/snapshot", .method = HTTP_GET, .handler = camera_snapshot_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_camera);
#endif

    httpd_uri_t uri_room2_post = {.uri = "/api/room2/sensors", .method = HTTP_POST, .handler = room2_sensors_post_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_room2_post);

    httpd_uri_t uri_room2_get = {.uri = "/api/room2", .method = HTTP_GET, .handler = room2_sensors_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_room2_get);

    httpd_uri_t uri_favicon = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = [](httpd_req_t *req) -> esp_err_t {
        httpd_resp_set_type(req, "image/x-icon");
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }, .user_ctx = NULL};
    httpd_register_uri_handler(s_server, &uri_favicon);

    ESP_LOGI(TAG, "Web server running at http://192.168.4.1");
}

static void web_server_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));

    start_web_server();

    Application::GetInstance().OnChatMessage(on_chat_message);

    vTaskDelete(NULL);
}

void smart_home_get_room2_sensors(float *temp, float *humid, float *light, int64_t *last_update)
{
    std::lock_guard<std::mutex> lock(s_room2_mutex);
    if (temp) *temp = s_room2_temp;
    if (humid) *humid = s_room2_humid;
    if (light) *light = s_room2_light;
    if (last_update) *last_update = s_room2_last_update;
}

void smart_home_web_server_start(void)
{
    if (s_server_running) return;
    s_server_running = true;

    xTaskCreate(web_server_task, "smart_home_web", 10240, NULL, 5, NULL);
    ESP_LOGI(TAG, "Smart home web server task created");
}
