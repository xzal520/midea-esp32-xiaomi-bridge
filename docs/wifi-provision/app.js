(() => {
  'use strict';
  const SERVICE = '6d696465-6100-4000-8000-000000000001';
  const RX = '6d696465-6100-4000-8000-000000000002';
  const TX = '6d696465-6100-4000-8000-000000000003';
  const root = document.getElementById('provisionView')?.shadowRoot || document;
  const $ = id => root.getElementById(id);
  const state = {
    phase: 'idle', device: null, server: null, rx: null, tx: null,
    disconnectHandler: null, generation: 0, connectPending: false,
    retryTimer: null, requestsInFlight: 0, authBusy: false, scanBusy: false, saveBusy: false,
    restarting: false, requestId: 0, incoming: [], pending: new Map(), networks: []
  };
  const supported = () => !!(window.isSecureContext && navigator.bluetooth);
  const CONNECT_RETRIES = 3;
  const CONNECT_RETRY_MS = 1200;
  // Windows keeps BluetoothDeviceWinrt::pairing_ set until the OS pairing
  // callback runs, which can take as long as the user takes to answer the
  // prompt. Give the encrypted probe far more patience than a link retry.
  const PAIR_RETRIES = 5;
  const PAIR_RETRY_MS = 4000;
  const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
  const bytes = text => new TextEncoder().encode(text).length;
  // Blink maps CONNECT_ALREADY_IN_PROGRESS to a NetworkError, and Windows
  // returns device::BluetoothDevice::ERROR_INPROGRESS ("Another Pair Operation
  // is already in progress") while a pairing is still outstanding. GATT_NOT_PAIRED
  // means the same thing from the other side: the link exists but is not bonded.
  const connectBusy = error => error?.name === 'NetworkError' &&
    /(already in progress|not paired)/i.test(error.message || '');
  // Chrome cancels a pending connect() when disconnect() runs, so call this
  // even when gatt.connected is false. Without it the browser keeps an earlier
  // attempt alive and every retry is rejected with "Connection already in
  // progress".
  let gattConnectPending = false;
  function cancelConnect(device) {
    if (!device || (!device.gatt.connected && !gattConnectPending)) return;
    gattConnectPending = false;
    try { device.gatt.disconnect(); } catch (_) {}
  }
  const validPassword = value => bytes(value) === 0 ||
    (bytes(value) >= 8 && bytes(value) <= 63) || /^[0-9a-fA-F]{64}$/.test(value);
  const show = (message, kind = 'info') => {
    $('status').textContent = message;
    $('status').className = 'status show ' + kind;
  };
  function step(number) {
    for (let i = 1; i <= 3; i++) {
      $('step' + i).classList.toggle('active', i === number);
      $('step' + i).classList.toggle('done', i < number);
    }
  }
  function render() {
    const ready = state.phase === 'ready';
    const authenticated = state.phase === 'authenticated';
    const connected = ready || authenticated;
    const connect = $('connectButton');
    connect.hidden = connected;
    connect.disabled = !supported() || state.connectPending || state.requestsInFlight > 0 || state.phase === 'releasing';
    connect.textContent = state.phase === 'connecting' ? '正在连接…' :
      state.phase === 'releasing' ? '请稍候，正在复位蓝牙…' :
      state.connectPending || state.requestsInFlight > 0 ? '正在释放连接…' :
      state.device ? '重新连接蓝牙设备' : '连接蓝牙设备';
    $('authField').hidden = !ready;
    $('authButton').hidden = !ready;
    $('authButton').disabled = state.authBusy;
    $('authButton').textContent = state.authBusy ? '正在验证…' : '验证并继续';
    $('networkSection').hidden = !authenticated;
    $('deviceName').hidden = !connected;
    $('deviceName').textContent = state.device?.name || '设备已连接';
    $('formTitle').textContent = authenticated ? '选择 Wi‑Fi 网络' : ready ? '验证设备身份' : '连接你的设备';
    $('formSubtitle').textContent = authenticated ? '扫描附近网络，选择后输入 Wi‑Fi 密码。' :
      ready ? '输入设备网页密码，验证后开始配网。' : '打开蓝牙，搜索附近的 Midea Bridge。';
    $('networkSelect').disabled = !authenticated || state.scanBusy;
    $('scanButton').disabled = !authenticated || state.scanBusy;
    $('scanButton').textContent = state.scanBusy ? '扫描中…' : '扫描网络';
    $('saveButton').disabled = !authenticated || state.saveBusy || state.scanBusy || bytes($('ssidInput').value) < 1 ||
      bytes($('ssidInput').value) > 32 || !validPassword($('wifiPassword').value);
    $('saveButton').textContent = state.saveBusy ? '正在发送…' : '保存 Wi‑Fi 并重启设备';
    if (!state.restarting) step(authenticated ? 2 : 1);
  }
  function rejectPending(message) {
    for (const request of state.pending.values()) {
      clearTimeout(request.timer);
      request.reject(new Error(message));
    }
    state.pending.clear();
  }
  function release(message, kind = 'error') {
    ++state.generation;
    clearTimeout(state.retryTimer);
    rejectPending('蓝牙连接已断开');
    const device = state.device;
    if (device && state.disconnectHandler) {
      device.removeEventListener('gattserverdisconnected', state.disconnectHandler);
    }
    state.disconnectHandler = null;
    state.server = state.rx = state.tx = null;
    state.incoming = [];
    state.authBusy = state.scanBusy = state.saveBusy = false;
    state.phase = 'releasing';
    cancelConnect(device);
    if (message) show(message, kind);
    render();
    const generation = state.generation;
    state.retryTimer = setTimeout(() => {
      if (state.generation !== generation) return;
      state.phase = 'idle';
      render();
    }, 4000);
  }
  function ensureCurrent(generation, device) {
    if (generation !== state.generation || state.device !== device || !device.gatt.connected) {
      throw new Error('蓝牙连接已中断');
    }
  }
  function handleMessage(message) {
    if (message.type === 'status') {
      if (message.deviceName) $('deviceName').textContent = message.deviceName;
      return;
    }
    const request = state.pending.get(message.id);
    if (!request) return;
    clearTimeout(request.timer);
    state.pending.delete(message.id);
    if (message.ok === false) {
      const error = new Error(message.message || '设备操作失败');
      error.deviceReply = true;
      request.reject(error);
    } else request.resolve(message);
  }
  function receive(event) {
    const view = event.target.value;
    for (const value of new Uint8Array(view.buffer, view.byteOffset, view.byteLength)) {
      if (value === 10) {
        const incoming = state.incoming;
        state.incoming = [];
        try {
          if (incoming.length) handleMessage(JSON.parse(new TextDecoder().decode(new Uint8Array(incoming))));
        } catch (_) { show('设备返回的数据无法读取，请重新连接。', 'error'); }
      } else if (state.incoming.length < 8192) state.incoming.push(value);
      else state.incoming = [];
    }
  }
  async function request(op, payload = {}, timeout = 12000) {
    if (!state.rx || !state.device?.gatt?.connected) throw new Error('蓝牙未连接');
    if (state.requestsInFlight) throw new Error('上一条蓝牙请求尚未结束');
    state.requestsInFlight++;
    try {
    const rx = state.rx;
    const generation = state.generation;
    const id = ++state.requestId;
    const data = new TextEncoder().encode(JSON.stringify({ id, op, ...payload }) + '\n');
    if (data.length > 511) throw new Error('请求内容过长');
    let pending;
    const response = new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        state.pending.delete(id);
        reject(new Error('设备响应超时，请重新连接'));
      }, timeout);
      pending = { resolve, reject, timer };
      state.pending.set(id, pending);
    });
    // A write may still be pending when the response timeout fires.
    response.catch(() => {});
    try {
      for (let offset = 0; offset < data.length; offset += 20) {
        ensureCurrent(generation, state.device);
        const part = data.slice(offset, offset + 20);
        if (rx.writeValueWithResponse) await rx.writeValueWithResponse(part);
        else await rx.writeValue(part);
      }
    } catch (error) {
      clearTimeout(pending.timer);
      state.pending.delete(id);
      throw error;
    }
    return await response;
    } finally {
      state.requestsInFlight--;
      render();
    }
  }
  // Windows keeps a link/pairing operation alive after a failed or interrupted
  // attempt, so an immediate retry is rejected. Clear the pending algorithm,
  // wait, then try again. Only disconnect when the link is actually down: a
  // live link may be carrying an OS pairing prompt that must not be dropped.
  async function openGatt(device, generation) {
    for (let attempt = 1; ; attempt++) {
      gattConnectPending = true;
      try {
        const server = await device.gatt.connect();
        gattConnectPending = false;
        return server;
      } catch (error) {
        // Keep the flag set: a failed attempt may leave the browser-side
        // algorithm pending, and cancelConnect() must clear it.
        if (generation !== state.generation || !connectBusy(error) || attempt >= CONNECT_RETRIES) throw error;
        if (device.gatt.connected) {
          // The link survived; do not tear down an in-flight pairing.
          await sleep(CONNECT_RETRY_MS * attempt);
        } else {
          cancelConnect(device);
          await sleep(CONNECT_RETRY_MS * attempt);
        }
        if (generation !== state.generation) throw new Error('蓝牙连接已中断');
      }
    }
  }
  // A harmless request with no credentials. RX requires BLE link encryption, so
  // this write is what makes Windows raise its pairing prompt. An application
  // level error reply is expected and fine; the point is the transport.
  //
  // Do not disconnect while waiting: Windows drops the outstanding pairing when
  // the link goes away and BluetoothDeviceWinrt::pairing_ stays set, which
  // turns every later attempt into "Connection already in progress". Keep the
  // link up and rewrite the probe until the OS finishes pairing.
  async function probePairing(generation) {
    for (let attempt = 1; ; attempt++) {
      try {
        await request('pair_probe', {}, 30000);
        return;
      } catch (error) {
        if (error.deviceReply) return;
        if (generation !== state.generation) throw error;
        const gatt = state.device?.gatt;
        // The link is gone, so waiting in place cannot help; let connect()
        // restart from scratch.
        if (!gatt?.connected) throw error;
        if (!connectBusy(error) || attempt >= PAIR_RETRIES) throw error;
        show('请在系统弹窗中确认蓝牙配对；完成后会自动继续。', 'info');
        await sleep(PAIR_RETRY_MS);
        if (generation !== state.generation) throw new Error('蓝牙连接已中断');
      }
    }
  }
  async function connect() {
    if (!supported()) {
      show(!window.isSecureContext ? '请通过 HTTPS 或 localhost 打开此页面。' : '当前浏览器不支持 Web Bluetooth，请使用 Chrome 或 Edge。', 'error');
      return;
    }
    if (state.connectPending || state.requestsInFlight || state.phase === 'releasing' || state.phase !== 'idle') return;
    state.connectPending = true;
    state.phase = 'connecting';
    state.restarting = false;
    const generation = ++state.generation;
    let selectedDevice = null;
    let opened = false;
    let currentStep = '开始连接';
    render();
    try {
      currentStep = '正在请求蓝牙设备';
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: 'Midea-Bridge-' }, { namePrefix: 'Midea-S3-Bridge-' }],
        optionalServices: [SERVICE]
      });
      selectedDevice = device;
      if (generation !== state.generation) return;
      if (state.device && state.disconnectHandler) {
        state.device.removeEventListener('gattserverdisconnected', state.disconnectHandler);
      }
      state.device = device;
      state.disconnectHandler = () => {
        if (state.generation !== generation || state.device !== device) return;
        release(state.restarting ? 'Wi‑Fi 设置已保存，设备正在重启。' : '蓝牙连接已断开，请重新连接。', state.restarting ? 'success' : 'error');
      };
      device.addEventListener('gattserverdisconnected', state.disconnectHandler);
      currentStep = '正在建立 GATT 连接 (device.gatt.connect)';
      show(currentStep + '…');
      const server = await openGatt(device, generation);
      opened = true;
      ensureCurrent(generation, device);
      currentStep = '正在获取主服务 (getPrimaryService)';
      show(currentStep + '…');
      const service = await server.getPrimaryService(SERVICE);
      ensureCurrent(generation, device);
      currentStep = '正在获取特征值与订阅';
      show(currentStep + '…');
      const rx = await service.getCharacteristic(RX);
      const tx = await service.getCharacteristic(TX);
      ensureCurrent(generation, device);
      tx.addEventListener('characteristicvaluechanged', receive);
      await tx.startNotifications();
      ensureCurrent(generation, device);
      state.server = server;
      state.rx = rx;
      state.tx = tx;
      currentStep = '正在建立安全加密连接 (pair_probe)';
      show('正在建立安全加密连接；如系统弹出配对请求，请确认。');
      await probePairing(generation);
      ensureCurrent(generation, device);
      state.phase = 'ready';
      show('蓝牙已连接。请输入设备网页登录密码。', 'success');
    } catch (error) {
      if (generation !== state.generation) return;
      const detail = '[' + (error.name || 'Error') + ': ' + (error.message || '未知异常') + '] (在【' + currentStep + '】阶段)';
      if (error.name === 'NotFoundError') {
        state.phase = 'idle';
        show('没有选择设备。请确认设备已上电并开启蓝牙。');
      } else if (connectBusy(error)) {
        release('蓝牙连接未完成：系统仍有一个蓝牙配对或连接未结束。' + detail, 'error');
      } else {
        release('蓝牙连接未完成：' + detail, 'error');
      }
    } finally {
      // A connect promise can settle after a disconnect event. Also cancel it
      // when it never reached the connected state, otherwise the browser keeps
      // the algorithm pending and the next attempt fails immediately.
      if (!opened || generation !== state.generation) cancelConnect(selectedDevice);
      state.connectPending = false;
      if (state.phase === 'connecting' && generation === state.generation) state.phase = 'idle';
      render();
    }
  }
  async function authenticate() {
    if (state.phase !== 'ready' || state.authBusy) return;
    const password = $('devicePassword').value;
    if (!password) { show('请输入设备网页登录密码。', 'error'); return; }
    state.authBusy = true;
    render();
    try {
      await request('auth', { password });
      if (state.phase !== 'ready') return;
      state.phase = 'authenticated';
      show('身份验证成功。正在扫描附近的 Wi‑Fi…', 'success');
      render();
      void scan();
    } catch (error) {
      if (error.deviceReply) show(error.message, 'error');
      else if (state.phase !== 'releasing' && state.phase !== 'idle') {
        release('加密蓝牙写入失败：' + (error.message || '请重新连接'), 'error');
      }
    } finally { state.authBusy = false; render(); }
  }
  function renderNetworks(networks) {
    state.networks = networks;
    const select = $('networkSelect');
    select.replaceChildren();
    select.add(new Option(networks.length ? '选择一个 Wi‑Fi 网络' : '未发现网络 · 可手动输入', ''));
    networks.forEach((network, index) => {
      const signal = network.rssi >= -60 ? '信号强' : network.rssi >= -75 ? '信号中等' : '信号较弱';
      select.add(new Option(network.ssid + ' · ' + (network.secure ? '需密码' : '开放网络') + ' · ' + signal, String(index + 1)));
    });
    select.add(new Option('手动输入隐藏网络…', 'manual'));
  }
  async function scan() {
    if (state.phase !== 'authenticated' || state.scanBusy) return;
    state.scanBusy = true;
    render();
    try {
      const response = await request('wifi_scan', {}, 30000);
      const networks = Array.isArray(response.networks) ? response.networks : [];
      renderNetworks(networks);
      show(networks.length ? '扫描完成，共找到 ' + networks.length + ' 个网络。' : '没有找到广播名称的 Wi‑Fi，可手动填写 SSID。', networks.length ? 'success' : 'info');
    } catch (error) {
      if (error.deviceReply) show(error.message + '；旧固件可手动输入 SSID。', 'error');
      else if (state.phase === 'authenticated') release('扫描失败：' + error.message, 'error');
    } finally { state.scanBusy = false; render(); }
  }
  async function save() {
    if (state.phase !== 'authenticated' || state.saveBusy) return;
    const ssid = $('ssidInput').value, password = $('wifiPassword').value;
    if (bytes(ssid) < 1 || bytes(ssid) > 32) { show('SSID 长度须为 1–32 个 UTF‑8 字节。', 'error'); return; }
    if (!validPassword(password)) { show('Wi‑Fi 密码须为空、8–63 个 UTF‑8 字节，或 64 位十六进制 PSK。', 'error'); return; }
    if (!confirm('将 Wi‑Fi「' + ssid + '」保存到设备并重启？')) return;
    state.saveBusy = true;
    render();
    try {
      const result = await request('wifi', { ssid, password }, 15000);
      state.restarting = true;
      step(3);
      show(result.message || '设置已保存。设备即将重启，并连接所选 Wi‑Fi。', 'success');
      $('wifiPassword').value = '';
      $('devicePassword').value = '';
    } catch (error) {
      if (error.deviceReply) show(error.message, 'error');
      else if (state.phase === 'authenticated') release('发送 Wi‑Fi 设置失败：' + error.message, 'error');
    } finally { state.saveBusy = false; render(); }
  }
  $('connectButton').addEventListener('click', connect);
  $('authButton').addEventListener('click', authenticate);
  $('scanButton').addEventListener('click', scan);
  $('saveButton').addEventListener('click', save);
  $('ssidInput').addEventListener('input', render);
  $('wifiPassword').addEventListener('input', render);
  $('networkSelect').addEventListener('change', () => {
    const value = $('networkSelect').value;
    if (value === 'manual') $('ssidInput').value = '';
    else if (value) $('ssidInput').value = state.networks[Number(value) - 1].ssid;
    render();
  });
  root.querySelectorAll('[data-toggle]').forEach(button => button.addEventListener('click', () => {
    const input = $(button.dataset.toggle);
    input.type = input.type === 'password' ? 'text' : 'password';
    button.textContent = input.type === 'password' ? '显示' : '隐藏';
  }));
  if (!supported()) show(!window.isSecureContext ? '此网页需要通过 HTTPS 或 localhost 打开。' : '浏览器不支持 Web Bluetooth。请使用 Chrome 或 Edge。');
  render();
})();
