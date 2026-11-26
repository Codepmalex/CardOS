# CardOS (M5Stack Cardputer ADV)

A sleek, custom standalone OS for the **M5Stack Cardputer ADV** featuring a multi-page app launcher, Wi-Fi Over-the-Air updates, GitHub App Store (30+ Lua apps & games), MP3 Music Player + Phone Stream, Voice Recorder, Phone Link, MacLink AP wireless compute bridge, Shell, and hardware RGB notification customization.

## Built-in Apps
- **Home** (`[`/`]`, `-`/`=`, or `\` Move Mode, Search bar, live Widgets)
- **Music** (`/music/<playlist>/` MP3s, All Songs, Shuffle, Speaker/3.5mm AUX auto-switch, Wi-Fi Phone Stream on `:8080`)
- **Voice** (`M5Cardputer.Mic` 16 kHz WAV recorder & playback in `/recordings/`)
- **Phone** (BLE `"CardOS-Phone"` + Wi-Fi `:8081` for Texts, Call Alerts, Notifications & Quick Replies)
- **MacLink** (Create `CardOS-AP` Wi-Fi hotspot & run wireless compute/shell/media control on Mac via `tools/cardos_mac_bridge.py`)
- **Shell** (On-device CLI + `@mac <cmd>` wireless Mac execution)
- **Notes**, **Calendar**, **Folders**, **Calculator**, **Gyro/Compass**, **Converter**, **Weather**, **Kryptex Miner**, **System Info**, **USB Disk**, **App Store** (with `[I]` Full Info modal, Updates tab & Auto-Update), **Settings**, **Sleep**, **Snake**, **Flappy**.

## Developer Documentation
Want to create your own Lua apps or native C++ apps for CardOS? Read **[DEVELOPERS.md](./DEVELOPERS.md)** for the full step-by-step guide and `cardos.*` API reference!

## Updating CardOS
Once flashed, connect to Wi-Fi with an SD card inserted and go to **Settings → System → Software Update**. CardOS automatically checks GitHub for OS and App Store updates on boot/Wi-Fi connect and every 24 hours.
