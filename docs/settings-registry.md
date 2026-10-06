# Moonlight Dock settings in the Windows registry

Moonlight stores its preferences with Qt's `QSettings`. In a normal install, and in the copy Winhanced manages at
`%LOCALAPPDATA%\Winhanced\Clients\MoonlightDock\current`, nothing is written to an `.ini` file: the values live in the
registry of the user that runs the client.

```text
HKCU\Software\Moonlight Game Streaming Project\Moonlight
```

The organization and application names come from `app/main.cpp`. Portable mode (a `portable.dat` file in the working
directory) switches the same keys to an INI file under `<working directory>\Moonlight Game Streaming Project\Moonlight.ini`.
That file name is derived from Qt's rules and has not been seen on a real install. The Winhanced-managed copy has no
`portable.dat`, so it uses the registry.

## Value types

- Booleans are `REG_SZ` text, `true` or `false`.
- Integers and enums are `REG_DWORD`.
- `certificate` and `key` are `REG_SZ` PEM text wrapped as `@ByteArray(...)`.
- A missing value means the default from `app/settings/streamingpreferences.cpp` applies. Moonlight writes every
  preference when the Settings page is left, so a profile that has been through Settings holds all of them.

To set one without opening the UI, write the value with the right type and start the client afterwards. For example, to
hide the connection warnings (this also hides the "VRR buffer limit reached" warning):

```powershell
reg add "HKCU\Software\Moonlight Game Streaming Project\Moonlight" /v connwarnings /t REG_SZ /d false /f
```

## Settings

Values in the "Seen" column come from a Claw profile that had just left the Settings page, which is why all of them are
present. Defaults are the code defaults. Descriptions follow the setting names; behaviour was checked in code only where
a line says so.

### Stream

| Value | Type | Default | Seen | Notes |
|---|---|---|---|---|
| `width` | DWORD | 1280 | 0x500 | Stream width in pixels |
| `height` | DWORD | 720 | 0x2d0 | Stream height in pixels |
| `fps` | DWORD | 60 | 0x3c | Stream frame rate |
| `bitrate` | DWORD | from resolution, fps and 4:4:4 | 0x2710 | Kbps (10000 here) |
| `unlockbitrate` | bool | false | false | |
| `autoadjustbitrate` | bool | true | true | |
| `videocfg` | DWORD | 0 | 0 | Codec: 0 auto, 1 H.264, 2 HEVC, 3 HEVC HDR (deprecated, kept for old profiles), 4 AV1, 5 PyroWave |
| `videodec` | DWORD | 0 | 0 | Decoder: 0 auto, 1 force hardware, 2 force software |
| `renderer` | DWORD | 0 | 0 | 0 auto, 1 Vulkan, 2 Metal, 3 AVSBDL (-1 is internal probing only) |
| `hdr` | bool | false | false | |
| `yuv444` | bool | false | false | |
| `pyrowavecompression` | bool | false | false | Falls back to the older `pyrowavehybrid` value if missing |
| `packetsize` | DWORD | 0 | 0 | |
| `audiocfg` | DWORD | 0 | 0 | 0 stereo, 1 5.1, 2 7.1 |
| `hostaudio` | bool | false | false | Play audio on the host |
| `gameopts` | bool | true | true | |
| `quitAppAfter` | bool | false | false | |

### Window and display

| Value | Type | Default | Seen | Notes |
|---|---|---|---|---|
| `windowmode` | DWORD | 0 (recommended fullscreen) | 0 | 0 fullscreen, 1 fullscreen desktop, 2 windowed. Falls back to the old `fullscreen` value |
| `uidisplaymode` | DWORD | windowed | 0 | 0 windowed, 1 maximized, 2 fullscreen. Falls back to the old `startwindowed` value |
| `vsync` | bool | true | true | |
| `framepacing` | bool | false | false | |
| `showperfoverlay` | bool | false | false | Debug overlay only. Does not gate any warning |
| `language` | DWORD | 0 | 0 | 0 auto, then a fixed list of locales (see `Language` in `streamingpreferences.h`) |

### VRR

