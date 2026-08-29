# MuteAlert for Windows

A native Windows 11 notification-area application containing the complete
microphone widget without Windhawk or Explorer injection.

## Features

- Live microphone icon filled from bottom to top using Windows Core Audio.
- Left-click Windows input mute/unmute and scroll-over-icon volume control.
- Optional microphone-volume lock that restores a configured input level when
  Windows or another application changes it. Scrolling the tray icon updates
  the locked target while the feature is enabled.
- Compact hover help showing the active device, volume, mute state, and mouse
  controls.
- Separate Slack, Microsoft Teams, or Zoom call-state icon with app logo,
  mute slash, multiple-call badge, left-click focus, and right-click toggle.
- Speaking-while-call-muted notification and optional Windows audio cue.
- Zoom meeting persistence through `CptHost.exe`, with an optional, explicitly
  enabled `Alt+A` fallback when Zoom hides its accessible meeting toolbar.
- Opt-in headset mute synchronization through Windows hardware-mute reporting,
  standard USB HID microphone/call-mute buttons, and extensible vendor
  adapters.
- SteelSeries Arctis Nova Pro Wireless support retained as the first
  high-confidence vendor adapter.
- Headset detection method and confidence shown in Settings, with a sanitized
  diagnostics export for adding more devices without exposing serial numbers,
  device paths, or raw HID values.
- Tabbed settings panel with General, Slack, Teams, Zoom, and Headset pages.
- Embedded multi-resolution microphone application icon for Explorer,
  shortcuts, the taskbar, and Settings windows.
- Settings stored at `%LOCALAPPDATA%\MuteAlert\settings.ini`.
- Optional per-user automatic startup.
- Optional daily GitHub release check with pre-release support, a manual
  **Check now** action, verified download-and-install support, and a direct
  link to newer releases.

Middle-click either icon to open Settings. When a call is active, right-click
the microphone icon or call icon to toggle the active call microphone. With no
active call, right-click the microphone icon for Settings and Exit. Hold Ctrl
while right-clicking to open that menu even during a call.

Windows initially places new notification icons in the overflow area. Open
**Taskbar settings → Other system tray icons** to promote the microphone and
call icons to the visible taskbar area.

## Build with Visual Studio

1. Install Visual Studio 2022 with **Desktop development with C++**.
2. Open this `standalone` folder in Visual Studio.
3. Select an x64 configuration.
4. Build `MuteAlert`.

Or from a Visual Studio developer terminal:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is produced as `build\Release\MuteAlert.exe`. It does not require
administrator rights. The Release build statically embeds its C++ runtime, so
no compiler-specific DLL or `.whl` files need to be distributed beside it.
Keep the executable at a stable path if you enable automatic startup or
promote its notification icons.

## Integration limitations

Slack, Teams, and Zoom are best-effort integrations based on their accessible
Windows UI labels. Localized or changed labels can be adjusted from Settings.
Zoom's hidden-toolbar fallback is disabled by default because it temporarily
activates Zoom and sends its default `Alt+A` mute shortcut. Enable it on the
Zoom Settings page only if that shortcut has not been customized.

Headset hardware is not universal. The application reports one of four
detection methods: Windows hardware mute, standard HID mute button,
SteelSeries device state, or unsupported/no observable state. A standard HID
button is a toggle event and does not by itself prove the position of a
latched physical switch. Purely mechanical microphone disconnect switches
cannot be observed by software, and a zero audio level is never interpreted as
mute. Headset synchronization and standard HID monitoring are disabled by
default; enabling them is an explicit choice on the Headset Settings page.

See [HEADSET_ADAPTERS.md](HEADSET_ADAPTERS.md) for the provider architecture,
privacy rules for diagnostics, and instructions for adding vendor protocols.

## Update checks

MuteAlert can make an anonymous request to GitHub's public releases API at
startup when the previous successful check is at least 24 hours old. No GitHub
account or access token is used. Automatic checks and inclusion of pre-release
versions can be changed on the Updates page in Settings; that page also offers
a manual check. GitHub builds can download a `MuteAlert.exe` release asset,
require its GitHub-provided SHA-256 digest to match, preserve the previous
executable as `MuteAlert.exe.previous`, replace the app after it exits, and
restart it. Automatic installation is disabled by default and can be enabled
on the Updates page. Builds distributed through an app store should use that
store's update mechanism instead.

## License

The GitHub source and free GitHub build are source-available under the
[PolyForm Shield License 1.0.0](LICENSE.md), with the declarations in
[NOTICE](NOTICE). You may use, inspect, modify, and redistribute this edition,
but you may not use it to provide a product that competes with MuteAlert.

Official Microsoft Store and other commercial distributions may be offered
under separate commercial terms. PolyForm Shield is a source-available license,
not an OSI-approved open-source license.
