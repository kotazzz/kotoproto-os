const left = document.getElementById("left");
const right = document.getElementById("right");
const leftCtx = left.getContext("2d");
const rightCtx = right.getContext("2d");
const lists = document.getElementById("lists");
const meta = document.getElementById("meta");

let catalog = [];
let currentId = "";

function decodeBase64(b64) {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

function drawMatrix(ctx, canvas, rgb, w, h) {
  const cw = canvas.width;
  const ch = canvas.height;
  const cellW = cw / w;
  const cellH = ch / h;
  ctx.fillStyle = "#05060a";
  ctx.fillRect(0, 0, cw, ch);
  for (let y = 0; y < h; y += 1) {
    for (let x = 0; x < w; x += 1) {
      const i = (y * w + x) * 3;
      const r = rgb[i];
      const g = rgb[i + 1];
      const b = rgb[i + 2];
      const lit = r | g | b;
      ctx.fillStyle = lit ? `rgb(${r},${g},${b})` : "#14161d";
      ctx.fillRect(x * cellW + 1, y * cellH + 1, Math.max(1, cellW - 2), Math.max(1, cellH - 2));
    }
  }
}

function drawThumb(canvas, rgb, w, h) {
  const ctx = canvas.getContext("2d");
  const img = ctx.createImageData(w, h);
  for (let i = 0; i < w * h; i += 1) {
    img.data[i * 4] = rgb[i * 3] || 0;
    img.data[i * 4 + 1] = rgb[i * 3 + 1] || 0;
    img.data[i * 4 + 2] = rgb[i * 3 + 2] || 0;
    img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function byKind(kind) {
  return catalog.filter((e) => e.kind === kind);
}

function renderLists() {
  lists.innerHTML = "";
  for (const kind of ["classic", "special"]) {
    const wrap = document.createElement("div");
    wrap.className = "emo-group";
    const title = document.createElement("h3");
    title.textContent = kind === "classic" ? "Classic" : "Special";
    wrap.appendChild(title);
    const grid = document.createElement("div");
    grid.className = "emo-grid";
    for (const emo of byKind(kind)) {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = "emo-card" + (emo.id === currentId ? " active" : "");
      const canvas = document.createElement("canvas");
      canvas.width = 64;
      canvas.height = 32;
      if (emo.preview) {
        drawThumb(canvas, decodeBase64(emo.preview), 64, 32);
      }
      const label = document.createElement("span");
      label.textContent = emo.short || emo.id;
      btn.appendChild(canvas);
      btn.appendChild(label);
      btn.title = `${emo.id} · ${emo.effect} · ${emo.transition}`;
      btn.addEventListener("click", () => selectFace(emo.id, true));
      grid.appendChild(btn);
    }
    wrap.appendChild(grid);
    lists.appendChild(wrap);
  }
}

function describe(emo) {
  if (!emo) {
    return "";
  }
  const overlays = emo.kind === "classic" ? "рот и blink включены" : "оверлеи выкл";
  return `${emo.id} · ${emo.kind} · effect ${emo.effect} · ${emo.transition} · ${overlays}`;
}

function syncOverlayButtons(emo) {
  const blink = document.getElementById("blink");
  const boop = document.getElementById("boop");
  const mouth = document.getElementById("mouth");
  const replay = document.getElementById("replay");
  const classic = Boolean(emo && emo.kind === "classic");
  blink.disabled = !classic || (emo && !emo.allow_blink);
  boop.disabled = Boolean(emo && !emo.allow_boop);
  mouth.disabled = !classic;
  replay.disabled = !currentId;
}

async function selectFace(id, withTransition) {
  currentId = id;
  const emo = catalog.find((e) => e.id === id);
  meta.textContent = describe(emo);
  syncOverlayButtons(emo);
  renderLists();
  await fetch("/api/face", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: `id=${encodeURIComponent(id)}&transition=${withTransition ? "1" : "0"}`,
  });
}

async function refresh() {
  const res = await fetch("/api/state", { cache: "no-store" });
  if (!res.ok) {
    return;
  }
  const state = await res.json();
  if (state.face && state.face !== currentId) {
    currentId = state.face;
    const emo = catalog.find((e) => e.id === currentId);
    meta.textContent = describe(emo);
    syncOverlayButtons(emo);
    renderLists();
  }
  if (state.matrix) {
    const w = state.matrix.w;
    const h = state.matrix.h;
    if (state.matrix.left) {
      drawMatrix(leftCtx, left, decodeBase64(state.matrix.left), w, h);
    }
    if (state.matrix.right) {
      drawMatrix(rightCtx, right, decodeBase64(state.matrix.right), w, h);
    }
  }
}

async function sendMic(level, prox) {
  await fetch("/api/sensors", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: `mic=${level.toFixed(3)}&prox=${prox}`,
  });
}

document.getElementById("blink").addEventListener("click", async () => {
  await fetch("/api/preview", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: "blink=1",
  });
});

const boopBtn = document.getElementById("boop");
boopBtn.addEventListener("pointerdown", async (event) => {
  event.preventDefault();
  await sendMic(0, 1);
});
const releaseBoop = async () => {
  await sendMic(0, 0);
};
boopBtn.addEventListener("pointerup", releaseBoop);
boopBtn.addEventListener("pointerleave", releaseBoop);

const mouthBtn = document.getElementById("mouth");
mouthBtn.addEventListener("pointerdown", async (event) => {
  event.preventDefault();
  await sendMic(1, 0);
});
const releaseMouth = async () => {
  await sendMic(0, 0);
};
mouthBtn.addEventListener("pointerup", releaseMouth);
mouthBtn.addEventListener("pointerleave", releaseMouth);

document.getElementById("replay").addEventListener("pointerdown", (event) => {
  event.preventDefault();
  if (currentId) {
    selectFace(currentId, true);
  }
});

async function boot() {
  const res = await fetch("/api/emotions", { cache: "no-store" });
  const data = await res.json();
  catalog = data.emotions || [];
  const state = await fetch("/api/state").then((r) => r.json());
  currentId = state.face || (catalog[0] && catalog[0].id) || "";
  const emo = catalog.find((e) => e.id === currentId);
  syncOverlayButtons(emo);
  renderLists();
  if (currentId) {
    await selectFace(currentId, false);
  }
  setInterval(refresh, 80);
  refresh();
}

boot();
