#pragma once

// Each URL sends a shared shell and only that page's content, directly from Flash.
static const char WEB_STYLE[] PROGMEM = R"CSS(

:root{color-scheme:light dark;--bg:#f5f5f7;--card:#fff;--text:#1d1d1f;--muted:#6e6e73;--line:#e5e5ea;--blue:#007aff;--soft:#eaf3ff;--red:#c5342b;--green:#188350;--track:#ececf0}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.55 system-ui,-apple-system,"Microsoft YaHei",sans-serif}button,input{font:inherit}button,a,input{-webkit-tap-highlight-color:transparent}button,a{touch-action:manipulation}a{color:var(--blue);text-decoration:none}a:hover{text-decoration:underline}button{border:0;border-radius:12px;padding:11px 16px;background:var(--blue);color:white;cursor:pointer;font-weight:600;min-height:44px}button:hover{filter:brightness(.94)}button:disabled{opacity:.5;cursor:wait}.secondary{background:var(--soft);color:var(--blue)}.danger{background:var(--red);color:#fff}.quiet{background:var(--track);color:var(--text)}:focus-visible{outline:3px solid #72acff;outline-offset:3px}input,select{width:100%;min-width:0;background:var(--card);color:var(--text);border:1px solid var(--line);padding:12px;border-radius:10px}label{display:block;font-size:14px;font-weight:600;margin:12px 0 6px}.topbar{border-bottom:1px solid var(--line);background:var(--card)}.topbar-inner{max-width:1080px;margin:auto;display:flex;align-items:center;gap:16px;padding:16px 24px}.brand{font-size:19px;font-weight:750;letter-spacing:-.5px;color:var(--text);white-space:nowrap}.connection{font-size:13px;color:var(--muted);margin-left:auto;display:flex;align-items:center;gap:7px}.dot{width:7px;height:7px;border-radius:50%;background:#999}.connection[data-connected="true"] .dot{background:var(--green)}#logoutButton{font-size:13px;min-height:36px;padding:7px 12px}.nav-wrap{max-width:1080px;margin:auto;padding:16px 24px 0}nav{display:flex;gap:5px;background:var(--track);padding:5px;border-radius:14px}nav a{flex:1;min-width:0;padding:9px 6px;border-radius:10px;text-align:center;color:var(--muted);font-size:14px;font-weight:650;white-space:nowrap}nav a[aria-current="page"]{color:var(--blue);background:var(--card);box-shadow:0 1px 5px #00000008}nav a:hover{text-decoration:none;background:var(--card)}main{max-width:1080px;margin:auto;padding:22px 24px 48px}.page-heading{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:2px 0 18px}h1{font-size:26px;letter-spacing:-.7px;line-height:1.25;margin:0}h2{font-size:17px;margin:0 0 14px;letter-spacing:-.2px}p{margin:10px 0}.card{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:22px;margin-bottom:18px}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}.grid>.card{margin:0}.grid+.card,.grid+.grid{margin-top:18px}.muted{color:var(--muted);font-size:13px}.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.row input{flex:1;min-width:160px}.row button{flex-shrink:0}.controls{display:flex;gap:9px;flex-wrap:wrap;margin-top:16px}.controls button{flex:1;white-space:nowrap;min-width:105px}.status{font-weight:650}.temperature{display:flex;align-items:center;justify-content:center;gap:24px;margin:26px 0}.temperature strong{font-size:72px;line-height:1.1;font-weight:650;letter-spacing:-4px}.temperature small{font-size:25px;color:var(--muted);vertical-align:top;letter-spacing:0}.temperature button{border-radius:50%;width:48px;height:48px;padding:0;font-size:26px}.center{text-align:center}.badge{display:inline-flex;font-size:12px;color:var(--muted);background:var(--track);border-radius:20px;padding:5px 10px;white-space:nowrap}.badge[data-stale="true"]{color:var(--red)}.notice{font-size:14px;min-height:24px;padding-top:10px;overflow-wrap:anywhere}.notice[data-error="true"]{color:var(--red)}.notice:empty{display:none}.pairing{display:flex;gap:24px;align-items:center;flex-wrap:wrap}.pairing-code{text-align:center;flex-shrink:0}.pairing-code img{display:block;width:200px;height:200px;background:white;border:1px solid var(--line);border-radius:14px;image-rendering:pixelated}.setup-code{font-size:21px;letter-spacing:2px;font-variant-numeric:tabular-nums}.pairing-info{flex:1;min-width:220px}.facts{margin:0}.facts>div{display:flex;justify-content:space-between;align-items:baseline;gap:18px;padding:10px 0;border-bottom:1px solid var(--line)}.facts>div:last-child{border:0}.facts dt{color:var(--muted);font-size:14px}.facts dd{margin:0;text-align:right;overflow-wrap:anywhere;max-width:70%}.metric-value{font-size:36px;font-weight:650;letter-spacing:-1px;font-variant-numeric:tabular-nums}.metric-value small{font-size:15px;color:var(--muted);letter-spacing:0}progress{display:block;width:100%;height:8px;margin:12px 0 10px;border:0;border-radius:10px;overflow:hidden;background:var(--track);accent-color:var(--blue)}progress::-webkit-progress-bar{background:var(--track);border-radius:10px}progress::-webkit-progress-value{background:var(--blue);border-radius:10px}progress::-moz-progress-bar{background:var(--blue);border-radius:10px}.core-load{display:flex;justify-content:space-between;gap:16px;font-size:13px;color:var(--muted);flex-wrap:wrap}.log{height:52vh;min-height:260px;overflow:auto;background:#111820;color:#d7e2ed;border-radius:12px;padding:14px;font:12px/1.65 Consolas,monospace;white-space:nowrap;margin-top:16px}.rx{color:#88d9ac}.tx{color:#8bbcff}.raw{font:12px/1.7 Consolas,monospace;overflow-wrap:anywhere;background:var(--bg);border-radius:10px;padding:12px;margin-top:14px}details summary{cursor:pointer;color:var(--muted);font-size:13px;padding:8px 0}details p{font-size:13px;color:var(--muted)}.form-width{max-width:560px}.footer-note{font-size:12px;color:var(--muted);margin:18px 2px}.full{grid-column:1/-1}#firmwareProgress[hidden],[hidden]{display:none!important}
.network-card{padding:16px 20px}.network-card h2{margin-bottom:10px}.network-form{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr) auto;gap:14px;align-items:end}.network-form label{margin:0}.network-form label>.muted{font-size:12px;font-weight:400}.network-form input{padding:9px 10px;min-height:44px;margin-top:5px}.hostname-address{display:flex;align-items:center;border:1px solid var(--line);border-radius:10px;overflow:hidden;margin-top:5px}.hostname-address input{margin:0;border:0;border-radius:0}.hostname-address>span{padding:0 10px;color:var(--muted);font-size:13px;white-space:nowrap}.network-actions{display:flex;gap:8px}.network-actions button{padding:10px 13px;white-space:nowrap}.network-help{margin-top:6px}.network-help summary{padding:4px 0;font-size:12px}.network-help p{margin:4px 0}.network-links:empty{display:none}.network-links{display:flex;gap:8px 18px;flex-wrap:wrap;font-size:13px;margin-top:8px;overflow-wrap:anywhere}@media(max-width:650px){.network-form{grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:10px}.network-actions{grid-column:1/-1;justify-content:flex-end}}@media(max-width:400px){.network-form{grid-template-columns:1fr}.network-actions{grid-column:auto}}
@media(prefers-color-scheme:dark){:root{--bg:#000;--card:#1c1c1e;--text:#f5f5f7;--muted:#a1a1a6;--line:#343437;--blue:#0a84ff;--soft:#102a43;--red:#d3463d;--green:#44c786;--track:#2c2c2e}}@media(max-width:650px){.topbar-inner{padding:14px 16px;gap:9px}.brand{font-size:17px}.connection{font-size:12px}#logoutButton{padding:7px 9px}.nav-wrap{padding:12px 12px 0}nav{gap:2px}nav a{font-size:12px;padding:10px 1px}main{padding:20px 16px 32px}.grid{grid-template-columns:1fr;gap:14px}.card{padding:18px;border-radius:18px;margin-bottom:14px}.grid+.card,.grid+.grid{margin-top:14px}h1{font-size:24px}.temperature{gap:18px}.temperature strong{font-size:62px}.pairing{justify-content:center}.pairing-info{min-width:0;flex-basis:100%}.controls button{min-width:95px}.row input[type=file]{flex-basis:100%}.facts>div{gap:10px}.facts dd{max-width:65%}.page-heading{gap:8px}}

.log{font:12px/1.8 Consolas,monospace;white-space:nowrap}.log-entry{display:flex;align-items:baseline;gap:12px;white-space:nowrap;width:max-content;min-width:100%;padding:2px 0;border-bottom:1px solid #ffffff0d}.log-time{color:#8995a2;flex-shrink:0}.log-source{width:64px;flex-shrink:0;color:#e2e8ef;font-weight:700}.log-direction{width:116px;flex-shrink:0}.log-data{color:#d7e2ed;white-space:nowrap}.log-entry.midea-rx .log-direction,.log-entry.miot-rx .log-direction{color:#88d9ac}.log-entry.midea-tx .log-direction,.log-entry.miot-tx .log-direction{color:#8bbcff}@media(max-width:650px){.log-entry{gap:8px}.log-source{width:56px}.log-direction{width:96px}}

.integration-card{padding:16px 20px}.integration-card h2{margin:0}.section-heading{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:10px;margin-bottom:8px}.integration-title,.integration-actions{display:flex;align-items:center;gap:10px;flex-wrap:wrap}.integration-actions a{font-size:13px}.integration-actions button,.integration-tools button{font-size:13px;padding:8px 12px;min-height:44px;border-radius:10px;white-space:normal;min-width:0}.module-facts{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px 20px}.module-facts>div{display:block;border:0;padding:5px 0;min-width:0}.module-facts dt{font-size:12px}.module-facts dd{max-width:none;text-align:left;font-size:14px;line-height:1.45;margin-top:3px}.integration-details{border-top:1px solid var(--line);margin-top:8px}.integration-details summary{padding:8px 0;font-size:13px}.integration-details p{font-size:12px;line-height:1.6;margin:7px 0}.integration-tools{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}.integration-caption{color:var(--muted);font-size:13px;margin:6px 0 0}.integration-switch{display:flex;align-items:center;gap:7px;margin:0;min-height:44px;font-size:13px;white-space:nowrap}.integration-switch input{width:16px;height:16px;margin:0;padding:0;accent-color:var(--blue)}@media(max-width:650px){.integration-card{padding:16px}.module-facts{grid-template-columns:repeat(2,minmax(0,1fr));gap:6px 14px}.integration-title{gap:7px}.integration-title h2{font-size:16px}}.fan-grid{display:grid;grid-template-columns:repeat(7,minmax(0,1fr));gap:8px;margin:12px 0}.fan-item{display:flex;flex-direction:column;align-items:center;background:var(--track);padding:8px 4px;border-radius:12px;text-align:center}.fan-item span{font-size:12px;font-weight:700;color:var(--text);white-space:nowrap}.fan-item small{font-size:11px;color:var(--muted);margin:2px 0 6px}.fan-item input{width:100%;text-align:center;padding:7px 2px;font-size:15px;font-weight:700;border-radius:8px;background:var(--card)}@media(max-width:750px){.fan-grid{grid-template-columns:repeat(4,minmax(0,1fr))}}@media(max-width:450px){.fan-grid{grid-template-columns:repeat(3,minmax(0,1fr))}}
)CSS";

static const char WEB_HEAD[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="color-scheme" content="light dark"><title>Midea Bridge</title><link rel="stylesheet" href="/assets/app.css?v=)HTML" BRIDGE_VERSION R"HTML(" onerror="const n=Number(this.dataset.retry||0)+1;this.dataset.retry=n;if(n<=3)setTimeout(()=>{this.href=this.href.replace(/&retryCss=\d+$/,'')+'&retryCss='+n},1500)"></head><body><header class="topbar"><div class="topbar-inner"><a class="brand" href="/">Midea Bridge</a><div class="connection" id="connection"><i class="dot"></i><span id="connectionText">连接中</span></div><button class="quiet" id="logoutButton" type="button">退出登录</button></div></header><div class="nav-wrap"><nav aria-label="功能页面"><a href="/">空调控制</a><a href="/integrations">智能接入</a><a href="/device">设备状态</a><a href="/debug">串口调试</a><a href="/settings">设置</a></nav></div><main>
)HTML";

static const char WEB_CONTROL[] PROGMEM = R"HTML(
<div class="page-heading"><h1>空调控制</h1><span class="badge" id="stateAge">等待回包</span></div>
<section class="card"><div class="row"><span class="status" id="state">正在读取状态…</span><span class="muted" id="room">室温 —</span></div><div class="temperature"><button class="secondary" data-cmd="temp_down" aria-label="温度降低半度">−</button><div class="center"><strong id="targetTemperature">—</strong><small>°C</small><div class="muted">设定温度</div></div><button class="secondary" data-cmd="temp_up" aria-label="温度升高半度">+</button></div><div class="controls"><button data-cmd="power_on">开机</button><button class="danger" data-cmd="power_off">关机</button><button class="secondary" data-cmd="query">查询状态</button></div><div class="notice" id="notice" role="status" aria-live="polite"></div></section>
<div class="grid"><section class="card"><h2>模式与风速</h2><dl class="facts"><div><dt>当前模式</dt><dd id="acMode">—</dd></div><div><dt>当前风速</dt><dd id="acFan">—</dd></div></dl><div class="controls"><button class="secondary" data-cmd="mode_next">切换模式</button><button class="secondary" data-cmd="fan_next">切换风速</button></div><p class="muted">关机时保留上次开机风速，重新开机继续沿用；主动设置风速时使用新值。重启后需先同步一次开机风速。</p></section><section class="card"><h2>舒适功能</h2><dl class="facts"><div><dt>上下摆风</dt><dd id="acSwing">—</dd></div><div><dt>ECO 节能</dt><dd id="acEco">—</dd></div><div><dt>睡眠</dt><dd id="acSleep">—</dd></div></dl><div class="controls"><button class="secondary" data-cmd="swing_toggle">上下摆风</button><button class="secondary" data-cmd="eco_toggle">ECO</button><button class="secondary" data-cmd="sleep_toggle">睡眠</button></div></section></div>
<section class="card" style="margin-top:18px"><div class="row"><h2>开关机倒计时</h2><span class="badge" id="timerState">等待回包</span><button class="secondary" id="timerQuery" type="button">查询倒计时</button></div><p class="muted">设置从现在起多少时间后开机或关机，由空调执行。下方显示空调实际回报的剩余时间，随状态回包更新。</p><div class="grid"><section><h2>倒计时开机</h2><p class="status" id="timerOnValue">等待空调回报</p><form id="timerOnForm"><div class="row"><div style="flex:1;min-width:100px"><label for="timerOnHours">小时</label><input id="timerOnHours" type="number" inputmode="numeric" min="0" max="24" step="1" value="1" required></div><div style="flex:1;min-width:100px"><label for="timerOnMinutes">分钟</label><input id="timerOnMinutes" type="number" inputmode="numeric" min="0" max="59" step="1" value="0" required></div></div><div class="controls"><button data-timer-action type="submit">设置开机倒计时</button><button data-timer-action class="quiet" id="timerOnCancel" type="button">取消开机倒计时</button></div></form></section><section><h2>倒计时关机</h2><p class="status" id="timerOffValue">等待空调回报</p><form id="timerOffForm"><div class="row"><div style="flex:1;min-width:100px"><label for="timerOffHours">小时</label><input id="timerOffHours" type="number" inputmode="numeric" min="0" max="24" step="1" value="1" required></div><div style="flex:1;min-width:100px"><label for="timerOffMinutes">分钟</label><input id="timerOffMinutes" type="number" inputmode="numeric" min="0" max="59" step="1" value="0" required></div></div><div class="controls"><button data-timer-action type="submit">设置关机倒计时</button><button data-timer-action class="quiet" id="timerOffCancel" type="button">取消关机倒计时</button></div></form></section></div><p class="muted">范围：1 分钟至 24 小时。设置或取消一项时保留另一项；实际支持的时长以空调回包为准。</p><div class="notice" id="timerActionNotice" role="status" aria-live="polite"></div><div class="notice" id="timerResultNotice" role="status" aria-live="polite"></div></section>
<p class="footer-note">页面显示空调实际回报；指令发送后等待回包更新。原始数据请前往“串口调试”。</p>
)HTML";

static const char WEB_INTEGRATIONS[] PROGMEM = R"HTML(
<div class="page-heading"><h1>智能接入</h1></div><section class="card"><h2>HomeKit</h2><div class="pairing"><div class="pairing-code"><img src="/pairing.svg" alt="HomeKit 配对二维码"><p class="muted">配对码</p><b class="setup-code" id="homekitCode">读取中</b></div><div class="pairing-info"><p>配对状态：<b id="hk">读取中</b></p><p class="muted">用 iPhone“家庭”App 扫码添加，也可手动输入配对码。支持关机、制冷、制热和自动，除湿与送风可在空调控制页操作。</p><button class="danger" id="unpairButton">清除 HomeKit 配对</button><p class="muted">清除后，在“家庭”App 移除旧配件再重新添加。</p><div class="notice" id="homekitNotice" role="status"></div></div></div></section>
<section class="card integration-card">
<div class="section-heading"><h2>米家模块</h2><div class="integration-actions"><button data-miot="sync">同步美的状态</button><a href="/debug">串口日志</a></div></div>
<dl class="facts module-facts"><div><dt>串口状态</dt><dd id="miotSeen">—</dd></div><div><dt>模块网络</dt><dd id="miotNet">—</dd></div><div><dt>模块型号</dt><dd id="miotModel">—</dd></div><div><dt>米家模式</dt><dd id="miotMode">—</dd></div><div><dt>协议 / 握手</dt><dd id="miotProfile">—</dd></div><div><dt>最新同步</dt><dd id="miotResult">—</dd></div></dl>
<div class="notice" id="miotNotice" role="status"></div>
<details class="integration-details"><summary>同步统计、串口诊断与查询工具</summary><dl class="facts module-facts"><div><dt>美的 → 米家</dt><dd id="miotReports">—</dd></div><div><dt>米家 → 美的</dt><dd id="miotConfirmed">—</dd></div><div><dt>回复延迟</dt><dd id="miotLatency">—</dd></div><div><dt>程序服务间隔</dt><dd id="miotServiceGap">—</dd></div><div><dt>接收溢出 / 信号错误</dt><dd id="miotUartErrors">—</dd></div><div><dt>最后超时命令</dt><dd id="miotLastTimeout">—</dd></div></dl><div class="integration-tools"><button class="secondary" data-miot="model">查询型号</button><button class="secondary" data-miot="version">查询版本</button><button class="secondary" data-miot="net">查询网络</button><button class="secondary" data-miot="mac">查询 MAC</button><button class="secondary" data-miot="time">查询时间</button><button class="secondary" data-miot="get_down">查询下行</button></div><p>查询结果显示在<a href="/debug">串口日志</a>中。</p><div class="integration-tools"><button class="secondary" id="miotTestEnvironmentButton">测试上报 23°C / 75%</button></div><p>测试值只用于验证上报，不控制空调；真实传感器值覆盖前，米家可能继续显示测试值。</p><div class="notice" id="miotTestEnvironmentNotice" role="status"></div></details>
<details class="integration-details"><summary>接线与协议映射说明</summary><p>UART1：115200 8N1。ESP32 GPIO7 ← 米家模块 GPIO17（MCU_TX）；ESP32 GPIO8 → 米家模块 GPIO16（MCU_RX），共地。不要接米家模块的 TXD0/RXD0 日志口。C20 支持电源、模式、温度、七档风速、上下/左右摆风开关、ECO、睡眠与有效室温；灯光 6.1 关/开对应屏显关/开，亮度 6.2 自动/中/高均对应屏显开且回报统一为自动；保留 MA2、M4/M6 映射。C20 睡眠由 ESP32 RAM 虚拟维护，不读取美的睡眠位：开启设置美的原始风速码 1，关闭设置自定义第 3 档（默认风速码 30）；确认美的风速回包后更新状态，直接调风速或收到其他风速码会清除睡眠状态，重启后默认关闭。0.1°C 请求取最近 0.5°C 后限于 17–30°C。控制以美的校验通过的回包确认；目标湿度 2.14 独立模拟：0 表示关闭，1–100 为设定值；不控制美的。提示音 5.1 和辅热 2.9 与美的真实状态同步；自清洁 9.1 使用美的独立命令和状态查询，回包确认后应答。固定摆风角度、新风等没有对应状态的属性返回不支持。</p><p>美的自动模式在米家按室温与设温估算显示制冷或制热，中间区保留上次方向；这不表示压缩机实时状态。开关、调温保持美的自动模式，主动设置模式则切换为指定模式。米家再次选择当前已启用的模式会关闭空调；如需切换，请选择其他模式。</p></details>
</section>
<section class="card integration-card"><div class="section-heading"><div class="integration-title"><h2>米家扩展属性模拟</h2><span class="badge" id="miotSimulationState">读取中</span></div><label class="integration-switch" for="miotSimulationToggle"><input id="miotSimulationToggle" type="checkbox"> 启用模拟</label></div><p class="integration-caption">缺少真实室温时使用 23°C，湿度 75%；模拟功能不驱动美的硬件。</p><div class="notice" id="miotSimulationNotice" role="status"></div><details class="integration-details"><summary>模拟范围与说明</summary><p>室温优先使用真实读数，未知时模拟为 23°C；相对湿度 75%，滤芯寿命 100%。新风、净化等额外属性可在米家读取和模拟切换。提示音、辅热和自清洁使用真实美的映射。</p><p>目标湿度 2.14、干燥 2.10、机组体检 9.2 使用独立 RAM 模拟，不依赖此开关；正常读写应答，不发送美的控制帧。模拟值重启后恢复默认。</p><p>开关机、模式、设温、风速、摆风、ECO 和睡眠仍与美的实际回包同步。开关重启后保留，模拟功能的设置重启后恢复默认。未定义的属性 ID 仍返回不支持。</p><p id="miotSimulationControls"></p></details></section>
<section class="card"><div class="row"><h2>脱机模拟调试模式</h2><span class="badge" id="mockBadge">读取中</span></div><label><input id="mockToggle" type="checkbox" style="width:auto"> 启用脱机模拟调试（离线调试米家 / HomeKit）</label><p>开启后不再向美的串口发送硬件指令，也不依赖美的硬件回包；网页、HomeKit 与米家操作直接在 ESP32 内存中更新并返回成功应答，用于在未接空调时单独调试米家模块与 HomeKit 配对联动。</p><div class="notice" id="mockNotice" role="status"></div></section>
<section class="card"><h2>米家模组配网</h2><p class="muted">清除米家模组原 Wi-Fi 并重新配网，随后在米家 App 添加设备。此操作只修改接在 GPIO7/8 的米家模组。</p><button class="danger" id="miotProvisionButton">进入米家模组配网</button><div class="notice" id="miotProvisionNotice" role="status"></div></section>
)HTML";

