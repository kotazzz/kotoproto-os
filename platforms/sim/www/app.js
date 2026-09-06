const matrixCanvas = document.getElementById("matrix");
const oledCanvas = document.getElementById("oled");
const ringCanvas = document.getElementById("ring");
const matrixCtx = matrixCanvas.getContext("2d");
const oledCtx = oledCanvas.getContext("2d");
const ringCtx = ringCanvas.getContext("2d");
const logEl = document.getElementById("log");
const playBtn = document.getElementById("play");
const stickEl = document.getElementById("stick");
const knobEl = document.getElementById("knob");
const modeEl = document.getElementById("mode");
const ledEl = document.getElementById("led");

const BTN = {
  a: 1 << 0,
  b: 1 << 1,
  x: 1 << 2,
  y: 1 << 3,
  ok: 1 << 4,
  esc: 1 << 5,
  select: 1 << 6,
};

const pad = {
  mode: 0,
  x: 128,
  y: 128,
  buttons: 0,
};

function decodeBase64(b64) {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

function formatUptime(ms) {
  const total = Math.floor(ms / 1000);
  const s = String(total % 60).padStart(2, "0");
  const m = String(Math.floor(total / 60) % 60).padStart(2, "0");
  const h = String(Math.floor(total / 3600)).padStart(2, "0");
  return `${h}:${m}:${s}`;
}

function hatFrom(x, y) {
  const dx = x - 128;
  const dy = y - 128;
  const left = dx < -40;
  const right = dx > 40;
  const up = dy < -40;
  const down = dy > 40;
  if (up && right) return 1;
  if (down && right) return 3;
  if (down && left) return 5;
  if (up && left) return 7;
  if (up) return 0;
  if (right) return 2;
  if (down) return 4;
  if (left) return 6;
  return 8;
}

function quantizeAxis(value) {
  if (value < 88) return 0;
  if (value > 168) return 255;
  return 128;
}

function encodeReport() {
  let x = pad.x;
  let y = pad.y;
  if (pad.mode === 1) {
    x = quantizeAxis(x);
    y = quantizeAxis(y);
  }
  const bytes = [x, y, hatFrom(x, y), pad.buttons, pad.mode, 0];
  return bytes.map((b) => b.toString(16).padStart(2, "0")).join("");
}

let hidBusy = false;
let hidQueued = false;
let lastHidAt = 0;

async function sendHid() {
  const now = Date.now();
  if (hidBusy) {
    hidQueued = true;
    return;
  }
  const wait = 40 - (now - lastHidAt);
  if (wait > 0) {
    hidQueued = true;
    window.setTimeout(() => {
      if (hidQueued && !hidBusy) {
        hidQueued = false;
        sendHid();
      }
    }, wait);
    return;
  }
  hidBusy = true;
  lastHidAt = now;
  try {
    await fetch("/api/hid", {
      method: "POST",
      headers: { "Content-Type": "text/plain; charset=utf-8" },
      body: encodeReport(),
    });
  } finally {
    hidBusy = false;
    if (hidQueued) {
      hidQueued = false;
      sendHid();
    }
  }
}

function setKnob(x, y) {
  const dx = ((x - 128) / 128) * 32;
  const dy = ((y - 128) / 128) * 32;
  knobEl.style.transform = `translate(${dx}px, ${dy}px)`;
}

function setStickFromEvent(event) {
  const rect = stickEl.getBoundingClientRect();
  const cx = rect.left + rect.width / 2;
  const cy = rect.top + rect.height / 2;
  let dx = event.clientX - cx;
  let dy = event.clientY - cy;
  const max = rect.width / 2 - 8;
  const mag = Math.hypot(dx, dy);
  if (mag > max) {
    dx = (dx / mag) * max;
    dy = (dy / mag) * max;
  }
  pad.x = Math.round(128 + (dx / max) * 127);
  pad.y = Math.round(128 + (dy / max) * 127);
  pad.x = Math.max(0, Math.min(255, pad.x));
  pad.y = Math.max(0, Math.min(255, pad.y));
  if (pad.mode === 1) {
    pad.x = quantizeAxis(pad.x);
    pad.y = quantizeAxis(pad.y);
  }
  setKnob(pad.x, pad.y);
}

function drawMatrix(rgb, w, h) {
  const cw = matrixCanvas.width;
  const ch = matrixCanvas.height;
  const cellW = cw / w;
  const cellH = ch / h;
  matrixCtx.fillStyle = "#05060a";
  matrixCtx.fillRect(0, 0, cw, ch);
  for (let y = 0; y < h; y += 1) {
    for (let x = 0; x < w; x += 1) {
      const i = (y * w + x) * 3;
      const r = rgb[i];
      const g = rgb[i + 1];
      const b = rgb[i + 2];
      const lit = r | g | b;
      matrixCtx.fillStyle = lit ? `rgb(${r},${g},${b})` : "#14161d";
      matrixCtx.fillRect(x * cellW + 1, y * cellH + 1, Math.max(1, cellW - 2), Math.max(1, cellH - 2));
    }
  }
}

function drawOled(bits, w, h) {
  const img = oledCtx.createImageData(w, h);
  const bpr = Math.ceil(w / 8);
  for (let y = 0; y < h; y += 1) {
    for (let x = 0; x < w; x += 1) {
      const byte = bits[y * bpr + (x >> 3)];
      const on = (byte >> (7 - (x & 7))) & 1;
      const i = (y * w + x) * 4;
      if (on) {
        if (y < 16) {
          img.data[i] = 255;
          img.data[i + 1] = 210;
          img.data[i + 2] = 70;
        } else {
          img.data[i] = 120;
          img.data[i + 1] = 210;
          img.data[i + 2] = 255;
        }
        img.data[i + 3] = 255;
      } else {
        img.data[i] = 8;
        img.data[i + 1] = 10;
        img.data[i + 2] = 14;
        img.data[i + 3] = 255;
      }
    }
  }
  const tmp = document.createElement("canvas");
  tmp.width = w;
  tmp.height = h;
  tmp.getContext("2d").putImageData(img, 0, 0);
  oledCtx.imageSmoothingEnabled = false;
  oledCtx.drawImage(tmp, 0, 0, oledCanvas.width, oledCanvas.height);
}

function drawRing(rgb, n) {
  const w = ringCanvas.width;
  const h = ringCanvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const radius = Math.min(cx, cy) - 18;
  ringCtx.fillStyle = "#05060a";
  ringCtx.beginPath();
  ringCtx.arc(cx, cy, Math.min(cx, cy) - 2, 0, Math.PI * 2);
  ringCtx.fill();
  ringCtx.strokeStyle = "#1c2230";
  ringCtx.lineWidth = 6;
  ringCtx.beginPath();
  ringCtx.arc(cx, cy, radius, 0, Math.PI * 2);
  ringCtx.stroke();
  for (let i = 0; i < n; i += 1) {
    const angle = -Math.PI / 2 + (i * 2 * Math.PI) / n;
    const x = cx + Math.cos(angle) * radius;
    const y = cy + Math.sin(angle) * radius;
    const r = rgb[i * 3] || 0;
    const g = rgb[i * 3 + 1] || 0;
    const b = rgb[i * 3 + 2] || 0;
    const lit = r | g | b;
    ringCtx.beginPath();
    ringCtx.arc(x, y, 9, 0, Math.PI * 2);
    if (lit) {
      ringCtx.shadowColor = `rgb(${r},${g},${b})`;
      ringCtx.shadowBlur = 12;
      ringCtx.fillStyle = `rgb(${r},${g},${b})`;
    } else {
      ringCtx.shadowBlur = 0;
      ringCtx.fillStyle = "#161922";
    }
    ringCtx.fill();
    ringCtx.shadowBlur = 0;
  }
}

async function fetchState() {
  const res = await fetch("/api/state", { cache: "no-store" });
  if (!res.ok) {
    throw new Error("state " + res.status);
  }
  return res.json();
}

let playing = true;

async function refresh() {
  try {
    const state = await fetchState();
    const meta = document.getElementById("matrix-meta");
    meta.textContent = `${state.matrix.w}×${state.matrix.h} RGB`;
    meta.classList.remove("offline");
    document.getElementById("tick").textContent = `tick ${state.tick}`;
    document.getElementById("scene").textContent = state.scene;
    document.getElementById("text").textContent = state.face || state.text;
    if (state.transition && state.transition !== "none") {
      document.getElementById("text").textContent += ` · ${state.transition}`;
    }
    document.getElementById("bright").textContent = String(state.brightness);
    document.getElementById("uptime").textContent = formatUptime(state.uptime_ms);
    document.getElementById("pad-mode-label").textContent =
      (state.pad && state.pad.mode === "key") ? "KEY" : "GAME";
    playing = state.playing;
    playBtn.textContent = playing ? "Пауза" : "Пуск";
    ledEl.classList.toggle("on", Boolean(state.pad && (state.pad.a || state.pad.ok)));
    drawMatrix(decodeBase64(state.matrix.rgb), state.matrix.w, state.matrix.h);
    drawOled(decodeBase64(state.oled.bits), state.oled.w, state.oled.h);
    if (state.ring && state.ring.rgb) {
      drawRing(decodeBase64(state.ring.rgb), state.ring.n || 12);
    }
    logEl.textContent = (state.hid_log || []).join("\n");
    logEl.scrollTop = logEl.scrollHeight;
  } catch (err) {
    document.getElementById("matrix-meta").textContent = "нет связи";
    document.getElementById("matrix-meta").className = "offline";
  }
}

playBtn.addEventListener("click", async () => {
  await fetch(playing ? "/api/pause" : "/api/play", { method: "POST" });
  await refresh();
});

document.getElementById("step").addEventListener("click", async () => {
  await fetch("/api/pause", { method: "POST" });
  await fetch("/api/step?dt=33", { method: "POST" });
  await refresh();
});

document.getElementById("restart").addEventListener("click", async () => {
  await fetch("/api/restart", { method: "POST" });
  await fetch("/api/play", { method: "POST" });
  await refresh();
});

modeEl.addEventListener("change", async () => {
  pad.mode = modeEl.checked ? 0 : 1;
  await sendHid();
});

document.querySelectorAll("[data-btn]").forEach((btn) => {
  const bit = BTN[btn.dataset.btn];
  const down = async (event) => {
    event.preventDefault();
    btn.classList.add("held");
    pad.buttons |= bit;
    await sendHid();
  };
  const up = async (event) => {
    event.preventDefault();
    btn.classList.remove("held");
    pad.buttons &= ~bit;
    await sendHid();
  };
  btn.addEventListener("pointerdown", down);
  btn.addEventListener("pointerup", up);
  btn.addEventListener("pointerleave", () => {
    if ((pad.buttons & bit) !== 0) {
      up({ preventDefault() {} });
    }
  });
});

let dragging = false;
stickEl.addEventListener("pointerdown", async (event) => {
  dragging = true;
  stickEl.setPointerCapture(event.pointerId);
  setStickFromEvent(event);
  await sendHid();
});
stickEl.addEventListener("pointermove", async (event) => {
  if (!dragging) {
    return;
  }
  setStickFromEvent(event);
  await sendHid();
});
const releaseStick = async () => {
  if (!dragging) {
    return;
  }
  dragging = false;
  pad.x = 128;
  pad.y = 128;
  setKnob(128, 128);
  await sendHid();
};
stickEl.addEventListener("pointerup", releaseStick);
stickEl.addEventListener("pointercancel", releaseStick);

setKnob(128, 128);
setInterval(refresh, 80);
refresh();
sendHid();

const micEl = document.getElementById("mic");
const proxEl = document.getElementById("prox");
const pitchEl = document.getElementById("pitch");
const rollEl = document.getElementById("roll");
const yawEl = document.getElementById("yaw");
const micVal = document.getElementById("mic-val");
const proxVal = document.getElementById("prox-val");

const sensors = {
  mic: 0,
  prox: 0,
  pitch: 0,
  roll: 0,
  yaw: 0,
};

let snsBusy = false;
let snsQueued = false;

function sensorBody(extra) {
  const parts = [
    `mic=${sensors.mic.toFixed(3)}`,
    `prox=${sensors.prox.toFixed(3)}`,
    `pitch=${sensors.pitch.toFixed(1)}`,
    `roll=${sensors.roll.toFixed(1)}`,
    `yaw=${sensors.yaw.toFixed(1)}`,
  ];
  if (extra) {
    parts.push(extra);
  }
  return parts.join("&");
}

async function sendSensors(extra) {
  if (snsBusy) {
    snsQueued = extra || true;
    return;
  }
  snsBusy = true;
  try {
    await fetch("/api/sensors", {
      method: "POST",
      headers: { "Content-Type": "text/plain; charset=utf-8" },
      body: sensorBody(typeof extra === "string" ? extra : ""),
    });
  } finally {
    snsBusy = false;
    if (snsQueued) {
      const queued = snsQueued;
      snsQueued = false;
      sendSensors(queued === true ? "" : queued);
    }
  }
}

function syncSensorLabels() {
  micVal.textContent = `${Math.round(sensors.mic * 100)}%`;
  proxVal.textContent = `${Math.round(sensors.prox * 100)}%`;
}

function readSensorInputs() {
  sensors.mic = Number(micEl.value) / 100;
  sensors.prox = Number(proxEl.value) / 100;
  sensors.pitch = Number(pitchEl.value);
  sensors.roll = Number(rollEl.value);
  sensors.yaw = Number(yawEl.value);
  syncSensorLabels();
}

[micEl, proxEl, pitchEl, rollEl, yawEl].forEach((el) => {
  el.addEventListener("input", () => {
    readSensorInputs();
    sendSensors();
  });
});

const boopBtn = document.getElementById("boop");
boopBtn.addEventListener("pointerdown", (event) => {
  event.preventDefault();
  boopBtn.classList.add("held");
  proxEl.value = "100";
  readSensorInputs();
  sendSensors();
});
const releaseBoop = () => {
  boopBtn.classList.remove("held");
  proxEl.value = "0";
  readSensorInputs();
  sendSensors();
};
boopBtn.addEventListener("pointerup", releaseBoop);
boopBtn.addEventListener("pointerleave", () => {
  if (boopBtn.classList.contains("held")) {
    releaseBoop();
  }
});

document.getElementById("calibrate").addEventListener("click", () => {
  readSensorInputs();
  sendSensors("calibrate=1");
});

let shaking = false;
let shakeTimer = 0;
document.getElementById("shake").addEventListener("click", () => {
  shaking = !shaking;
  document.getElementById("shake").classList.toggle("held", shaking);
  if (!shaking) {
    window.clearInterval(shakeTimer);
    pitchEl.value = "0";
    rollEl.value = "0";
    readSensorInputs();
    sendSensors();
    return;
  }
  let t = 0;
  shakeTimer = window.setInterval(() => {
    t += 1;
    pitchEl.value = String(Math.round(Math.sin(t / 2) * 48));
    rollEl.value = String(Math.round(Math.cos(t / 3) * 42));
    readSensorInputs();
    sendSensors();
  }, 80);
});

