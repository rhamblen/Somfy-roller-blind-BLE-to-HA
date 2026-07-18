"use strict";
// Somfy BLE Hub web UI — vanilla SPA. Talks to the firmware JSON API + /ws/logs.
// Pattern carried over from the Shutter Hub's web UI (docs/decisions/0001-framework-reuse.md).

const $  = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];

async function apiGet(u) { const r = await fetch(u); if (!r.ok) throw new Error("HTTP " + r.status); return r.json(); }
async function apiPost(u, obj) {
  const body = obj ? new URLSearchParams(obj) : undefined;
  const r = await fetch(u, { method: "POST",
    headers: obj ? { "Content-Type": "application/x-www-form-urlencoded" } : {}, body });
  let j = {}; try { j = await r.json(); } catch (e) {}
  if (!r.ok) throw new Error(j.error || ("HTTP " + r.status));
  return j;
}
const fmtKB = b => (b / 1024).toFixed(1) + " KB";
const rssiTxt = r => r + " dBm" + (r >= -60 ? " (good)" : r >= -75 ? " (ok)" : " (poor)");
const esc = s => String(s).replace(/[&<>]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]));

// ---- Routing ----------------------------------------------------------------
const routes = ["info", "motors", "mqtt", "system", "ota", "logs"];
function go(route) {
  if (!routes.includes(route)) route = "info";
  $$(".nav-item").forEach(n => n.classList.toggle("active", n.dataset.route === route));
  $$(".page").forEach(p => p.classList.toggle("hidden", p.id !== "page-" + route));
  location.hash = route;
  if (route === "info")   loadInfo();
  if (route === "motors") loadMotors();
  if (route === "mqtt")   loadMqtt();
  if (route === "system") loadSystem();
  if (route === "ota")    loadOta();
  if (route === "logs")   logsOnShow();
}
$$(".nav-item").forEach(n => n.addEventListener("click", () => go(n.dataset.route)));
window.addEventListener("hashchange", () => go(location.hash.slice(1)));

// Generic sub-tab handler (System)
$$(".tabs").forEach(bar => {
  bar.addEventListener("click", e => {
    const b = e.target.closest(".tab"); if (!b) return;
    const scope = bar.parentElement;
    $$(".tab", bar).forEach(t => t.classList.toggle("active", t === b));
    $$(".tabpane", scope).forEach(p => p.classList.toggle("hidden", p.dataset.pane !== b.dataset.tab));
  });
});