static const char WEB_DEVICE[] PROGMEM = R"HTML(
<div class="page-heading"><h1>设备状态</h1><span class="badge" id="metricsUpdated">等待读取</span></div><div class="notice" id="metricsNotice" role="status"></div>
<div class="grid"><section class="card"><h2>CPU 使用率 <span class="badge">采样估算</span></h2><div class="metric-value" id="cpuLoad">—</div><progress id="cpuBar" max="100" value="0" aria-label="CPU 使用率"></progress><div class="core-load" id="cpuCores">等待首次采样</div><p class="muted" id="cpuDetails">—</p><details><summary>如何计算</summary><p>按每个核心的系统节拍采样，统计非空闲任务比例；总使用率取各核心平均。通常每秒更新一次，属于负载估算，短时尖峰和中断开销可能未完整计入。</p></details></section>
<section class="card"><h2>RAM 使用率 <span class="badge">内部动态堆</span></h2><div class="metric-value" id="ramUsage">—</div><progress id="ramBar" max="100" value="0" aria-label="RAM 使用率"></progress><p class="muted" id="ramSummary">—</p><dl class="facts"><div><dt>当前可用</dt><dd id="ramFree">—</dd></div><div><dt>历史最低可用</dt><dd id="ramMin">—</dd></div><div><dt>最大连续空闲块</dt><dd id="ramBlock">—</dd></div></dl><p class="muted">按可分配的内部 8 位堆内存统计，不等于芯片全部物理 SRAM。</p></section>
<section class="card"><h2>Flash / 固件空间 <span class="badge">ROM</span></h2><div class="metric-value" id="flashUsage">—</div><progress id="flashBar" max="100" value="0" aria-label="应用分区使用率"></progress><p class="muted" id="flashSummary">—</p><dl class="facts"><div><dt>Flash 芯片总容量</dt><dd id="flashTotal">—</dd></div><div><dt>当前分区 / 剩余</dt><dd id="flashPartition">—</dd></div><div><dt>下次 OTA 分区容量</dt><dd id="otaCapacity">—</dd></div></dl><p class="muted">百分比为固件占当前应用分区的比例，芯片固定 ROM 不参与统计。</p></section>
<section class="card"><h2>处理器与 PSRAM</h2><dl class="facts"><div><dt>芯片</dt><dd id="chip">—</dd></div><div><dt>固件版本</dt><dd id="firmware">—</dd></div><div><dt>构建时间</dt><dd id="build">—</dd></div><div><dt>运行时间</dt><dd id="uptime">—</dd></div><div><dt>PSRAM</dt><dd id="psramState">—</dd></div></dl><div id="psramDetails" hidden><progress id="psramBar" max="100" value="0" aria-label="PSRAM 使用率"></progress><p class="muted" id="psramSummary"></p></div></section></div>
<section class="card" style="margin-top:18px"><h2>网络与蓝牙</h2><dl class="facts"><div><dt>Wi-Fi</dt><dd id="wifiState">—</dd></div><div><dt>当前 IP</dt><dd id="deviceIp">—</dd></div><div><dt>主机名（DHCP）</dt><dd id="networkHostnameValue">—</dd></div><div><dt>MAC 地址</dt><dd id="deviceMac">—</dd></div><div><dt>本地域名</dt><dd><a id="localHostnameLink" href="#">读取中</a></dd></div><div><dt>信号强度</dt><dd id="rssi">—</dd></div><div><dt>蓝牙设备</dt><dd id="bleName">—</dd></div><div><dt>蓝牙连接</dt><dd id="bleState">—</dd></div></dl><p class="muted">需要更改 ESP32 的 Wi-Fi 时，可通过 Android App 的网络页面配置。</p></section>
<section class="card" style="margin-top:18px"><h2>网络健康</h2><dl class="facts"><div><dt>接入点 / 信道</dt><dd id="wifiAp">—</dd></div><div><dt>网关链路检测</dt><dd id="wifiProbe">等待检测</dd></div><div><dt>自动恢复</dt><dd id="wifiRecoveries">—</dd></div><div><dt>断线记录</dt><dd id="wifiDisconnects">—</dd></div><div><dt>Wi-Fi 省电</dt><dd id="wifiSleep">—</dd></div></dl><p class="muted">每 30 秒检测一次本地网关；连续多次严重丢包才重新连接 Wi-Fi。路由器不响应检测时，会结合网页发送故障判断。</p></section>
)HTML";

