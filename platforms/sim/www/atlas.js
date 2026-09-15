const left = document.getElementById("left");
const right = document.getElementById("right");
const leftCtx = left.getContext("2d");
const rightCtx = right.getContext("2d");
const lists = document.getElementById("lists");
const meta = document.getElementById("meta");
const micEl = document.getElementById("mic");

let catalog = [];
let currentId = "";
let classicOverlays = true;

function decodeBase64(b64) {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i += 1) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

function drawPanel(ctx, rgb, w, h) {
  const img = ctx.createImageData(w, h);
  for (let i = 0; i < w * h; i += 1) {
    img.data[i * 4] = rgb[i * 3];
    img.data[i * 4 + 1] = rgb[i * 3 + 1];
    img.data[i * 4 + 2] = rgb[i * 3 + 2];
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
      btn.className = "emo-btn" + (emo.id === currentId ? " active" : "");
      btn.textContent = emo.short || emo.id;
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
  const overlays = emo.kind === "classic" ? "рот и морг включены" : "оверлеи выкл";
  return `${emo.id} · ${emo.kind} · effect ${emo.effect} · ${emo.transition} · ${overlays}`;
}

async function selectFace(id, withTransition) {
  currentId = id;
  const emo = catalog.find((e) => e.id === id);
  classicOverlays = Boolean(emo && emo.kind === "classic");
  meta.textContent = describe(emo);
  document.getElementById("blink").disabled = !classicOverlays || (emo && !emo.allow_blink);
  micEl.disabled = !classicOverlays;
  document.getElementById("boop").disabled = Boolean(emo && !emo.allow_boop);
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
    classicOverlays = Boolean(emo && emo.kind === "classic");
    meta.textContent = describe(emo);
    renderLists();
  }
  if (state.matrix && state.matrix.rgb) {
    const rgb = decodeBase64(state.matrix.rgb);
    drawPanel(leftCtx, rgb, state.matrix.w, state.matrix.h);
    drawPanel(rightCtx, rgb, state.matrix.w, state.matrix.h);
  }
}

async function sendMic() {
  await fetch("/api/sensors", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: micBody(0),
  });
}

document.getElementById("blink").addEventListener("click", async () => {
  await fetch("/api/preview", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: "blink=1",
  });
});

function micBody(prox) {
  const mic = Number(micEl.value) / 100;
  return `mic=${mic.toFixed(3)}&prox=${prox}`;
}

const boopBtn = document.getElementById("boop");
boopBtn.addEventListener("pointerdown", async (event) => {
  event.preventDefault();
  await fetch("/api/sensors", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: micBody(1),
  });
});
const releaseBoop = async () => {
  await fetch("/api/sensors", {
    method: "POST",
    headers: { "Content-Type": "text/plain; charset=utf-8" },
    body: micBody(0),
  });
};
boopBtn.addEventListener("pointerup", releaseBoop);
boopBtn.addEventListener("pointerleave", releaseBoop);

document.getElementById("replay").addEventListener("click", () => {
  if (currentId) {
    selectFace(currentId, true);
  }
});

micEl.addEventListener("input", sendMic);

async function boot() {
  const res = await fetch("/api/emotions", { cache: "no-store" });
  const data = await res.json();
  catalog = data.emotions || [];
  const state = await fetch("/api/state").then((r) => r.json());
  currentId = state.face || (catalog[0] && catalog[0].id) || "";
  renderLists();
  if (currentId) {
    await selectFace(currentId, false);
  }
  setInterval(refresh, 80);
  refresh();
}

boot();
