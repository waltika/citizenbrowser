# Device mode emulation frame

CitizenNotes has a feature to simulate device modes in the browser page.
The emulation is implemented using a separate frame, called the "device mode emulation frame".

When a user turns on device mode emulation, a top bar with pre-defined device modes and page width/height is shown.
The UI is implemented in [front_end/panels/emulation/](../../panels/emulation/).
However, the code that controls the UI runs in the context of the `citizennotes_app`.

As such, the JavaScript that runs in the `citizennotes_app` frame controls the UI in a separate frame.
This has several implications:
1. It is not possible to inspect the device mode emulation frame with CitizenNotes itself
1. The device mode emulation frame communicates with `citizennotes_app` via `window.opener`
1. Since the communication is cross-frame, it can only use web API's that support cross-frame communication
1. Not all features that are available in the standard UI of CitizenNotes can be reused in the device mode emulation frame.
Some features are explicitly reconstructed in the other frame, to avoid cross-frame limitations
1. Whenever the device mode emulation frame needs to access information that is available in `citizennotes_app` (such as settings), it needs to access the information in the correct frame.
For example, any information coming from localStorage has to be mediated via API's in `citizennotes_app`.
Typically, these API's live in [front_end/panels/emulation/AdvancedApp.ts](../../panels/emulation/AdvancedApp.ts)