static const char WEB_DEBUG[] PROGMEM = R"HTML(




<div class="page-heading"><h1>串口调试</h1><span class="badge" id="logStatus">连接中</span></div><section class="card"><h2>原始指令透传</h2><form id="sendForm"><label for="hex">十六进制指令（发送到美的空调串口）</label><div class="row"><input id="hex" autocomplete="off" spellcheck="false" placeholder="AA 22 AC …" required><button id="send">发送</button></div></form><div class="notice" id="notice" role="status"></div><details><summary>最近空调状态回包</summary><div class="raw" id="raw">等待回包</div></details><div class="row" style="margin-top:18px"><strong>串口收发记录</strong><label style="margin:0 0 0 auto;font-weight:400"><input id="autoScroll" type="checkbox" checked style="width:auto"> 自动滚动</label><button class="quiet" id="clearLog" type="button">清空显示</button></div><p class="muted">时间是“ESP32 本次启动后经过多久”，重启后从零开始。RX 表示 ESP32 收到，TX 表示 ESP32 发出；每行会标注来源模块和方向。</p><div class="log" id="log" aria-label="串口日志"></div><p class="muted">美的空调串口：GPIO17（ESP32 RX）/ GPIO16（ESP32 TX），9600 8N1。米家模块串口：GPIO7（ESP32 RX）/ GPIO8（ESP32 TX），115200 8N1。这里展示的是 ESP32 采集的美的与米家串口数据，不是 ESP32 系统日志。</p></section>
<section class="card"><div class="row"><h2 style="margin:0">IO16 独立 RX 调试</h2><span class="badge" id="io16RxState">读取中</span></div><label><input id="io16RxToggle" type="checkbox" style="width:auto" disabled> 将 IO16 切换为 RX</label><p class="muted">开启后，IO16 仅接收，暂停 ESP32 向美的发送；IO17 继续接收。关闭后恢复 IO16 TX，并停止本窗口更新。清空日志后，刷新页面也不会恢复旧记录。重启默认恢复 TX。本窗口按 9600 8N1 显示 IO16 收到的原始十六进制数据，不混入上方日志。</p><div class="row"><span class="muted" id="io16RxStats">尚未接收数据</span><label style="margin:0 0 0 auto;font-weight:400"><input id="io16RxAutoScroll" type="checkbox" checked style="width:auto"> 自动滚动</label><button class="quiet" id="io16RxClear" type="button">清空日志</button></div><div class="notice" id="io16RxNotice" role="status" aria-live="polite"></div><div class="log" id="io16RxLog" aria-label="IO16 独立接收日志" style="height:30vh;min-height:200px"></div><p class="muted" id="io16RxLogStatus">连接中</p></section>
)HTML";

