// Stella's Day API - single Netlify Function serving /api/*
//
// Parent portal (cookie session):
//   POST /api/login      { password }            -> sets session cookie
//   POST /api/logout
//   GET  /api/me                                 -> { ok: true } if logged in
//   GET  /api/config                             -> routines + events
//   PUT  /api/config     { sections, events }    -> save
//
// Display device (X-Device-Key header) or logged-in parent:
//   GET  /api/today?date=YYYY-MM-DD              -> today's merged list (date optional)
//   POST /api/done       { date, taskId, done }  -> record a completion
//
// Storage: Netlify Blobs store "stella"
//   key "config"           -> { sections, events }
//   key "done/YYYY-MM-DD"  -> { [taskId]: true }

import { getStore } from "@netlify/blobs";
import { createHmac, timingSafeEqual } from "node:crypto";

const SESSION_DAYS = 30;
const ICONS = ["toothbrush", "shirt", "cereal", "soccer", "books", "heart", "flower"];

const DEFAULT_CONFIG = {
  sections: [
    { id: "before_school", title: "Before School", tasks: [] },
    { id: "after_school",  title: "After School",  tasks: [] },
    { id: "bedtime",       title: "Bedtime",       tasks: [] },
  ],
  events: [],
};

// ---------- helpers ----------

const store = () => getStore("stella");

function json(data, status = 200, extraHeaders = {}) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", ...extraHeaders },
  });
}

function env(name) {
  const v = process.env[name];
  if (!v) throw new Error(`Missing environment variable ${name}`);
  return v;
}

function safeEqual(a, b) {
  const ba = Buffer.from(String(a));
  const bb = Buffer.from(String(b));
  return ba.length === bb.length && timingSafeEqual(ba, bb);
}

function sign(payload) {
  return createHmac("sha256", env("SESSION_SECRET")).update(payload).digest("hex");
}

function makeSession() {
  const payload = `ok.${Date.now() + SESSION_DAYS * 86400_000}`;
  return `${payload}.${sign(payload)}`;
}

function sessionValid(req) {
  const cookie = req.headers.get("cookie") || "";
  const m = cookie.match(/(?:^|;\s*)session=([^;]+)/);
  if (!m) return false;
  const [ok, exp, sig] = m[1].split(".");
  if (ok !== "ok" || !exp || !sig) return false;
  if (Number(exp) < Date.now()) return false;
  return safeEqual(sig, sign(`${ok}.${exp}`));
}

function sessionCookie(value, maxAge) {
  return `session=${value}; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=${maxAge}`;
}

function deviceValid(req) {
  const key = req.headers.get("x-device-key");
  return !!key && safeEqual(key, env("DEVICE_KEY"));
}

// "YYYY-MM-DD" for now in the configured timezone
function todayString() {
  const tz = process.env.TIMEZONE || "America/New_York";
  const parts = new Intl.DateTimeFormat("en-CA", {
    timeZone: tz, year: "numeric", month: "2-digit", day: "2-digit",
  }).formatToParts(new Date());
  const get = (t) => parts.find((p) => p.type === t).value;
  return `${get("year")}-${get("month")}-${get("day")}`;
}

// Day-of-week (0=Sun..6=Sat) and a short label like "Mon, Sep 15"
function dateInfo(ymd) {
  const [y, m, d] = ymd.split("-").map(Number);
  const dt = new Date(Date.UTC(y, m - 1, d));
  const dow = dt.getUTCDay();
  const label = dt.toLocaleDateString("en-US", {
    weekday: "short", month: "short", day: "numeric", timeZone: "UTC",
  });
  return { dow, label };
}

function isYMD(s) {
  return typeof s === "string" && /^\d{4}-\d{2}-\d{2}$/.test(s);
}

async function loadConfig() {
  const cfg = await store().get("config", { type: "json" });
  return cfg || structuredClone(DEFAULT_CONFIG);
}

