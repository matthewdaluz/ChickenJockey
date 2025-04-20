# 🐔 Chicken Jockey (Proof-Of-Content/Alpha)

**Chicken Jockey** is an offline, tamper-resistant website blocking utility for Windows.  
It leverages the system hosts file and persistent background services to enforce blocklists — even if the system is rebooted or tampered with.

---

## 🔐 Key Features

- Blocks websites system-wide using `hosts` file overrides
- Background watchdog services auto-repair any tampering
- GUI for entering or importing domains to block
- AES-encrypted password generation (OpenSSL)
- Persistent on boot via Windows Service
- Lightweight (~100 KB), no internet or background syncs required

---

## 🚀 Quick Start

### 🖥️ Launch GUI and Add Blocked Domains

```bash
ChickenJockey.exe --gui