static const char WEB_SETTINGS[] PROGMEM = R"HTML(



<div class="page-heading"><h1>设置</h1></div><section class="card network-card"><h2>网络名称</h2><form id="hostnameForm" class="network-form"><label for="networkHostnameInput">主机名 <span class="muted">可中文 · <span id="hostnamePreview">0/31 字节</span></span><input id="networkHostnameInput" type="text" minlength="1" maxlength="31" placeholder="主卧空调" autocomplete="off" spellcheck="false" required disabled></label><label for="mdnsHostnameInput">访问地址 <span class="hostname-address"><input id="mdnsHostnameInput" type="text" minlength="1" maxlength="31" placeholder="midea-bedroom" autocomplete="off" autocapitalize="none" spellcheck="false" aria-label="本地访问地址前缀" required disabled><span>.local</span></span></label><div class="network-actions"><button id="hostnameSave" disabled>保存并重启</button><button id="hostnameReset" class="secondary" type="button" disabled>默认</button></div></form><details class="network-help"><summary>名称规则与路由器显示</summary><p>两个名称独立保存。主机名向路由器报告，支持中文，最多 31 个 UTF-8 字节（纯中文约 10 个字）；路由器可能优先显示自己的备注。访问地址仅用 1–31 位英文字母、数字和短横线，首尾不能是短横线，不填写 .local。同一局域网内请使用不同地址。保存后约 3 秒重启，IP 仍可访问。</p></details><div class="notice" id="hostnameNotice" role="status" aria-live="polite"></div><div class="network-links" id="hostnameLinks"></div></section><section class="card"><h2>米家七档风速映射</h2><p class="muted">设置米家 1–7 档各自对应的美的风速值（1–100 整数，按档严格递增；自动档固定为 102）。关闭米家睡眠时使用自定义第 3 档。</p><form id="fanMappingForm"><div class="fan-grid"><div class="fan-item"><span>第 1 档</span><small>滑条 5%</small><input id="fanMap1" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 1 档对应的美的风速值"></div><div class="fan-item"><span>第 2 档</span><small>滑条 10%</small><input id="fanMap2" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 2 档对应的美的风速值"></div><div class="fan-item"><span>第 3 档</span><small>滑条 30%</small><input id="fanMap3" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 3 档对应的美的风速值"></div><div class="fan-item"><span>第 4 档</span><small>滑条 45%</small><input id="fanMap4" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 4 档对应的美的风速值"></div><div class="fan-item"><span>第 5 档</span><small>滑条 60%</small><input id="fanMap5" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 5 档对应的美的风速值"></div><div class="fan-item"><span>第 6 档</span><small>滑条 80%</small><input id="fanMap6" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 6 档对应的美的风速值"></div><div class="fan-item"><span>第 7 档</span><small>滑条 100%</small><input id="fanMap7" type="number" min="1" max="100" step="1" required disabled inputmode="numeric" aria-label="第 7 档对应的美的风速值"></div></div><p class="muted">保存后，后续调档和状态回读使用新映射，重启后仍有效。</p><div class="controls"><button id="fanMappingSave" disabled>保存映射</button><button class="secondary" id="fanMappingReset" type="button" disabled>恢复默认</button></div></form><div class="notice" id="fanMappingNotice" role="status" aria-live="polite"></div></section><section class="card"><h2>固件更新</h2><dl class="facts"><div><dt>当前版本</dt><dd id="firmware">—</dd></div><div><dt>构建时间</dt><dd id="build">—</dd></div><div><dt>可上传固件大小</dt><dd id="otaCapacity">读取中</dd></div></dl><p class="muted">选择为当前 ESP32-S3 编译的应用固件 .bin；上传成功后自动重启。使用应用镜像，不要选择 bootloader、分区表或 merged 合并镜像。</p><form id="firmwareForm"><label for="firmwareFile">固件文件</label><div class="row"><input id="firmwareFile" type="file" accept=".bin,application/octet-stream" required><button id="firmwareUploadButton">上传并安装</button></div></form><progress id="firmwareProgress" value="0" max="100" hidden aria-label="固件上传进度"></progress><div class="notice" id="firmwareNotice" role="status" aria-live="polite"></div></section>
<section class="card"><h2>Wi-Fi 连接设置</h2><form id="wifiForm" class="form-width"><label for="wifiSelect">附近的 Wi-Fi 信号</label><div class="row" style="margin-bottom:10px"><select id="wifiSelect" style="flex:1"><option value="">点击右侧按钮扫描 Wi-Fi</option></select><button class="secondary" id="wifiScanButton" type="button">扫描网络</button></div><label for="wifiSsidInput">Wi-Fi 名称 (SSID)</label><input id="wifiSsidInput" type="text" maxlength="32" placeholder="可从上方选择，或手动输入隐藏网络" required autocomplete="off"><label for="wifiPasswordInput">Wi-Fi 密码</label><input id="wifiPasswordInput" type="password" maxlength="64" placeholder="开放网络留空即可" autocomplete="new-password"><p class="muted">当前已连接：<strong id="currentWifiSsid">—</strong>。保存后 ESP32 会在 3 秒后自动重启并连接新网络。</p><div class="controls"><button id="wifiSaveButton">保存 Wi-Fi 并重启</button></div></form><div class="notice" id="wifiNotice" role="status" aria-live="polite"></div></section><section class="card"><h2>修改网页登录密码</h2><form id="passwordForm" class="form-width"><label for="oldPassword">当前密码</label><input id="oldPassword" type="password" autocomplete="current-password" required><label for="newPassword">新密码</label><input id="newPassword" type="password" autocomplete="new-password" placeholder="6–64 个字符" minlength="6" maxlength="64" required><label for="confirmPassword">确认新密码</label><input id="confirmPassword" type="password" autocomplete="new-password" minlength="6" maxlength="64" required><div class="controls"><button>保存密码</button></div></form><div class="notice" id="passwordNotice" role="status"></div><p class="muted">密码保存到 ESP32，重启后仍有效。网页只需密码登录，App 认证使用相同密码。</p></section><section class="card"><h2>蓝牙功能</h2><label><input id="bleEnabledToggle" type="checkbox" style="width:auto"> 启用 BLE 蓝牙</label><p class="muted">关闭后手机 BLE App 将无法发现或连接设备；网页、Wi-Fi、HomeKit 和米家桥接不受影响。切换后 ESP32 会在 3 秒后自动重启生效。</p><div class="notice" id="bleNotice" role="status" aria-live="polite"></div></section><p class="footer-note">HomeKit 配对与米家配网位于“智能接入”。网络、CPU 与内存信息位于“设备状态”。</p>
)HTML";

