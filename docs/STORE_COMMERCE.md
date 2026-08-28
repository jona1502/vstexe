# Microsoft Store commerce

The Store package is free to install. InputRack Pro is a one-time, durable
Microsoft Store add-on. The application uses the add-on's in-app offer token
`inputrack.pro`; create that token in Partner Center before submission.

Pro unlocks named profiles, automatic per-application profile switching,
global bypass/profile hotkeys, and the Windows startup shortcut. Audio
processing, plug-in hosting, monitoring, and manual routing remain available
without a purchase.

The **Get Pro** menu starts the Store purchase UI. **Restore purchases** queries
the current Microsoft account's durable products. Network or Store errors never
disable audio and never erase an entitlement already confirmed in the running
session.

For local Store-build testing only, set `INPUTRACK_PRO=1` before starting the
application. Non-Store developer builds have Pro enabled so development and
direct-install workflows do not depend on Partner Center.
