# CardOS (M5Stack Cardputer ADV)

A sleek, custom standalone OS for the **M5Stack Cardputer ADV** featuring a multi-page app launcher, Wi-Fi Over-the-Air updates, GitHub App Store (30+ Lua apps & games), MP3 Music Player + Phone Stream, Voice Recorder with Auto Mic Sensitivity, ApOS universal captive-portal web workspace, Android-style Wi-Fi & Bluetooth in Settings, Crash BSOD recovery diagnostics, and customizable notification LEDs.

## Built-in Apps
- **Home** (`-`/`=` or `\` Move Mode, Search bar, live Widgets)
- **Music** (`/music/<playlist>/` MP3s, All Songs, Shuffle, Speaker/3.5mm AUX auto-switch, Wi-Fi Phone Stream on `:8080`, independent Media Volume)
- **Voice** (`M5Cardputer.Mic` 16 kHz WAV recorder in `/recordings/`, manual & Auto Mic Sensitivity AGC, independent Media Volume playback)
- **ApOS** (Create customizable Wi-Fi AP with automatic "Sign in to Wi-Fi network" captive portal -> full web file manager, Lua/text editor, upload/download, remote terminal & keyboard from any Mac/PC/phone)
- **Shell** (On-device terminal + SD file commands + `bsod` test)
- **Settings** (Wi-Fi & Android-style Bluetooth toggles/naming, per-app Notification Color & `Pulse`/`Blink`/`Fast`/`Solid` LED patterns, Media Output Volume, Saver Widgets, GitHub OTA `Software Update`)
- **Notes**, **Calendar**, **Folders**, **Calculator**, **Gyro/Compass**, **Converter**, **Weather**, **Kryptex Miner**, **System Info**, **USB Disk**, **App Store** (`[I]` Full Info modal, Updates tab & Auto-Update), **Sleep**, **Snake**, **Flappy**.

## Developer Documentation
Want to create your own Lua apps or native C++ apps for CardOS? Read **[DEVELOPERS.md](./DEVELOPERS.md)** for the full step-by-step guide and `cardos.*` API reference!

## Updating CardOS
Connect to Wi-Fi with an SD card inserted and open **Settings → System → Software Update**. CardOS automatically checks GitHub for OS and App Store updates on boot / Wi-Fi connect and every 24 hours.