static const char WEB_SCRIPT[] PROGMEM = R"JS(
'use strict';
const $=id=>document.getElementById(id),page=location.pathname.replace(/\/$/,'')||'/',modeNames={1:'自动',2:'制冷',3:'除湿',4:'制热',5:'送风'},fanNames={20:'静音',40:'低风',60:'中风',80:'高风',100:'强劲',101:'自动',102:'自动'},netNames={updating:'模块报告更新中（未提供进度）',uap:'等待配网',unprov:'等待配网',cloud:'米家云已连接',local:'本地网络已连接',offline:'离线',requested:'已请求配网，等待回报',unknown:'未知'};
let cursor=0,statusBusy=false,metricsBusy=false,logBusy=false,updating=false,lastStatus=null,otaCapacity=0,actionTimer=0;
let timerBusy=false;
let hostnameBusy=false,hostnameDirty=false,hostnameLoaded=false,hostnameRestarting=false;
let fanMappingBusy=false,fanMappingDirty=false,fanMappingLoaded=false,fanMappingDefaults=[],fanMappingRevision=0,lastFanMapping=null;
let io16RxCursor=0,io16RxBusy=false,io16RxLogBusy=false,io16RxEnabled=false,lastIo16RxState=null,io16RxClearBusy=false,io16RxRevision=0;
const text=(id,value)=>{if($(id))$(id).textContent=value},bind=(id,event,fn)=>{if($(id))$(id).addEventListener(event,fn)};
for(const a of document.querySelectorAll('nav a'))if(a.getAttribute('href')===page)a.setAttribute('aria-current','page');
const heading=document.querySelector('h1');if(heading)document.title=heading.textContent+' · Midea Bridge';
function note(id,message,error=false){text(id,message);if($(id))$(id).dataset.error=String(error)}
function connected(ok){$('connection').dataset.connected=String(ok);text('connectionText',ok?'设备已连接':'连接中断')}
function bytes(n){if(!Number.isFinite(n))return '—';return n>=1048576?(n/1048576).toFixed(2)+' MB':(n/1024).toFixed(1)+' KB'}
function percent(n){return Number.isFinite(n)?n.toFixed(1)+'%':'—'}
function meter(id,n){if($(id))$(id).value=Math.max(0,Math.min(100,Number(n)||0))}
function duration(n){n=Math.max(0,Math.floor(n||0));return (n>=86400?Math.floor(n/86400)+'天 ':'')+Math.floor(n%86400/3600)+'时 '+Math.floor(n%3600/60)+'分 '+n%60+'秒'}
async function request(url,options={}){
  const {timeout=5000,...fetchOptions}=options,c=new AbortController();
  const t=setTimeout(()=>c.abort(),timeout);
  try{
    const r=await fetch(url,{cache:'no-store',...fetchOptions,signal:c.signal});
    if(r.status===401){location.replace('/login');throw Error('登录已失效')}
    // fetch resolves at headers. Keep the deadline until the body is complete,
    // otherwise a stalled body leaves statusBusy/logBusy permanently set.
    const body=await r.text();
    if(!r.ok)throw Error(body||('HTTP '+r.status));
    return {ok:r.ok,status:r.status,headers:r.headers,text:async()=>body,json:async()=>JSON.parse(body)};
  }catch(e){throw(e.name==='AbortError'?Error('请求超时('+Math.ceil(timeout/1000)+'秒)'):e)}
  finally{clearTimeout(t)}
}
async function post(url,body){return (await request(url,{method:'POST',headers:{'Content-Type':'text/plain'},body})).text()}
function renderTimers(s){
  if(!$('timerState'))return;
  const t=s.timers||{},available=t.known&&!s.mockMode;
  text('timerState',s.mockMode?'脱机模式不可用':!available?'等待回包':!t.fresh?'回包已过期':t.pending?'等待设置确认':'回包 '+t.age+' 秒前');
  $('timerState').dataset.stale=String(!available||!t.fresh);
  for(const [key,id]of [['on','timerOnValue'],['off','timerOffValue']]){
    const m=Number(t[key+'Minutes'])||0;
    text(id,!available?'等待空调回报':!t[key]?'未启用':t.absolute?'时刻定时（请使用原控制器调整）':'剩余 '+Math.floor(m/60)+' 小时 '+m%60+' 分钟'+(!t.fresh?'（旧回报）':''));
  }
  for(const b of document.querySelectorAll('[data-timer-action]'))b.disabled=timerBusy||updating||!available||!t.fresh||t.pending||s.miotPending||(t.absolute&&(t.on||t.off));
  $('timerQuery').disabled=timerBusy||updating||!!s.mockMode;
  note('timerResultNotice',t.result||'',t.resultState==='timeout'||t.resultState==='error');
}
async function setTimer(target,minutes){
  if(timerBusy||updating)return;
  timerBusy=true;if(lastStatus)renderTimers(lastStatus);
  note('timerActionNotice','正在发送…');
  try{const message=await post('/api/timers',JSON.stringify({target,minutes}));note('timerActionNotice',message);await getState();note('timerActionNotice','')}
  catch(e){note('timerActionNotice',e.message,true)}
  finally{timerBusy=false;if(lastStatus)renderTimers(lastStatus)}
}
for(const [name,target]of [['On','on'],['Off','off']]){
  bind('timer'+name+'Form','submit',e=>{e.preventDefault();const h=Number($('timer'+name+'Hours').value),m=Number($('timer'+name+'Minutes').value),total=h*60+m;if(!Number.isInteger(h)||!Number.isInteger(m)||h<0||h>24||m<0||m>59||total<1||total>1440){note('timerActionNotice','请输入 1 分钟至 24 小时的倒计时。',true);return}setTimer(target,total)});
  bind('timer'+name+'Cancel','click',()=>setTimer(target,0));
}
bind('timerQuery','click',async()=>{if(timerBusy||updating)return;timerBusy=true;if(lastStatus)renderTimers(lastStatus);try{note('timerActionNotice',await post('/api/control','query'));clearTimeout(actionTimer);actionTimer=setTimeout(getState,700)}catch(e){note('timerActionNotice',e.message,true)}finally{timerBusy=false;if(lastStatus)renderTimers(lastStatus)}});
async function getState(){if(statusBusy||updating||document.hidden)return;statusBusy=true;const io16Revision=io16RxRevision,fanRevision=fanMappingRevision;try{const s=await(await request('/api/status')).json();lastStatus=s;renderNetworkHostname(s);if(fanRevision===fanMappingRevision&&!fanMappingBusy)renderFanMapping(s.fanMapping);if(io16Revision===io16RxRevision)renderIo16Rx(s.io16Rx);text('miotMode',s.miotModeEstimated?('自动 → '+({2:'制冷',5:'制热'}[s.miotMode]||'未知')+'（估算）'):({2:'制冷',3:'除湿',4:'送风',5:'制热',6:'关机'}[s.miotMode]||'未确认'));renderMiotSimulation(s);renderMockMode(s);renderTimers(s);text('miotLatency',(s.miotResponseMs??0)+' ms · 最大 '+(s.miotMaxResponseMs??0)+' ms');text('miotServiceGap','最大 '+(s.miotMaxServiceGapMs??0)+' ms · 超过100ms '+(s.miotSlowServiceGaps??0)+' 次');text('miotUartErrors',(s.miotRxOverflows??0)+' / '+(s.miotSignalErrors??0));text('miotLastTimeout',s.miotLastTimeoutCommand||'无');connected(true);text('state',s.mockMode?(s.power?'[脱机] 空调已开机':'[脱机] 空调已关机'):s.known?(s.power?'空调已开机':'空调已关机'):'尚未收到空调状态');text('stateAge',s.mockMode?'脱机模拟模式':s.known?'回包 '+s.age+' 秒前':'等待回包');if($('stateAge'))$('stateAge').dataset.stale=String(!s.mockMode&&!s.fresh);text('targetTemperature',s.known?Number(s.temp).toFixed(1):'—');text('room','室温 '+(s.roomKnown?Number(s.room).toFixed(1)+'°C':'—'));text('acMode',s.known?(modeNames[s.mode]||'未知'):'—');text('acFan',s.known?(fanNames[s.fan]||('风速码 '+s.fan)):'—');for(const [id,key]of [['acSwing','swing'],['acEco','eco'],['acSleep','sleep']])text(id,s.known?(s[key]?'开启':'关闭'):'—');text('raw',s.raw||'尚无状态回包');text('hk',s.homekit?'已配对':'未配对');const code=(s.homekitCode||'').replace(/\D/g,'');text('homekitCode',code.length===8?code.slice(0,3)+'-'+code.slice(3,5)+'-'+code.slice(5):'未配置');text('miotSeen',s.miotSeen?'协议近期有应答':'近期无协议应答');text('miotModel',s.miotModel||'尚未识别');text('miotNet',(netNames[s.miotNet]||s.miotNet||'未知')+(s.miotNetAge>=15000?' · 状态已过期':''));text('miotProfile',(s.miotProfile||'未识别')+' / '+(s.miotInitialized?'已握手':'等待握手'));text('miotReports',String(s.miotReports||0)+' 次已确认'+(s.miotReportAge>=0?' · '+Math.floor(s.miotReportAge/1000)+' 秒前':''));text('miotConfirmed',String(s.miotConfirmed||0)+' 次实际回包确认'+(s.miotPending?' · 正在等待美的':''));text('miotResult',s.miotResult||'—');text('wifiState',s.wifiConnected?'已连接 · '+s.ssid:'未连接路由器');text('deviceIp',s.ip||'—');text('rssi',s.wifiConnected?s.rssi+' dBm':'—');text('bleName',s.bleEnabled?(s.deviceName||'—'):'已关闭');text('bleState',!s.bleEnabled?'已关闭':s.bleConnected?'手机已连接':'等待连接');if($('bleEnabledToggle')&&!$('bleEnabledToggle').disabled)$('bleEnabledToggle').checked=!!s.bleEnabled;text('firmware',s.firmware||'—');text('currentWifiSsid',s.wifiConnected?(s.ssid||'已连接'):'未连接');if($('wifiSsidInput')&&!$('wifiSsidInput').value&&s.ssid)$('wifiSsidInput').value=s.ssid;}catch(e){connected(false)}finally{statusBusy=false}}
async function getMetrics(){if(!($('cpuLoad')||$('firmwareForm'))||metricsBusy||updating||document.hidden)return;metricsBusy=true;try{const s=await(await request('/api/system')).json(),c=s.cpu,r=s.ram,f=s.flash,p=s.psram;otaCapacity=f.otaCapacity;text('build',s.build);text('otaCapacity',f.otaCapacity?bytes(f.otaCapacity):'没有 OTA 备用分区');text('chip',s.chip+' rev '+s.revision);text('uptime',duration(s.uptime));text('cpuLoad',c.available?(c.ready?percent(c.load):'采样中…'):'暂不可用');meter('cpuBar',c.ready?c.load:0);text('cpuCores',c.available&&c.ready?c.cores.map(x=>'核心 '+x.id+'：'+percent(x.load)).join('　 / 　'):'等待首次采样');text('cpuDetails',s.coreCount+' 核 · '+c.mhz+' MHz · '+(c.ready?'最近 '+(c.sampleMs/1000).toFixed(1)+' 秒平均':'约 1 秒后更新'));text('ramUsage',percent(r.usage));meter('ramBar',r.usage);text('ramSummary','已用 '+bytes(r.used)+' / 可分配 '+bytes(r.total));text('ramFree',bytes(r.free));text('ramMin',bytes(r.minFree));text('ramBlock',bytes(r.maxBlock));text('flashUsage',f.appTotal?percent(f.appUsage):'不可用');meter('flashBar',f.appUsage);text('flashSummary','当前固件 '+bytes(f.appUsed)+' / 应用分区 '+bytes(f.appTotal));text('flashTotal',bytes(f.total));text('flashPartition',f.partition+' / '+bytes(f.appFree));text('psramState',p.total?percent(p.usage)+' · 总计 '+bytes(p.total):'未检测到可用 PSRAM');if($('psramDetails'))$('psramDetails').hidden=!p.total;meter('psramBar',p.usage);text('psramSummary','已用 '+bytes(p.used)+' / 可用 '+bytes(p.free)+(s.logBuffer&&s.logBuffer.inPsram?' · 日志缓存 '+s.logBuffer.capacity+' 条（'+bytes(s.logBuffer.allocatedBytes)+'）':' · 日志缓存使用内部 RAM'));text('metricsUpdated','每 2 秒刷新');note('metricsNotice','')}catch(e){note('metricsNotice','设备指标读取失败：'+e.message,true);text('metricsUpdated','读取中断')}finally{metricsBusy=false}}
async function poll(){if(!$('log')||logBusy||updating||document.hidden)return;logBusy=true;try{const j=await(await request('/api/log?since='+cursor)).json();if(Number.isFinite(j.latestId)&&j.latestId<cursor){cursor=0;$('log').replaceChildren()}const added=[];for(const e of j.entries){cursor=Math.max(cursor,e.id);const dir=(e.dir||'').toUpperCase();const meta=({RX:['midea-rx','美的空调','空调 → ESP32'],TX:['midea-tx','美的空调','ESP32 → 空调'],MRX:['miot-rx','米家模块','米家模块 → ESP32'],MTX:['miot-tx','米家模块','ESP32 → 米家模块'],MCTL:['system','ESP32解析','米家请求 → 美的目标'],MTO:['system','ESP32','等待米家回复超时']})[dir]||['system','ESP32','ESP32内部'];const line=document.createElement('div');line.className='log-entry '+meta[0];const time=document.createElement('span');time.className='log-time';time.textContent=e.time||'';const source=document.createElement('span');source.className='log-source';source.textContent=meta[1];const direction=document.createElement('span');direction.className='log-direction';direction.textContent=meta[2];const data=document.createElement('span');data.className='log-data';data.textContent=e.hex;line.append(time,source,direction,data);added.push(line)}if(added.length){$('log').append(...added);while($('log').children.length>500)$('log').firstChild.remove();if($('autoScroll').checked)$('log').scrollTop=$('log').scrollHeight;}text('logStatus','已连接 · '+new Date().toLocaleTimeString())}catch(e){text('logStatus','连接中断: '+(e.message||'超时'))}finally{logBusy=false}}
for(const b of document.querySelectorAll('[data-cmd]'))b.addEventListener('click',async()=>{if(updating)return;b.disabled=true;note('notice','命令发送中…');try{note('notice',await post('/api/control',b.dataset.cmd));clearTimeout(actionTimer);actionTimer=setTimeout(getState,700)}catch(e){note('notice',e.message,true)}finally{b.disabled=false}});
for(const b of document.querySelectorAll('[data-miot]'))b.addEventListener('click',async()=>{b.disabled=true;note('miotNotice','正在查询…');try{note('miotNotice',await post('/api/miot/query',b.dataset.miot))}catch(e){note('miotNotice',e.message,true)}finally{b.disabled=false}});
bind('miotTestEnvironmentButton','click',async e=>{const b=e.currentTarget;b.disabled=true;note('miotTestEnvironmentNotice','正在发送一次性测试状态…');try{note('miotTestEnvironmentNotice',await post('/api/miot/test-environment','send'))}catch(e){note('miotTestEnvironmentNotice',e.message,true)}finally{b.disabled=false}});
function updateHostnamePreview(){
  if($('networkHostnameInput'))text('hostnamePreview',new TextEncoder().encode($('networkHostnameInput').value.trim()).length+'/31 字节');
}
function renderNetworkHealth(s){
  const h=s.wifiHealth;if(!h)return;
  text('wifiAp',(h.bssid||'未连接')+' / 信道 '+(h.channel||'—'));
  text('wifiProbe',h.known?('收到 '+h.received+'/'+h.sent+' · 平均 '+h.meanMs+' ms'+(h.badRounds?' · 连续异常 '+h.badRounds+' 轮':'')):(h.probing?'检测中…':'等待首次检测'));
  text('wifiRecoveries',h.recoveries+' 次'+(h.lastRecoveryAge>=0?' · 最近 '+duration(h.lastRecoveryAge)+'前':''));
  text('wifiDisconnects',h.disconnects+' 次'+(h.lastDisconnectReason?' · 最近原因码 '+h.lastDisconnectReason:''));
  text('wifiSleep',h.sleep<0?'读取中':h.sleep===0?'已关闭':'已启用');
}
function renderNetworkHostname(s){
  if(!s)return;
  renderNetworkHealth(s);
  const name=s.networkHostname||'midea-s3',mdns=s.mdnsHostname||'midea-s3';
  text('networkHostnameValue',name);text('deviceMac',s.mac||'—');
  const link=$('localHostnameLink');if(link){link.textContent=mdns+'.local';link.href=s.url||('http://'+mdns+'.local/')}
  if(!$('hostnameForm'))return;
  if(!hostnameBusy&&!hostnameDirty&&!hostnameRestarting){$('networkHostnameInput').value=name;$('mdnsHostnameInput').value=mdns}
  hostnameLoaded=true;
  for(const id of ['networkHostnameInput','mdnsHostnameInput','hostnameSave','hostnameReset'])$(id).disabled=hostnameBusy||hostnameRestarting||updating;
  updateHostnamePreview();
}
for(const id of ['networkHostnameInput','mdnsHostnameInput'])bind(id,'input',()=>{hostnameDirty=true;updateHostnamePreview()});
bind('hostnameReset','click',()=>{
  if(hostnameBusy||hostnameRestarting||updating)return;
  $('networkHostnameInput').value='midea-s3';$('mdnsHostnameInput').value='midea-s3';hostnameDirty=true;updateHostnamePreview();
  note('hostnameNotice','两个名称已填入默认值，保存后生效。');
});
bind('hostnameForm','submit',async e=>{
  e.preventDefault();if(hostnameBusy||hostnameRestarting||updating||!hostnameLoaded)return;
  const name=$('networkHostnameInput').value.trim(),mdns=$('mdnsHostnameInput').value.trim().toLowerCase();
  if(!name||new TextEncoder().encode(name).length>31||/[\u0000-\u001f\u007f-\u009f]/.test(name)){
    note('hostnameNotice','主机名支持中文，限 1–31 个 UTF-8 字节（纯中文约 10 个字），不能含控制字符。',true);return;
  }
  if(!/^[a-z0-9](?:[a-z0-9-]{0,29}[a-z0-9])?$/.test(mdns)){
    note('hostnameNotice','访问地址须为 1–31 位英文字母、数字或短横线，首尾不能是短横线；无需填写 .local。',true);return;
  }
  hostnameBusy=true;renderNetworkHostname(lastStatus);note('hostnameNotice','正在保存网络名称…');$('hostnameLinks').replaceChildren();
  try{
    const result=await(await request('/api/network/hostname',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({hostname:name,mdnsHostname:mdns})})).json();
    $('networkHostnameInput').value=result.hostname;$('mdnsHostnameInput').value=result.mdnsHostname;updateHostnamePreview();note('hostnameNotice',result.message);
    if(result.restart){
      hostnameRestarting=true;
      for(const [title,url]of [['新地址',result.url],['IP 访问',result.ipUrl]]){
        if(!url)continue;const a=document.createElement('a');a.textContent=title+'：'+url;a.href=url+'settings';$('hostnameLinks').appendChild(a);
      }
      const target=location.hostname.toLowerCase().endsWith('.local')?result.url:(result.ipUrl||result.url);
      setTimeout(()=>location.assign(target+'settings'),8000);
    }else hostnameDirty=false;
  }catch(error){note('hostnameNotice',error.message,true)}
  finally{if(!hostnameRestarting){hostnameBusy=false;renderNetworkHostname(lastStatus)}}
});
function renderFanMapping(mapping){
  if(!$('fanMappingForm')||!mapping||!Array.isArray(mapping.levels)||mapping.levels.length!==7)return;
  lastFanMapping=mapping;fanMappingDefaults=mapping.defaults||fanMappingDefaults;
  if(!fanMappingBusy&&!fanMappingDirty){
    for(let i=0;i<7;i++)$('fanMap'+(i+1)).value=String(mapping.levels[i]);
  }
  fanMappingLoaded=true;
  for(let i=1;i<=7;i++)$('fanMap'+i).disabled=fanMappingBusy||updating;
  $('fanMappingSave').disabled=$('fanMappingReset').disabled=fanMappingBusy||updating;
}
bind('fanMappingForm','input',()=>{fanMappingDirty=true});
bind('fanMappingReset','click',()=>{
  if(fanMappingBusy||updating||fanMappingDefaults.length!==7)return;
  for(let i=0;i<7;i++)$('fanMap'+(i+1)).value=String(fanMappingDefaults[i]);
  fanMappingDirty=true;note('fanMappingNotice','已填入默认值，点击“保存映射”生效。');
});
bind('fanMappingForm','submit',async e=>{
  e.preventDefault();if(fanMappingBusy||updating||!fanMappingLoaded)return;
  const levels=Array.from({length:7},(_,i)=>Number($('fanMap'+(i+1)).value));
  if(levels.some((value,i)=>!Number.isInteger(value)||value<1||value>100||(i>0&&value<=levels[i-1]))){
    note('fanMappingNotice','请填写七个 1–100 的整数，并按档位严格递增。',true);return;
  }
  fanMappingBusy=true;++fanMappingRevision;renderFanMapping(lastFanMapping);note('fanMappingNotice','正在保存…');
  try{
    const mapping=await(await request('/api/fan-mapping',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({levels})})).json();
    lastFanMapping=mapping;if(lastStatus)lastStatus.fanMapping=mapping;
    fanMappingDirty=false;note('fanMappingNotice',mapping.message||'风速映射已保存。');
  }catch(error){note('fanMappingNotice',error.message,true)}
  finally{++fanMappingRevision;fanMappingBusy=false;renderFanMapping(lastFanMapping)}
});
function renderMiotSimulation(s){const b=$('miotSimulationToggle');if(b&&!b.disabled)b.checked=!!s.miotSimulationEnabled;text('miotSimulationState',s.miotSimulationActive?'已启用 · 模拟数据':s.miotSimulationEnabled?'等待 C20 模块':'已关闭');text('miotSimulationControls','已处理 '+(s.miotSimulatedControls||0)+' 次模拟属性控制')}
function renderMockMode(s){const b=$('mockToggle');if(b&&!b.disabled)b.checked=!!s.mockMode;text('mockBadge',s.mockMode?'已启用 · 脱机调试':'已关闭 · 真实空调');if($('mockBadge'))$('mockBadge').dataset.stale=String(!!s.mockMode)}
bind('mockToggle','change',async e=>{const b=e.currentTarget,enabled=b.checked;b.disabled=true;note('mockNotice','正在保存…');try{note('mockNotice',await post('/api/mock',enabled?'on':'off'))}catch(e){b.checked=!!(lastStatus&&lastStatus.mockMode);note('mockNotice',e.message,true)}finally{b.disabled=false;getState()}});
bind('miotSimulationToggle','change',async e=>{const b=e.currentTarget,enabled=b.checked;b.disabled=true;note('miotSimulationNotice','正在保存…');try{note('miotSimulationNotice',await post('/api/miot/simulation',enabled?'on':'off'))}catch(e){b.checked=!!(lastStatus&&lastStatus.miotSimulationEnabled);note('miotSimulationNotice',e.message,true)}finally{b.disabled=false;getState()}});
bind('unpairButton','click',async e=>{if(!confirm('清除 ESP32 上所有 HomeKit 配对记录？之后需要在“家庭”App 移除旧配件，再重新添加。'))return;const b=e.currentTarget;b.disabled=true;try{note('homekitNotice',await post('/api/homekit/unpair','delete'));getState()}catch(e){note('homekitNotice',e.message,true)}finally{b.disabled=false}});
bind('miotProvisionButton','click',async e=>{if(!confirm('米家模组将清除原 Wi-Fi 并重启，之后需要在米家 App 重新配网。确定继续？'))return;const b=e.currentTarget;b.disabled=true;try{note('miotProvisionNotice',await post('/api/miot/provision','restore'));getState()}catch(e){note('miotProvisionNotice',e.message,true)}finally{b.disabled=false}});
bind('passwordForm','submit',async e=>{e.preventDefault();const form=e.currentTarget,b=form.querySelector('button'),newp=$('newPassword').value;if(newp!==$('confirmPassword').value){note('passwordNotice','两次输入的新密码不一致。',true);return}b.disabled=true;note('passwordNotice','正在保存…');try{note('passwordNotice',await post('/api/password',$('oldPassword').value+'\n'+newp));form.reset()}catch(e){note('passwordNotice','修改失败：'+e.message,true)}finally{b.disabled=false}});
bind('wifiForm','submit',async e=>{e.preventDefault();const ssid=$('wifiSsidInput').value.trim(),password=$('wifiPasswordInput').value;if(!ssid)return;if(!confirm('将 Wi-Fi 修改为「'+ssid+'」并重启设备？'))return;const b=$('wifiSaveButton');b.disabled=true;note('wifiNotice','正在保存 Wi-Fi 设置…');try{note('wifiNotice',await post('/api/wifi',JSON.stringify({ssid,password})))}catch(e){note('wifiNotice','保存失败：'+e.message,true);b.disabled=false}});
bind('wifiScanButton','click',async()=>{const b=$('wifiScanButton'),sel=$('wifiSelect');b.disabled=true;b.textContent='扫描中…';note('wifiNotice','正在扫描附近 2.4 GHz Wi-Fi…');try{const res=await(await request('/api/wifi/scan',{timeout:15000})).json();sel.replaceChildren();if(!res.networks||!res.networks.length){sel.add(new Option('未发现网络 · 可手动输入',''));note('wifiNotice','未发现广播网络，可手动输入 SSID。');}else{sel.add(new Option('选择附近的 Wi-Fi ('+res.networks.length+'个)',''));res.networks.forEach(n=>{const sig=n.rssi>=-60?'强':n.rssi>=-75?'中':'弱';sel.add(new Option(n.ssid+' ('+(n.secure?'加密':'开放')+', 信号'+sig+')',n.ssid));});note('wifiNotice','扫描完成，已找到 '+res.networks.length+' 个网络。');}}catch(e){note('wifiNotice','扫描失败：'+e.message,true);}finally{b.disabled=false;b.textContent='扫描网络';}});
bind('wifiSelect','change',()=>{const val=$('wifiSelect').value;if(val){$('wifiSsidInput').value=val;if(!$('wifiPasswordInput').value)$('wifiPasswordInput').focus();}});
bind('bleEnabledToggle','change',async e=>{const b=e.currentTarget,enabled=b.checked;b.disabled=true;note('bleNotice','正在保存蓝牙设置…');try{note('bleNotice',await post('/api/ble/config',enabled?'on':'off'))}catch(e){b.checked=!!(lastStatus&&lastStatus.bleEnabled);note('bleNotice',e.message,true)}finally{b.disabled=false;getState()}});
bind('sendForm','submit',async e=>{e.preventDefault();const b=$('send');b.disabled=true;try{note('notice',await post('/api/send',$('hex').value));$('hex').value='';poll()}catch(e){note('notice','发送失败：'+e.message,true)}finally{b.disabled=io16RxEnabled||io16RxBusy}});
bind('clearLog','click',()=>$('log').replaceChildren());
function renderIo16Rx(s){
  if(!s||!$('io16RxToggle'))return;
  lastIo16RxState=s;io16RxEnabled=!!s.enabled;
  const toggle=$('io16RxToggle');toggle.disabled=io16RxBusy||io16RxClearBusy;
  if(!io16RxBusy&&!io16RxClearBusy)toggle.checked=io16RxEnabled;
  text('io16RxState',io16RxEnabled?'IO16 RX · 美的 TX 已暂停':'IO16 TX · 独立接收已关闭');
  text('io16RxStats',(s.bytes||0)+' 字节 · '+(s.frames||0)+' 批次 · 溢出 '+(s.overflows||0)+' · 信号错误 '+(s.signalErrors||0));
  if($('send'))$('send').disabled=io16RxEnabled||io16RxBusy;
  if($('hex'))$('hex').disabled=io16RxEnabled||io16RxBusy;
  if($('io16RxClear'))$('io16RxClear').disabled=io16RxClearBusy||io16RxBusy;
  if(!io16RxEnabled)text('io16RxLogStatus','已暂停更新 · IO16 RX 已关闭');
}
async function pollIo16Rx(){
  if(!$('io16RxLog')||!io16RxEnabled||io16RxBusy||io16RxClearBusy||io16RxLogBusy||updating||document.hidden)return;
  io16RxLogBusy=true;const revision=io16RxRevision;
  try{
    const j=await(await request('/api/io16/log?since='+io16RxCursor)).json();
    if(revision!==io16RxRevision||io16RxBusy||io16RxClearBusy||!io16RxEnabled)return;
    renderIo16Rx(j.state);if(!io16RxEnabled)return;
    if(Number.isFinite(j.state?.latestId)&&j.state.latestId<io16RxCursor){io16RxCursor=0;$('io16RxLog').replaceChildren()}
    const added=[];
    for(const e of j.entries||[]){
      if(e.dir!=='IO16RX')continue;
      io16RxCursor=Math.max(io16RxCursor,e.id);
      const line=document.createElement('div');line.className='log-entry midea-rx';
      for(const [cls,value]of [['log-time',e.time||''],['log-source','IO16'],['log-direction','IO16 → ESP32'],['log-data',e.hex||'']]){
        const span=document.createElement('span');span.className=cls;span.textContent=value;line.appendChild(span);
      }
      added.push(line);
    }
    if(added.length){
      $('io16RxLog').append(...added);
      while($('io16RxLog').children.length>500)$('io16RxLog').firstChild.remove();
      if($('io16RxAutoScroll').checked)$('io16RxLog').scrollTop=$('io16RxLog').scrollHeight;
    }
    text('io16RxLogStatus','独立窗口已连接 · '+new Date().toLocaleTimeString());
  }catch(e){if(revision===io16RxRevision&&io16RxEnabled)text('io16RxLogStatus','独立窗口连接中断：'+(e.message||'超时'))}
  finally{io16RxLogBusy=false}
}
bind('io16RxToggle','change',async e=>{
  if(io16RxBusy||io16RxClearBusy||updating)return;
  io16RxBusy=true;++io16RxRevision;const enabled=e.currentTarget.checked;e.currentTarget.disabled=true;
  if($('send'))$('send').disabled=true;if($('hex'))$('hex').disabled=true;
  note('io16RxNotice',enabled?'正在切换 IO16 为 RX…':'正在恢复 IO16 TX…');
  try{const message=await post('/api/io16/rx',enabled?'on':'off');lastIo16RxState={...(lastIo16RxState||{}),enabled};note('io16RxNotice',message)}
  catch(error){e.currentTarget.checked=io16RxEnabled;note('io16RxNotice',error.message,true)}
  finally{io16RxBusy=false;renderIo16Rx(lastIo16RxState);getState();pollIo16Rx()}
});
bind('io16RxClear','click',async()=>{
  if(io16RxClearBusy||io16RxBusy||updating)return;
  io16RxClearBusy=true;++io16RxRevision;
  $('io16RxClear').disabled=true;$('io16RxToggle').disabled=true;
  try{
    const j=await(await request('/api/io16/log/clear',{method:'POST',headers:{'Content-Type':'text/plain'},body:'clear'})).json();
    io16RxCursor=j.latestId;$('io16RxLog').replaceChildren();note('io16RxNotice',j.message||'IO16 日志已清空。');
  }catch(e){note('io16RxNotice','清空失败：'+e.message,true)}
  finally{io16RxClearBusy=false;renderIo16Rx(lastIo16RxState)}
});
bind('logoutButton','click',async e=>{const b=e.currentTarget;b.disabled=true;try{await request('/api/logout',{method:'POST'});location.replace('/login')}catch(e){b.disabled=false;text('connectionText','退出失败，请重试')}});
function sendFirmwareOnce(file,progress,attempt){return new Promise((resolve,reject)=>{const data=new FormData();data.append('firmware',file,file.name);const xhr=new XMLHttpRequest();xhr.timeout=180000;xhr.open('POST','/api/update');xhr.upload.onprogress=e=>{if(e.lengthComputable){progress.value=Math.round(e.loaded/e.total*100);note('firmwareNotice','第 '+(attempt+1)+' 次上传：'+progress.value+'%')}};xhr.onload=()=>resolve({status:xhr.status,text:xhr.responseText||''});xhr.onerror=()=>reject(Error('传输连接中断'));xhr.ontimeout=()=>reject(Error('上传等待超时'));xhr.onabort=()=>reject(Error('上传已取消'));xhr.send(data)})}
async function verifyFirmwareInstalled(file,previousVersion){const match=file.name.match(/(?:^|[-_])(\d+\.\d+\.\d+-s3)(?:[-_.]|$)/),target=match&&match[1];let latest='';for(let i=0;i<8;i++){try{const r=await fetch('/api/status',{cache:'no-store'});if(r.ok){const s=await r.json();latest=s.firmware||'';if(target?latest===target:(previousVersion&&latest!==previousVersion))return {installed:true,version:latest};return {installed:false,version:latest}}}catch(_){}await new Promise(resolve=>setTimeout(resolve,2000))}return {installed:false,version:latest}}
bind('firmwareForm','submit',async e=>{e.preventDefault();if(hostnameBusy||hostnameRestarting){note('firmwareNotice','正在保存网络名称或等待重启，请重新连接后再更新固件。',true);return}const input=$('firmwareFile'),file=input.files[0],b=$('firmwareUploadButton'),progress=$('firmwareProgress');if(!file||!file.size||!file.name.toLowerCase().endsWith('.bin')){note('firmwareNotice','请选择非空的应用固件 .bin 文件。',true);return}if(otaCapacity&&file.size>otaCapacity){note('firmwareNotice','文件超过 OTA 应用分区容量：'+bytes(otaCapacity),true);return}if(!confirm('安装 '+file.name+'？上传期间保持设备供电，成功后自动重启。'))return;const previousVersion=lastStatus&&lastStatus.firmware||'';updating=true;b.disabled=true;input.disabled=true;$('logoutButton').disabled=true;progress.hidden=false;progress.value=0;try{for(let attempt=0;attempt<3;attempt++){try{const result=await sendFirmwareOnce(file,progress,attempt);if(result.status>=200&&result.status<300){progress.value=100;note('firmwareNotice',(result.text||'固件校验并写入成功。')+' 约 10 秒后重新连接并核对版本。');setTimeout(()=>{updating=false;location.reload()},10000);return}if(result.status===401){updating=false;location.replace('/login');return}note('firmwareNotice','更新失败：'+(result.text||('HTTP '+result.status)),true);return}catch(error){note('firmwareNotice','正在确认设备状态：'+error.message);const check=await verifyFirmwareInstalled(file,previousVersion);if(check.installed){progress.value=100;note('firmwareNotice','设备已切换到 '+check.version+'，刚才的断线是更新重启造成的。');setTimeout(()=>{updating=false;location.reload()},5000);return}if(attempt<2){note('firmwareNotice','设备当前版本 '+(check.version||'暂不可读')+'，准备第 '+(attempt+2)+' 次上传…');await new Promise(resolve=>setTimeout(resolve,1000));continue}note('firmwareNotice','更新未确认完成，设备当前版本 '+(check.version||'无法连接')+'。可保持设备供电后重试。',true)}}}finally{updating=false;b.disabled=false;input.disabled=false;$('logoutButton').disabled=false}});
// Pause background requests during upload and in hidden tabs; do not accumulate
// overlapping requests when the single-threaded device server is occupied.
const backgroundJobs=[
  {run:getState,period:()=>1000,next:0},
  ...($('cpuLoad')||$('firmwareForm')?[{run:getMetrics,period:()=>2000,next:0}]:[]),
  ...($('log')?[{run:poll,period:()=>700,next:0}]:[]),
  ...($('io16RxLog')?[{run:pollIo16Rx,period:()=>700,next:0}]:[])
];
let refreshRunning=false,refreshTimer=0;
async function refreshBackground(){
  if(refreshRunning)return;
  clearTimeout(refreshTimer);refreshRunning=true;
  try{
    for(const job of backgroundJobs){
      if(updating||hostnameRestarting||document.hidden)break;
      if(Date.now()<job.next)continue;
      try{await job.run()}catch(_){}finally{job.next=Date.now()+job.period()}
    }
  }finally{refreshRunning=false;refreshTimer=setTimeout(refreshBackground,document.hidden?2000:200)}
}
// Start one background refresh loop; actions can still run immediately.
globalThis.bridgeAppReady=true;
refreshBackground();document.addEventListener('visibilitychange',()=>{if(!document.hidden){for(const job of backgroundJobs)job.next=0;refreshBackground()}});window.addEventListener('beforeunload',e=>{if(updating){e.preventDefault();e.returnValue='固件正在更新'}});
)JS";

static const char WEB_FOOT[] PROGMEM = R"HTML(
<script>
(()=>{
  const base='/assets/app.js?v=)HTML" BRIDGE_VERSION R"HTML(',label=document.getElementById('connectionText');let attempt=0;
  function show(message){if(label)label.textContent=message}
  function load(){
    ++attempt;const script=document.createElement('script');script.src=base+(attempt>1?'&retry='+attempt:'');script.async=true;
    const slow=setTimeout(()=>show('正在下载页面脚本，请稍候…'),6000);
    script.onload=()=>{clearTimeout(slow);if(!globalThis.bridgeAppReady)show('页面脚本启动失败，请刷新')};
    script.onerror=()=>{clearTimeout(slow);script.remove();if(attempt<3){show('页面脚本下载中断，正在重试…');setTimeout(load,attempt*1000)}else show('页面脚本加载失败，请刷新重试')};
    document.body.append(script);
  }
  load();
})();
</script></body></html>
)HTML";
