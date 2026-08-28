# Microsoft Store commerce

The Store package is free to install. InputRack Pro is a one-time, durable
Microsoft Store add-on. The application uses the add-on's in-app offer token
`inputrack.pro`; create that token in Partner Center before submission.

Pro costs EUR 29.99 once, with a planned EUR 19.99 launch sale. Before buying,
each Windows user can explicitly start a 14-day trial. Partner Center provides
native add-on trials only for subscriptions, so this trial is tracked locally
under `HKCU\Software\InputRack`; it never asks for payment information. The
permanent license continues to come exclusively from the Microsoft Store.

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
