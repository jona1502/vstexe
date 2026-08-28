# Publishing the chain through a virtual audio cable

## Why a cable

Windows offers no way for an ordinary application to create a capture endpoint.
Only a kernel driver can publish a device that appears in every application's
microphone list. InputRack ships such a driver, but it is not release-signed,
so it cannot be installed on a machine with Secure Boot enabled. That path is
documented in [WINDOWS_VIRTUAL_MIC.md](WINDOWS_VIRTUAL_MIC.md) and remains the
long-term goal.

Until then InputRack uses a virtual audio cable: an already signed third-party
driver that exposes a render endpoint and a capture endpoint wired together.
InputRack writes the processed chain into the render side, and Discord, OBS or
any other application reads the capture side as a microphone.

```text
physical microphone -> InputRack (VST3 chain) -> cable render endpoint
                                                       |
                                     cable capture endpoint -> Discord / OBS
```

## Installing a cable

Any virtual audio cable works. [VB-CABLE](https://vb-audio.com/Cable/) is the
smallest option and installs a single pair:

- render endpoint: `CABLE Input (VB-Audio Virtual Cable)`
- capture endpoint: `CABLE Output (VB-Audio Virtual Cable)`

VoiceMeeter, SteelSeries Sonar and Elgato Wave Link create equivalent pairs and
are recognised as well. InputRack does not bundle any of them; each is
installed and licensed separately by its vendor.

## Routing

1. Start InputRack and select the physical microphone as the input device.
2. Select the cable's **render** endpoint as the output device.
3. Enable **Monitor**. This connects the end of the plug-in chain to the output
   device; without it nothing is published.
4. In Discord or OBS, select the cable's **capture** endpoint as the microphone.

InputRack opens this flow as a setup assistant on first launch. It can be opened
again from **... > Setup assistant**. Its routing test verifies that an input is
selected, a recognised virtual cable is the output, monitoring is enabled, and
a processed signal has actually reached the output meter.

The status line names both the device being written to and the endpoint to
select elsewhere, and says so explicitly when monitoring is off or when the
selected output is not a recognised cable.

## Known limitations

One audio device carries one output, so while the chain is routed into the cable
you cannot also hear it through headphones from within InputRack. Audio
interfaces with hardware direct monitoring cover this; otherwise the chain is
audible only to the applications reading the cable.

The microphone input is mono and the processing and cable output paths are
stereo at 48 kHz. InputRack copies the dry microphone to left and right before
the first effect, so stereo VST3 effects can create different left and right
signals. A mono-only effect downmixes the signal at its position in the chain;
later stereo effects can create stereo width again. Set the cable's endpoints
to 48 kHz stereo in the Windows sound settings so no format conversion is
inserted between InputRack and the reading application.

Two applications reading the same capture endpoint simultaneously share one
stream. A second cable pair avoids the interference if two consumers each need
independent control.
