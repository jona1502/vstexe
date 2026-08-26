# Windows virtual microphone design

> **Status:** development only. This driver has no Microsoft release signature,
> so installing it requires Windows test-signing and therefore a machine with
> Secure Boot disabled. The route that works on a stock installation is a
> virtual audio cable, documented in [VIRTUAL_CABLE.md](VIRTUAL_CABLE.md).

## User-visible result

The installed driver exposes one Windows capture endpoint named **InputRack
Microphone**. Applications such as Discord select this endpoint as their input.
The physical microphone remains owned by the InputRack desktop application.

```text
physical microphone -> JUCE/VST3 graph -> PCM16 packet transport
                    -> kernel ring buffer -> WaveRT capture endpoint -> Discord
```

Local monitoring is a separate graph branch and is disabled by default. Changing
the monitoring setting must never stop publication to the virtual microphone.

## MVP stream contract

- 48,000 samples per second
- one channel
- signed 16-bit little-endian PCM
- at most 960 frames per user-mode write
- monotonically increasing packet sequence number
- two seconds of preallocated driver-side ring-buffer capacity

The shared structures in `VirtualMicProtocol.h` are the ABI between user mode
and kernel mode. A major-version mismatch is fatal. A newer minor version is
accepted when all required structure fields and the fixed stream format match.

## Transport and real-time behaviour

The desktop app opens one exclusive control device and writes a header followed
by PCM payload. The driver validates every size and format field before copying
the payload into a nonpaged, bounded ring buffer. This initial transport moves
only about 96 kB/s and intentionally favours a small auditable interface over a
user-mapped kernel buffer.

The WaveRT capture stream consumes the ring buffer. On underrun it returns
silence. On overrun the oldest complete frames are discarded. Neither path may
allocate memory, touch files, log synchronously, or wait on user mode.

Closing or crashing the app immediately changes the published signal to silence.
Only one writer is permitted. Capture clients may open the Windows endpoint
normally through the system audio engine.

## Security and installation

- The control device ACL permits interactive authenticated users, not remote or
  anonymous access.
- Packet lengths are checked with overflow-safe arithmetic before any copy.
- The production installer deploys a componentized, signed driver package.
- Development builds use a dedicated test certificate and Windows test-signing
  mode; release packages must not enable test-signing.

## Development install on Windows 11

Install Visual Studio 2022 Build Tools with Desktop C++ and Driver development,
the Windows 11 SDK, and the matching WDK. Then open PowerShell as Administrator
in the repository root. The management script builds the driver, creates a
minimal INF/SYS/CAT package, manages the local development certificate, and
creates the root-enumerated audio device.

```powershell
.\driver\windows\manage-driver.ps1 Install -EnableTestSigning
```

The first run enables Windows test-signing and stops. Restart Windows, open an
Administrator PowerShell again, and run:

```powershell
.\driver\windows\manage-driver.ps1 Install
```

Secure Boot can prevent Windows test-signing from being enabled. Do not weaken a
production machine's boot policy for a development build; use a dedicated test
system or VM instead. A production release requires Microsoft-signed driver
artifacts and does not use this development certificate.

Check all three installation layers at any time without administrator rights:

```powershell
.\driver\windows\manage-driver.ps1 Status
```

`Device`, `Driver`, and `Endpoint` must all be available. Start InputRack and
select **InputRack Microphone** as the input device in Discord. Local monitoring
may remain off; it is a separate branch from the virtual microphone.

For a clean development uninstall, use an Administrator PowerShell:

```powershell
.\driver\windows\manage-driver.ps1 Uninstall
```

This removes both the root device and its matching package from the Windows
driver store. It deliberately leaves the development certificate and the global
test-signing setting unchanged because either may be shared with other test
drivers. Disable test-signing manually only after verifying that no other test
driver needs it.

## Delivery sequence

1. Freeze and test the protocol ABI.
2. Create the mono 48-kHz capture endpoint and signed development package.
3. Add the control device, packet validation, and kernel ring buffer.
4. Publish post-VST samples from the desktop engine independently of monitoring.
5. Add install, uninstall, diagnostics, and recovery checks.
