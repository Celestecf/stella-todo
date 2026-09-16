# Stella's Day - parent portal

Static site + one Netlify Function (`netlify/functions/api.mjs`) + Netlify Blobs
for storage. No database to run, no build step.

## Deploy on Netlify (once)

1. In Netlify: **Add new site → Import an existing project → GitHub** and pick this repo.
2. Set **Base directory** to `portal`. Publish directory and functions directory are
   read from `netlify.toml`. Deploy.
3. **Site configuration → Environment variables**, add:

   | Name | Value |
   |---|---|
   | `FAMILY_PASSWORD` | the password you and Matt will log in with |
   | `DEVICE_KEY` | a long random string; the ESP32 sends it in the `X-Device-Key` header |
   | `SESSION_SECRET` | a long random string used to sign login cookies |
   | `TIMEZONE` | e.g. `America/New_York` (decides what "today" is) |

   Generate random strings with: `openssl rand -hex 24`
4. **Deploys → Trigger deploy** so the new variables take effect.

Every push to the repo redeploys automatically.

## Run locally

```
cd portal
npm install
cp .env.example .env    # then edit the values
npx netlify dev         # http://localhost:8888
```

## API (what the ESP32 uses)

```
GET  /api/today                 header: X-Device-Key: <DEVICE_KEY>
POST /api/done                  header: X-Device-Key, body: {"taskId":"...","done":true}
```

`/api/today` returns the merged list for today:

```json
{
  "date": "2026-09-15",
  "label": "Tue, Sep 15",
  "sections": [
    { "id": "before_school", "title": "Before School",
      "tasks": [ { "id": "abc", "name": "Brush teeth", "icon": "toothbrush",
                   "time": "", "special": false, "done": true } ] },
    { "id": "after_school", "title": "After School", "tasks": [] },
    { "id": "bedtime", "title": "Bedtime", "tasks": [] }
  ]
}
```