// ---- Info ---------------------------------------------------------------------
// HomeKit reads as one of four states, matching what System > HomeKit shows: off,
// configured-but-not-yet-booted, bridge up and pairable, or paired.
function hkStateText(h) {
  if (!h) return "—";
  if (!h.enabled) return "Disabled";
  if (!h.running) return "Enabled — reboot to start";
  return h.paired ? `Paired — ${h.controllers} controller(s)` : "Active — not paired";
}
function renderMotorsTable(motors, tbodyId, emptyId) {
  const tbody = $(tbodyId), empty = $(emptyId);
  tbody.innerHTML = "";
  if (!motors || !motors.length) { empty.classList.remove("hidden"); return; }
  empty.classList.add("hidden");
  motors.forEach(m => {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${esc(m.name)}</td><td class="mono">${esc(m.mac || "—")}</td>` +
      `<td>${m.calibrated ? "calibrated" : "not calibrated"}</td>`;
    tbody.appendChild(tr);
  });
}

let infoTimer = null;
async function loadInfo() {
  try {
    const d = await apiGet("/api/info");
    $("#brandName").textContent = d.device;
    $("#sideFw").textContent = "v" + d.fw;
    $("#brandState").textContent = d.wifi.connected ? "Online" : "Offline";
    $("#brandDot").classList.toggle("off", !d.wifi.connected);
    $("#i_fw").textContent = "v" + d.fw;
    $("#i_dev").textContent = d.device;
    $("#i_host").textContent = d.host;
    $("#i_chip").textContent = d.chip;
    $("#i_up").textContent = d.uptime;
    $("#i_boot").textContent = d.boot_count;
    $("#i_reset").textContent = d.reset_reason;
    $("#i_ssid").textContent = d.wifi.ssid || "—";
    $("#i_ip").textContent = d.wifi.ip;
    $("#i_mac").textContent = d.wifi.mac;
    $("#i_rssi").textContent = rssiTxt(d.wifi.rssi);
    $("#i_heap").textContent = fmtKB(d.free_heap);
    $("#i_mqtt").textContent = d.mqtt.enabled ? (d.mqtt.connected ? "Connected" : d.mqtt.state) : "Disabled";
    $("#i_hk").textContent = hkStateText(d.homekit);
    renderMotorsTable(d.motors, "#i_motors", "#i_motors_empty");
  } catch (e) {}
  clearTimeout(infoTimer);
  if ($("#page-info").offsetParent !== null) infoTimer = setTimeout(loadInfo, 5000);
}

// ---- Motors: add/remove + BLE bench-test (Phase 1: goto/stop/identify) --------
async function loadMotors() {
  try {
    const motors = await apiGet("/api/motors");
    const list = $("#mo_list"), empty = $("#mo_empty");
    list.innerHTML = "";
    if (!motors.length) { empty.classList.remove("hidden"); return; }
    empty.classList.add("hidden");
    motors.forEach(m => {
      const row = document.createElement("div"); row.className = "mo-row";
      row.innerHTML = `
        <div class="mo-head">
          <span class="mo-name">${esc(m.name)} <span class="mono muted">${esc(m.mac || "")}</span></span>
          <span class="muted">${m.calibrated ? "calibrated" : "not calibrated"}</span>
        </div>
        <div class="mo-actions">
          <button class="btn ghost sm" data-act="identify">Identify</button>
          <input type="number" min="0" max="32767" placeholder="0-32767" data-role="pos">
          <button class="btn ghost sm" data-act="goto">Go</button>
          <button class="btn danger sm" data-act="stop">Stop</button>
          <button class="btn ghost sm" data-act="remove">Remove</button>
        </div>
        <p class="mo-msg"></p>`;
      const msg = row.querySelector(".mo-msg");
      const posInput = row.querySelector('[data-role="pos"]');
      row.querySelectorAll("[data-act]").forEach(btn => btn.addEventListener("click", async () => {
        const act = btn.dataset.act;
        if (act === "remove") {
          if (!confirm(`Remove "${m.name}"?`)) return;
          try { await apiPost("/api/motors/remove", { id: m.id }); loadMotors(); }
          catch (e) { msg.textContent = "Failed: " + e.message; }
          return;
        }
        row.querySelectorAll("[data-act]").forEach(b => b.disabled = true);
        msg.textContent = act === "identify" ? "Connecting… authenticating… identify sent…"
          : act === "stop" ? "Connecting… authenticating… stop sent…"
          : "Connecting… authenticating… goto sent…";
        try {
          const body = { id: m.id };
          if (act === "goto") body.pos = posInput.value || "0";
          await apiPost("/api/motors/" + act, body);
          msg.textContent = "OK — " + act + " succeeded.";
        } catch (e) { msg.textContent = "Failed: " + e.message; }
        row.querySelectorAll("[data-act]").forEach(b => b.disabled = false);
      }));
      list.appendChild(row);
    });
  } catch (e) {}
}
$("#mo_add").addEventListener("click", async () => {
  const name = $("#mo_name").value.trim(), mac = $("#mo_mac").value.trim(), pin = $("#mo_pin").value.trim();
  if (!mac || !pin) { $("#mo_addmsg").textContent = "MAC and PIN are required."; return; }
  $("#mo_addmsg").textContent = "Adding…";
  try {
    await apiPost("/api/motors/add", { name, mac, pin });
    $("#mo_name").value = ""; $("#mo_mac").value = ""; $("#mo_pin").value = "";
    $("#mo_addmsg").textContent = "Added.";
    loadMotors();
  } catch (e) { $("#mo_addmsg").textContent = "Failed: " + e.message; }
});

// ---- MQTT -----------------------------------------------------------------------
function mqttPill(d) {
  const p = $("#mq_pill"), dot = p.querySelector(".dot"), txt = p.querySelector("span:last-child");
  const state = !d.enabled ? "disabled" : d.connected ? "connected" : d.state;
  txt.textContent = state;
  dot.style.background = d.connected ? "var(--green)" : d.enabled ? "var(--amber)" : "var(--muted)";
}
async function loadMqtt() {
  try {
    const d = await apiGet("/api/mqtt");
    $("#mq_en").checked = d.enabled; $("#mq_ha").checked = d.haDiscovery;
    $("#mq_host").value = d.host; $("#mq_port").value = d.port;
    $("#mq_client").value = d.clientId; $("#mq_user").value = d.user;
    $("#mq_pass").value = ""; $("#mq_pass").placeholder = d.hasPass ? "•••••• (unchanged)" : "(none set)";
    $("#mq_base").value = d.base;
    mqttPill(d);
  } catch (e) { $("#mq_msg").textContent = "Load failed: " + e.message; }
}
$("#mq_save").addEventListener("click", async () => {
  $("#mq_msg").textContent = "Saving…";
  try {
    const d = await apiPost("/api/mqtt", {
      enabled: $("#mq_en").checked, host: $("#mq_host").value.trim(), port: $("#mq_port").value,
      clientId: $("#mq_client").value.trim(), user: $("#mq_user").value.trim(),
      pass: $("#mq_pass").value, base: $("#mq_base").value.trim(), haDiscovery: $("#mq_ha").checked,
    });
    $("#mq_pass").value = "";
    mqttPill(d);
    $("#mq_msg").textContent = "Saved.";
  } catch (e) { $("#mq_msg").textContent = "Failed: " + e.message; }
});

// ---- System -----------------------------------------------------------------
async function loadSystem() {
  try {
    const d = await apiGet("/api/info");
    $("#sy_dev").textContent = d.device; $("#sy_host").textContent = d.host;
    $("#sy_fw").textContent = "v" + d.fw;
    $("#wf_cur").textContent = d.wifi.ssid || "—"; $("#wf_ip").textContent = d.wifi.ip;
  } catch (e) {}
  try {
    const a = await apiGet("/api/auth");
    $("#se_en").checked = a.enabled; $("#se_user").value = a.user;
  } catch (e) {}
  loadHomekit();
}
function qa(id, url, confirmMsg) {
  $(id).addEventListener("click", async () => {
    if (confirmMsg && !confirm(confirmMsg)) return;
    $("#qa_msg").textContent = "Working…";
    try { const j = await apiPost(url); $("#qa_msg").textContent = j.msg || "OK"; }
    catch (e) { $("#qa_msg").textContent = "Failed: " + e.message; }
  });
}
qa("#qa_reboot", "/api/system/reboot", "Reboot the hub now?");
qa("#qa_wifi", "/api/system/reset-wifi",
   "RESET WIFI: forget the saved network and restart into Somfy-BLE-Setup? You'll reconnect the hub afterwards.");
qa("#qa_cfg", "/api/system/reset-config",
   "RESET CONFIG: clear device name, MQTT and web-auth settings (WiFi and paired motors are kept) and reboot?");

$("#wf_scan").addEventListener("click", scanWifi);
async function scanWifi() {
  const sel = $("#wf_sel"); $("#wf_msg").textContent = "Scanning…"; $("#wf_scan").disabled = true;
  try {
    let r = await fetch("/api/wifi/scan"); let j = await r.json();
    while (r.status === 202 || j.scanning) { await new Promise(s => setTimeout(s, 1500));
      r = await fetch("/api/wifi/scan"); j = await r.json(); }
    sel.innerHTML = '<option value="">— select network —</option>';
    j.sort((a, b) => b.rssi - a.rssi).forEach(n => {
      const o = document.createElement("option");
      o.value = n.ssid; o.textContent = `${n.ssid}${n.lock ? " 🔒" : ""} (${n.rssi}dBm)`; sel.appendChild(o);
    });
    $("#wf_msg").textContent = j.length + " network(s) found.";
  } catch (e) { $("#wf_msg").textContent = "Scan failed: " + e.message; }
  $("#wf_scan").disabled = false;
}
$("#wf_connect").addEventListener("click", async () => {
  const ssid = $("#wf_sel").value;
  if (!ssid) { $("#wf_msg").textContent = "Choose a network first."; return; }
  if (!confirm(`Switch the hub to "${ssid}"?`)) return;
  $("#wf_msg").textContent = "Connecting…";
  try { const j = await apiPost("/api/wifi/connect", { ssid, pass: $("#wf_pass").value });
    $("#wf_msg").textContent = j.msg || "Connecting…"; }
  catch (e) { $("#wf_msg").textContent = "The hub may have switched networks — reconnect at its .local name."; }
});
$("#se_save").addEventListener("click", async () => {
  $("#se_msg").textContent = "Saving…";
  try { await apiPost("/api/auth",
      { enabled: $("#se_en").checked, user: $("#se_user").value.trim(), pass: $("#se_pass").value });
    $("#se_pass").value = "";
    $("#se_msg").textContent = "Saved. If enabled, the browser will ask for login on the next request.";
  } catch (e) { $("#se_msg").textContent = "Failed: " + e.message; }
});

// ---- HomeKit (System > HomeKit sub-tab) --------------------------------------
// Config is stored regardless; the HomeSpan bridge itself only starts at boot (the API
// reports `running:false` until then, which drives the status text below).
let hkStat = { running: false, paired: false };
const hkDigits = v => v.replace(/\D/g, "").slice(0, 8);
const hkFmt = c => c.length === 8 ? `${c.slice(0, 3)}-${c.slice(3, 5)}-${c.slice(5)}` : c;
// Apple X-HM:// pairing payload: category 2 (bridge) + IP flag + the code, base-36, then
// the 4-char setup ID. "SOMF" must match HomeKit.cpp's homeSpan.setQRID().
function hkQrUri(code) {
  const v = 2 * 2 ** 31 + 2 * 2 ** 27 + parseInt(code, 10);
  return "X-HM://" + v.toString(36).toUpperCase().padStart(9, "0") + "SOMF";
}
let hkLastUri = "";
function hkRenderQr() {
  const code = hkDigits($("#hk_code").value);
  const show = $("#hk_en").checked && !hkStat.paired && code.length === 8 && typeof QRCode !== "undefined";
  $("#hk_qrbox").classList.toggle("hidden", !show);
  if (!show) { hkLastUri = ""; return; }
  const uri = hkQrUri(code);
  $("#hk_qrcap").innerHTML = (hkStat.running
    ? "Scan with the iPhone camera or the Home app to pair"
    : "Pairing QR preview — enable HomeKit and reboot to pair")
    + `<br><b>${hkFmt(code)}</b>`;
  if (uri === hkLastUri) return;
  hkLastUri = uri;
  $("#hk_qr").innerHTML = "";
  new QRCode($("#hk_qr"), { text: uri, width: 148, height: 148,
    colorDark: "#0b0d13", colorLight: "#ffffff", correctLevel: QRCode.CorrectLevel.Q });
}
async function loadHomekit() {
  try {
    const d = await apiGet("/api/homekit");
    hkStat = d;
    $("#hk_en").checked = d.enabled;
    $("#hk_name").value = d.name;
    $("#hk_code").value = hkFmt(d.code);
    $("#hk_run").textContent = d.running ? "Running"
      : d.enabled ? "Enabled — reboot to start the bridge" : "Disabled";
    $("#hk_pair").textContent = d.running
      ? (d.paired ? `Paired — ${d.controllers} controller(s)` : "Not paired — scan the QR to pair") : "—";
    $("#hk_reset").disabled = !d.running;
    hkRenderQr();
  } catch (e) { $("#hk_msg").textContent = "Load failed: " + e.message; }
}
$("#hk_code").addEventListener("input", () => {
  $("#hk_code").value = hkFmt(hkDigits($("#hk_code").value)); hkRenderQr(); });
$("#hk_en").addEventListener("change", hkRenderQr);
$("#hk_rand").addEventListener("click", () => {
  let c;
  do { c = String(Math.floor(Math.random() * 1e8)).padStart(8, "0"); }
  while (/^(\d)\1{7}$/.test(c) || c === "12345678" || c === "87654321");
  $("#hk_code").value = hkFmt(c); hkRenderQr();
});
$("#hk_save").addEventListener("click", async () => {
  const code = hkDigits($("#hk_code").value);
  if (code.length !== 8) { $("#hk_msg").textContent = "Setup code must be 8 digits."; return; }
  $("#hk_msg").textContent = "Saving…";
  try {
    const d = await apiPost("/api/homekit",
      { enabled: $("#hk_en").checked, name: $("#hk_name").value.trim(), code });
    hkStat = d;
    // Every HomeKit change is applied at boot, so a save alone never reaches the running
    // bridge — steer the user to reboot before they try to pair.
    $("#hk_msg").textContent = "Saved — now click “Reboot to apply” so the bridge picks up the change.";
    $("#hk_reboot").classList.add("primary"); $("#hk_reboot").classList.remove("ghost");
    hkRenderQr();
  } catch (e) { $("#hk_msg").textContent = "Failed: " + e.message; }
});
$("#hk_reboot").addEventListener("click", async () => {
  if (!confirm("Reboot the hub now?")) return;
  $("#hk_msg").textContent = "Rebooting — reconnect in ~15s…";
  try { await apiPost("/api/system/reboot"); } catch (e) {}
});
$("#hk_reset").addEventListener("click", async () => {
  if (!confirm("RESET HOMEKIT PAIRINGS: erase all paired controllers and reboot? You'll need to re-pair from the Home app.")) return;
  $("#hk_msg").textContent = "Resetting…";
  try { const j = await apiPost("/api/homekit/reset-pairings");
    $("#hk_msg").textContent = j.msg || "Reset — rebooting…"; }
  catch (e) { $("#hk_msg").textContent = "Failed: " + e.message; }
});

// ---- OTA --------------------------------------------------------------------
async function loadOta() {
  try {
    const d = await apiGet("/api/info");
    $("#o_fw").textContent = "v" + d.fw; $("#o_chip").textContent = d.chip;
    $("#o_heap").textContent = fmtKB(d.free_heap);
    const lf = d.last_flash;
    $("#o_last").textContent = lf.type === "none" ? "none yet" : `${lf.type} — ${lf.ok ? "OK" : "FAILED"}`;
  } catch (e) {}
}
function otaLog(msg) {
  const l = $("#o_log"); if (l.querySelector(".empty")) l.textContent = "";
  const t = new Date().toLocaleTimeString();
  l.textContent += `[${t}] ${msg}\n`; l.scrollTop = l.scrollHeight;
}
function otaUpload(target, file) {
  return new Promise((res, rej) => {
    const x = new XMLHttpRequest(); x.open("POST", "/api/ota?target=" + target);
    x.upload.onprogress = e => { if (e.lengthComputable) $("#o_prog").value = Math.round(100 * e.loaded / e.total); };
    x.onload = () => { $("#o_prog").value = 0; let r = {}; try { r = JSON.parse(x.responseText); } catch (e) {}
      (x.status === 200 && r.ok) ? res() : rej(r.error || ("HTTP " + x.status)); };
    x.onerror = () => rej("network error");
    const fd = new FormData(); fd.append("file", file); x.send(fd);
  });
}
$("#o_fwup").addEventListener("click", async () => {
  const f = $("#o_fwfile").files[0]; if (!f) { otaLog("Choose a firmware .bin first."); return; }
  $("#o_fwup").disabled = true; otaLog("Uploading firmware " + f.name + "…");
  try { await otaUpload("firmware", f);
    otaLog("Firmware flashed. Flash a filesystem too if needed, then click Reboot to run it.");
    $("#o_reboot").classList.add("primary"); $("#o_reboot").classList.remove("ghost");
  } catch (e) { otaLog("Firmware failed: " + e); } $("#o_fwup").disabled = false;
});
$("#o_fsup").addEventListener("click", async () => {
  const f = $("#o_fsfile").files[0]; if (!f) { otaLog("Choose a LittleFS .bin first."); return; }
  $("#o_fsup").disabled = true; otaLog("Uploading filesystem " + f.name + "…");
  try { await otaUpload("filesystem", f);
    otaLog("Filesystem flashed. Click Reboot to apply.");
    $("#o_reboot").classList.add("primary"); $("#o_reboot").classList.remove("ghost");
  } catch (e) { otaLog("Filesystem failed: " + e); } $("#o_fsup").disabled = false;
});
$("#o_reboot").addEventListener("click", async () => {
  if (!confirm("Reboot the hub now?")) return;
  otaLog("Rebooting — reconnect in ~15s…");
  try { await apiPost("/api/system/reboot"); } catch (e) {}
});

// ---- Logs -------------------------------------------------------------------
const RANK = { V: 0, D: 1, I: 2, W: 3, E: 4 };
let logBuf = [], logWs = null, chipOn = { E: 1, W: 1, I: 1, D: 1, V: 1 }, minLvl = "I", search = "";
const LOGCAP = 600;

function passes(l) {
  return chipOn[l.lvl] && RANK[l.lvl] >= RANK[minLvl] &&
    (!search || (l.tag + " " + l.msg).toLowerCase().includes(search));
}
function lineNode(l) {
  const secs = (l.t / 1000).toFixed(3).padStart(9, " ");
  const div = document.createElement("div"); div.className = "logline " + l.lvl;
  div.innerHTML = `<span class="t">[${secs}]</span><span class="lv">${l.lvl}</span>` +
    `<span class="tg">${esc(l.tag)}:</span><span class="mg">${esc(l.msg)}</span>`;
  return div;
}
function renderLogs() {
  const v = $("#lg_view"); v.innerHTML = "";
  const shown = logBuf.filter(passes);
  if (!shown.length) { v.innerHTML = '<div class="empty">No logs to display</div>'; }
  else shown.forEach(l => v.appendChild(lineNode(l)));
  $("#lg_count").textContent = logBuf.length;
  if ($("#lg_auto").checked) v.scrollTop = v.scrollHeight;
}
function addLine(l) {
  logBuf.push(l); if (logBuf.length > LOGCAP) logBuf.shift();
  if (!passes(l)) { $("#lg_count").textContent = logBuf.length; return; }
  const v = $("#lg_view"); const em = v.querySelector(".empty"); if (em) em.remove();
  v.appendChild(lineNode(l)); $("#lg_count").textContent = logBuf.length;
  if ($("#lg_auto").checked) v.scrollTop = v.scrollHeight;
}
function connectLogs() {
  if (logWs && (logWs.readyState === 0 || logWs.readyState === 1)) return;
  const proto = location.protocol === "https:" ? "wss" : "ws";
  logWs = new WebSocket(`${proto}://${location.host}/ws/logs`);
  $("#lg_ws").textContent = "connecting…";
  logWs.onopen = () => $("#lg_ws").textContent = "● live";
  logWs.onclose = () => { $("#lg_ws").textContent = "○ disconnected"; setTimeout(connectLogs, 3000); };
  logWs.onmessage = ev => {
    let d; try { d = JSON.parse(ev.data); } catch (e) { return; }
    if (Array.isArray(d)) { logBuf = d.slice(-LOGCAP); renderLogs(); } else addLine(d);
  };
}
function logsOnShow() { connectLogs(); renderLogs(); }
$$("#lg_chips .chip").forEach(c => c.addEventListener("click", () => {
  chipOn[c.dataset.lvl] = chipOn[c.dataset.lvl] ? 0 : 1; c.classList.toggle("on"); renderLogs(); }));
$("#lg_level").addEventListener("change", e => { minLvl = e.target.value; renderLogs(); });
$("#lg_search").addEventListener("input", e => { search = e.target.value.toLowerCase().trim(); renderLogs(); });
$("#lg_auto").addEventListener("change", renderLogs);
$("#lg_clear").addEventListener("click", () => { logBuf = []; renderLogs(); });
$("#lg_export").addEventListener("click", () => {
  const txt = logBuf.map(l => `[${(l.t / 1000).toFixed(3)}] ${l.lvl}/${l.tag}: ${l.msg}`).join("\n");
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([txt], { type: "text/plain" }));
  a.download = "somfy-ble-logs.txt"; a.click(); URL.revokeObjectURL(a.href);
});

// ---- Boot -------------------------------------------------------------------
connectLogs();                    // keep the log buffer warm regardless of page
go(location.hash.slice(1) || "info");
