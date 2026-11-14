# Changelog

What changed in each release. New notes go under **Unreleased** while the work is done; a release moves them under its version, and the release workflow publishes that section as the GitHub release notes (it refuses to release a version without them).

## Unreleased

- The **Device** tab shows the address that opens the device (`gizmo-reolink-switch.local`). It showed the Wi-Fi's default name (`esp32s3-XXXXXX.local`), which doesn't open it.
- The router lists the device by its name (`gizmo-reolink-switch`) instead of `esp32s3-XXXXXX`.
- gizmo-common v2.5.2.

## v1.2.0 - 2026-11-13

- A new Gizmo Reolink Switch logo, the slashed bell in the OpenGizmo style, as the favicon and the phone's home-screen icon, with a dark theme version in the header.
- The first tab is now **Switch** (it was Home), and the tabs follow the same order as every OpenGizmo app: Switch, NVR, Device, WiFi.
- The **Device** tab (it was Settings) has the device details, the network, Restart and Forget network. Its first group is now **Info**.
- The language row is gone from the Device tab: pick the language with the flag button of the top bar, like in every OpenGizmo app.
- Through the setup hotspot, the setup page shows its logo again (from gizmo-common v2.4.1: the captive portal no longer redirects the web app's own files).
- gizmo-common v2.5.1 and ui-vintage 0.9.51.

Flash it again from the website to get the new web app: the saved Wi-Fi and NVR login are kept.

## Earlier versions

See the [GitHub releases](https://github.com/open-gizmo/gizmo-reolink-switch/releases) up to v1.1.0.
