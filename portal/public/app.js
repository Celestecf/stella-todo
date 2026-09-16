// Stella's Day parent portal

// Icon name -> emoji. Names must match ICON_TABLE in the firmware's icons.h.
const ICON_GROUPS = {
  "Routine": {
    toothbrush: "🪥", hairbrush: "🪮", shirt: "👕", cereal: "🥣", backpack: "🎒",
    vitamin: "💊", pencil: "✏️", homework: "📝", books: "📚", shower: "🚿",
    bath: "🛁", bed: "🛏️", moon: "🌙", sun: "☀️", clock: "⏰", socks: "🧦",
    shoes: "👟", coat: "🧥", pajamas: "👚", laundry: "🧺", trash: "🗑️",
    dishes: "🍽️", toys: "🧸", plant: "🪴", water: "💧", milk: "🥛",
    apple: "🍎", lunch: "🥪", banana: "🍌",
  },
  "Pets": { dog: "🐶", cat: "🐱", fish: "🐟" },
  "Activities": {
    soccer: "⚽", ballet: "🩰", swim: "🏊", bike: "🚲", piano: "🎹", music: "🎵",
    basketball: "🏀", tennis: "🎾", gymnastics: "🤸", dance: "💃", art: "🎨",
    game: "🎮", tv: "📺", tablet: "📱",
  },
  "Places & events": {
    school: "🏫", bus: "🚌", car: "🚗", doctor: "🩺", dentist: "🦷",
    gift: "🎁", cake: "🎂", party: "🎉", umbrella: "☂️",
  },
  "Fun": {
    heart: "❤️", flower: "🌸", star: "⭐", sparkles: "✨", rainbow: "🌈",
    unicorn: "🦄", medal: "🏅", smile: "😊",
  },
};
const ICONS = Object.assign({}, ...Object.values(ICON_GROUPS));
const DAY_NAMES = ["S", "M", "T", "W", "T", "F", "S"];

const $ = (sel) => document.querySelector(sel);
const el = (tag, attrs = {}, ...children) => {
  const n = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === "class") n.className = v;
    else if (k.startsWith("on")) n.addEventListener(k.slice(2), v);
    else if (k === "html") n.innerHTML = v;
    else n.setAttribute(k, v);
  }
  for (const c of children) n.append(c);
  return n;
};
const uid = () => Math.random().toString(36).slice(2, 10);
const todayYMD = () => {
  const d = new Date();
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
};
const prettyDate = (ymd) => {
  const [y, m, d] = ymd.split("-").map(Number);
  return new Date(y, m - 1, d).toLocaleDateString("en-US", { weekday: "short", month: "short", day: "numeric" });
};

async function api(path, opts = {}) {
  const res = await fetch(`/api${path}`, {
    headers: { "content-type": "application/json" },
    ...opts,
    body: opts.body ? JSON.stringify(opts.body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
  return data;
}

let config = null;

// ---------- auth ----------

async function boot() {
  const me = await api("/me").catch(() => ({ ok: false }));
  if (me.ok) await enterApp(); else showLogin();
}

function showLogin() {
  $("#login").classList.remove("hidden");
  $("#app").classList.add("hidden");
  $("#password").focus();
}

$("#login-form").addEventListener("submit", async (e) => {
  e.preventDefault();
  const err = $("#login-error");
  err.classList.add("hidden");
  try {
    await api("/login", { method: "POST", body: { password: $("#password").value } });
    $("#password").value = "";
    await enterApp();
  } catch (ex) {
    err.textContent = ex.message;
    err.classList.remove("hidden");
  }
});

$("#logout").addEventListener("click", async () => {
  await api("/logout", { method: "POST" });
  showLogin();
});

async function enterApp() {
  config = await api("/config");
  $("#login").classList.add("hidden");
  $("#app").classList.remove("hidden");
  renderRoutine();
  renderEventForm();
  renderEvents();
  $("#today-date").value = todayYMD();
}

// ---------- tabs ----------

document.querySelectorAll(".tab").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((b) => b.classList.toggle("active", b === btn));
    document.querySelectorAll(".tab-panel").forEach((p) => p.classList.toggle("hidden", p.id !== `tab-${btn.dataset.tab}`));
    if (btn.dataset.tab === "today") renderToday();
  });
});

// ---------- saving ----------

let saveTimer = null;
function scheduleSave() {
  $("#save-status").textContent = "Saving…";
  clearTimeout(saveTimer);
  saveTimer = setTimeout(async () => {
    try {
      // Keep the local object as the source of truth: rendered widgets hold
      // references into it, so swapping in the server's copy would orphan them.
      await api("/config", { method: "PUT", body: config });
      $("#save-status").textContent = "Saved ✓";
      setTimeout(() => { if ($("#save-status").textContent === "Saved ✓") $("#save-status").textContent = ""; }, 2000);
    } catch (ex) {
      $("#save-status").textContent = `Save failed: ${ex.message}`;
    }
  }, 600);
}

// ---------- routine ----------

function iconSelect(current, onchange, withNames = false) {
  const sel = el("select", { class: "icon-pick", onchange: (e) => onchange(e.target.value) });
  for (const [group, icons] of Object.entries(ICON_GROUPS)) {
    const og = el("optgroup", { label: group });
    for (const [key, emoji] of Object.entries(icons)) {
      const o = el("option", { value: key }, withNames ? `${emoji} ${key}` : emoji);
      if (key === current) o.selected = true;
      og.append(o);
    }
    sel.append(og);
  }
  return sel;
}

