import json
import os

import requests

DEFAULT_MODEL = "llama-3.3-70b-versatile"

KEY_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "oracle.key")


def _file_key():
    try:
        with open(KEY_FILE, "r", encoding="utf-8") as f:
            return f.read().strip() or None
    except Exception:
        return None


def _key():
    return (os.environ.get("GROQ_API_KEY") or os.environ.get("ORACLE_API_KEY")
            or _file_key())


def save_key(key):
    key = (key or "").strip()
    if not key:
        return False
    with open(KEY_FILE, "w", encoding="utf-8") as f:
        f.write(key + "\n")
    try:
        os.chmod(KEY_FILE, 0o600)
    except Exception:
        pass
    return True


def key_status():
    if os.environ.get("GROQ_API_KEY") or os.environ.get("ORACLE_API_KEY"):
        return "env var"
    if _file_key():
        return "oracle.key"
    return None


def analyze_profile(profile, cfg=None, model=None):
    key = _key()
    if not key:
        return ("No GROQ_API_KEY set; skipping LLM analysis. "
                "Set GROQ_API_KEY to enable --oracle.")
    model = model or DEFAULT_MODEL
    summary = profile.get("summary", {})
    anomalies = profile.get("anomalies", [])
    transitions = profile.get("transitions", [])[-50:]

    prompt_lines = [
        "You are analyzing a passive observation profile of a DeskBuddy productivity "
        "device (LD2410 radar presence + AI wellness reminders).",
        "Provide: (1) a short narrative of the work session, (2) notable anomalies, "
        "(3) concrete recommendations to improve focus/break behaviour.",
        "",
        "SUMMARY:",
        json.dumps(summary, indent=2),
        "ANOMALIES:",
        json.dumps(anomalies, indent=2),
        "STATE TRANSITIONS:",
        json.dumps(transitions, indent=2),
    ]
    body = {
        "model": model,
        "messages": [{"role": "user", "content": "\n".join(prompt_lines)}],
        "temperature": 0.4,
        "max_tokens": 700,
    }
    try:
        r = requests.post("https://api.groq.com/openai/v1/chat/completions",
                          headers={"Authorization": "Bearer %s" % key},
                          json=body, timeout=90)
        r.raise_for_status()
        data = r.json()
        return data["choices"][0]["message"]["content"].strip()
    except Exception as e:
        return "Oracle analysis failed: %s" % e