| Value | Type | Default | Seen | Notes |
|---|---|---|---|---|
| `enablevrr` | bool | false | false | CLI `--no-vrr` also turns it off |
| `vrrlatencymode` | DWORD | 1 | 1 | 0 Smooth, 1 Balanced, 2 Low latency. Falls back to the retired `vrrlatencyfix` checkbox |
| `vrrbufferpermille` | DWORD | from the mode | 0x3e8 (1000) | Buffer allowance; the mode preset fills any zero value |
| `vrrtargethundredths` | DWORD | from the mode | 0x26de (9950) | Interval quality target, in hundredths of a percent |
| `vrrhistoryseconds` | DWORD | from the mode | 0x78 (120) | History window |
| `vrrtoleranceus` | DWORD | from the mode | 0x1f4 (500) | Tolerance in microseconds |
| `smoothvrrframetiming` | bool | true | true | |
| `tracevrrframes` | bool | false | false | Frame trace capture |

Mode presets (`app/settings/vrrtimingoptions.h`): Smooth 4000 / 9999 / 300 / 250, Balanced 1000 / 9950 / 120 / 500,
Low latency 500 / 9900 / 60 / 500, in the order buffer, target, history, tolerance. The seen values are the Balanced
preset. Removed on load: `allowvrrtearing` and `vrrdiagnosticmode`.

### Warnings

| Value | Type | Default | Seen | Notes |
|---|---|---|---|---|
| `connwarnings` | bool | true | false | The off switch for the red status overlay: network-quality warnings and the VRR "buffer limit reached" and "processing cannot keep up" warnings. Does not change buffering, pacing or bitrate |
| ↳ CLI override | | | | `stream --connection-warnings` / `--no-connection-warnings` overrides `connwarnings` for that launch only; omitted keeps the saved value, and the last flag wins |
| `confwarnings` | bool | true | true | Configuration warnings |
| `detectnetblocking` | bool | true | true | |

### Input

| Value | Type | Default | Seen | Notes |
|---|---|---|---|---|
| `multicontroller` | bool | true | true | |
| `gamepadmouse` | bool | true | true | |
| `backgroundgamepad` | bool | false | false | |
| `swapfacebuttons` | bool | false | false | |
| `swapmousebuttons` | bool | false | false | |
| `reversescroll` | bool | false | false | |
| `mouseacceleration` | bool | false | false | Stored under this name but loaded as the absolute mouse mode setting |
| `abstouchmode` | bool | true | true | Absolute touch mode |
| `capturesyskeys` | DWORD | 0 | 0 | 0 off, 1 fullscreen only, 2 always |

### Application

| Value | Type | Default | Seen | Notes |
|---|---|---|---|---|
| `mdns` | bool | true | true | Host discovery |
| `richpresence` | bool | true | true | |
| `muteonfocusloss` | bool | false | false | |
| `keepawake` | bool | true | true | |
| `defaultver` | DWORD | 0 | 2 | Version of the default-value migrations that have been applied (current: 2) |

### Identity and pairing

| Value | Type | Notes |
|---|---|---|
| `uniqueid` | REG_SZ | Client id sent to hosts |
| `certificate` | REG_SZ | Client certificate (PEM, `@ByteArray` wrapper). Winhanced's `MoonlightLauncher` reads it from here |
| `key` | REG_SZ | Client private key (PEM, `@ByteArray` wrapper). Treat as a secret and never log or copy it |

## Subkeys

| Subkey | Contents |
|---|---|
| `hosts\<n>` | One entry per paired host: `hostname`, `uuid`, `mac`, `localaddress`, `localport`, `remoteaddress`, `remoteport`, `manualaddress`, `manualport`, `ipv6address`, `ipv6port`, `srvcert`, `customname`, `nvidiasw` |
| `hosts\<n>\apps\<m>` | That host's cached app list |
| `hostsbackup` | Backup copy of the host list, read first when it exists |
| `gcmapping\<n>` | Per-controller button mappings: `guid` and `mapping` |

Source: `app/backend/computermanager.cpp`, `app/backend/nvcomputer.cpp`, `app/settings/mappingmanager.cpp`,
`app/backend/identitymanager.cpp`.