function renderRoutine() {
  const root = $("#sections");
  root.innerHTML = "";
  for (const section of config.sections) {
    const list = el("div", { class: "task-list" });
    section.tasks.forEach((task, i) => list.append(renderTask(section, task, i)));

    root.append(el("div", { class: "card section-card" },
      el("h2", {}, section.title),
      list,
      el("button", { class: "add-task", onclick: () => {
        section.tasks.push({ id: uid(), name: "", icon: "heart", days: [1, 2, 3, 4, 5] });
        renderRoutine();
        scheduleSave();
        const inputs = root.querySelectorAll(".name");
        inputs[inputs.length - 1]?.focus();
      } }, "+ Add task"),
    ));
  }
}

function renderTask(section, task, index) {
  const days = el("div", { class: "days" });
  DAY_NAMES.forEach((label, d) => {
    const b = el("button", { class: "day" + (task.days.includes(d) ? " on" : ""), title: ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"][d],
      onclick: () => {
        task.days = task.days.includes(d) ? task.days.filter((x) => x !== d) : [...task.days, d].sort();
        b.classList.toggle("on");
        scheduleSave();
      } }, label);
    days.append(b);
  });

  const move = (delta) => {
    const j = index + delta;
    if (j < 0 || j >= section.tasks.length) return;
    [section.tasks[index], section.tasks[j]] = [section.tasks[j], section.tasks[index]];
    renderRoutine();
    scheduleSave();
  };

  return el("div", { class: "task" },
    iconSelect(task.icon, (v) => { task.icon = v; scheduleSave(); }),
    el("input", { class: "name", type: "text", value: task.name, placeholder: "Task name", maxlength: "40",
      oninput: (e) => { task.name = e.target.value; scheduleSave(); } }),
    el("div", { class: "task-actions" },
      el("button", { class: "icon", title: "Move up", onclick: () => move(-1) }, "↑"),
      el("button", { class: "icon", title: "Move down", onclick: () => move(1) }, "↓"),
      el("button", { class: "icon", title: "Remove", onclick: () => {
        if (task.name && !confirm(`Remove "${task.name}"?`)) return;
        section.tasks.splice(index, 1);
        renderRoutine();
        scheduleSave();
      } }, "✕"),
    ),
    days,
  );
}

// ---------- events ----------

function renderEventForm() {
  const sec = $("#ev-section");
  sec.innerHTML = "";
  for (const s of config.sections) sec.append(el("option", { value: s.id }, s.title));
  sec.value = "after_school";

  const ic = $("#ev-icon");
  ic.replaceChildren(...iconSelect("soccer", () => {}, true).children);
  ic.value = "soccer";

  $("#ev-date").value = todayYMD();
}

$("#event-form").addEventListener("submit", (e) => {
  e.preventDefault();
  config.events.push({
    id: uid(),
    name: $("#ev-name").value.trim(),
    date: $("#ev-date").value,
    time: $("#ev-time").value.trim(),
    section: $("#ev-section").value,
    icon: $("#ev-icon").value,
  });
  config.events.sort((a, b) => a.date.localeCompare(b.date));
  $("#ev-name").value = "";
  $("#ev-time").value = "";
  renderEvents();
  scheduleSave();
});

$("#show-past").addEventListener("change", renderEvents);

function renderEvents() {
  const root = $("#events-list");
  root.innerHTML = "";
  const today = todayYMD();
  const showPast = $("#show-past").checked;
  const list = config.events.filter((ev) => showPast || ev.date >= today);
  if (!list.length) { root.append(el("p", { class: "empty" }, "Nothing scheduled.")); return; }

  for (const ev of list) {
    const section = config.sections.find((s) => s.id === ev.section);
    root.append(el("div", { class: "event" + (ev.date < today ? " past" : "") },
      el("span", { class: "emoji" }, ICONS[ev.icon] || "❤️"),
      el("div", {},
        el("div", {}, ev.name),
        el("div", { class: "meta" }, `${section?.title || ev.section}${ev.time ? " · " + ev.time : ""}`),
      ),
      el("span", { class: "when" }, prettyDate(ev.date)),
      el("button", { class: "icon", title: "Remove", onclick: () => {
        config.events = config.events.filter((x) => x.id !== ev.id);
        renderEvents();
        scheduleSave();
      } }, "✕"),
    ));
  }
}

// ---------- today ----------

$("#today-date").addEventListener("change", renderToday);

async function renderToday() {
  const root = $("#today-view");
  const date = $("#today-date").value || todayYMD();
  root.innerHTML = "";
  let day;
  try { day = await api(`/today?date=${date}`); }
  catch (ex) { root.append(el("p", { class: "error" }, ex.message)); return; }

  root.append(el("h2", {}, day.label));
  for (const s of day.sections) {
    const card = el("div", { class: "card today-section" }, el("h2", {}, s.title));
    if (!s.tasks.length) card.append(el("p", { class: "empty" }, "Nothing today."));
    for (const t of s.tasks) {
      const row = el("div", { class: "today-task" + (t.done ? " done" : "") + (t.special ? " special" : ""),
        onclick: async () => {
          t.done = !t.done;
          row.classList.toggle("done", t.done);
          row.querySelector(".box").textContent = t.done ? "✓" : "";
          await api("/done", { method: "POST", body: { date: day.date, taskId: t.id, done: t.done } });
        } },
        el("span", { class: "emoji" }, ICONS[t.icon] || "❤️"),
        el("div", {}, el("div", {}, t.name), t.time ? el("div", { class: "time" }, t.time) : ""),
        el("div", { class: "box" }, t.done ? "✓" : ""),
      );
      card.append(row);
    }
    root.append(card);
  }
}

boot();