// Validate and normalise what the portal sends before saving
function cleanConfig(input) {
  const cleanTask = (t) => ({
    id: String(t.id || "").slice(0, 40),
    name: String(t.name || "").trim().slice(0, 40),
    icon: ICONS.includes(t.icon) ? t.icon : "heart",
    days: Array.isArray(t.days) ? t.days.filter((d) => Number.isInteger(d) && d >= 0 && d <= 6) : [],
  });

  const sections = DEFAULT_CONFIG.sections.map((def) => {
    const s = (input.sections || []).find((x) => x && x.id === def.id) || {};
    return {
      id: def.id,
      title: def.title,
      tasks: (s.tasks || []).map(cleanTask).filter((t) => t.id && t.name),
    };
  });

  const events = (input.events || [])
    .map((e) => ({
      id: String(e.id || "").slice(0, 40),
      date: isYMD(e.date) ? e.date : "",
      section: DEFAULT_CONFIG.sections.some((s) => s.id === e.section) ? e.section : "after_school",
      name: String(e.name || "").trim().slice(0, 40),
      time: String(e.time || "").trim().slice(0, 12),
      icon: ICONS.includes(e.icon) ? e.icon : "heart",
    }))
    .filter((e) => e.id && e.date && e.name)
    .sort((a, b) => a.date.localeCompare(b.date));

  return { sections, events };
}

// Build the list the display shows for a given date
async function buildDay(ymd) {
  const cfg = await loadConfig();
  const done = (await store().get(`done/${ymd}`, { type: "json" })) || {};
  const { dow, label } = dateInfo(ymd);

  const sections = cfg.sections.map((s) => {
    const routine = s.tasks
      .filter((t) => t.days.includes(dow))
      .map((t) => ({ id: t.id, name: t.name, icon: t.icon, time: "", special: false, done: !!done[t.id] }));
    const oneOffs = cfg.events
      .filter((e) => e.date === ymd && e.section === s.id)
      .map((e) => ({ id: e.id, name: e.name, icon: e.icon, time: e.time, special: true, done: !!done[e.id] }));
    return { id: s.id, title: s.title, tasks: [...routine, ...oneOffs] };
  });

  return { date: ymd, label, sections };
}

// ---------- handler ----------

export default async (req) => {
  const url = new URL(req.url);
  const path = url.pathname.replace(/^\/api/, "") || "/";
  const method = req.method;

  try {
    // --- auth ---
    if (method === "POST" && path === "/login") {
      const body = await req.json().catch(() => ({}));
      if (!safeEqual(body.password || "", env("FAMILY_PASSWORD"))) {
        return json({ error: "Wrong password" }, 401);
      }
      return json({ ok: true }, 200, {
        "set-cookie": sessionCookie(makeSession(), SESSION_DAYS * 86400),
      });
    }

    if (method === "POST" && path === "/logout") {
      return json({ ok: true }, 200, { "set-cookie": sessionCookie("", 0) });
    }

    if (method === "GET" && path === "/me") {
      return json({ ok: sessionValid(req) });
    }

    const parent = sessionValid(req);
    const device = deviceValid(req);

    // --- parent-only ---
    if (path === "/config") {
      if (!parent) return json({ error: "Not logged in" }, 401);
      if (method === "GET") return json(await loadConfig());
      if (method === "PUT") {
        const body = await req.json().catch(() => null);
        if (!body) return json({ error: "Bad JSON" }, 400);
        const cfg = cleanConfig(body);
        await store().setJSON("config", cfg);
        return json(cfg);
      }
    }

    // --- device or parent ---
    if (method === "GET" && path === "/today") {
      if (!parent && !device) return json({ error: "Unauthorized" }, 401);
      const q = url.searchParams.get("date");
      const ymd = isYMD(q) ? q : todayString();
      return json(await buildDay(ymd));
    }

    if (method === "POST" && path === "/done") {
      if (!parent && !device) return json({ error: "Unauthorized" }, 401);
      const body = await req.json().catch(() => ({}));
      const ymd = isYMD(body.date) ? body.date : todayString();
      const taskId = String(body.taskId || "").slice(0, 40);
      if (!taskId) return json({ error: "taskId required" }, 400);
      const key = `done/${ymd}`;
      const done = (await store().get(key, { type: "json" })) || {};
      if (body.done === false) delete done[taskId]; else done[taskId] = true;
      await store().setJSON(key, done);
      return json({ ok: true, date: ymd, done });
    }

    return json({ error: "Not found" }, 404);
  } catch (err) {
    console.error(err);
    return json({ error: err.message || "Server error" }, 500);
  }
};

export const config = { path: "/api/*" };
