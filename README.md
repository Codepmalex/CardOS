# CardOS for M5Stack Cardputer ADV

A custom, feature-rich OS for the **M5Stack Cardputer ADV** (ESP32-S3FN8) built with Arduino + PlatformIO.

## Included Features
- **Home Screen & Search:** Live clock, customizable Home Widgets (`SysInfo`, `Weather`, `Miner`, `Calendar`, `Notes`, `Snake`, `Flappy`, `Off`), global file/app search (`Tab`), and reorderable app grid (`[` / `]`).
- **Stock Apps:** Notes (SD text editor with save prompt), Calendar (JSON events), Calculator, Gyro/Compass (BMI270 IMU), Unit Converter, Folders Explorer, USB Mass Storage (in-place USB drive mode), Wi-Fi Setup (multi-network auto-connect), SysInfo, Snake, Sleep Mode (silent charging), Settings (with World Timezone Map, Per-App Notifications, Screensaver, Low Battery ECO Mode, Themes & Animated Dropdowns).
- **GitHub App Store (`store/catalog.json`):** Download categorized apps directly onto your Home screen over Wi-Fi, or boot third-party `.bin` firmwares from `/apps/` on SD via the **Other Apps** tab (`app1` OTA partition).

## Building & Flashing
1. Install [PlatformIO Core](https://platformio.org/).
2. Build and flash from the `CardOS/` directory:
   ```bash
   pio run -t upload
   ```
3. To enable the dual-boot `app0` (CardOS) / `app1` (`.bin` app) flow on RST, flash `bootloader_cardos.bin` at `0x0` and `partitions.csv` at `0x8000`.

## Adding Apps to the Store
To add downloadable app packages in your fork or PR:
1. Add an entry in `store/catalog.json` under `"apps"` with `id`, `name`, `category`, `version`, `desc`, and `pkg`.
2. Add a matching manifest file in `store/apps/<id>.json` and implement your `App` class inside `src/apps/`.
