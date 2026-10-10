# QtExtKeySequenceEdit regression tests

These tests compile the production widget directly against Qt6 Widgets and Test.
They cover bare Backspace clearing, modified Backspace recording, unchanged
Delete/letter recording, a second chord, and the recording timer.

From the repository root, on a machine with CMake and Qt6 development packages:

```sh
cmake -S tests/keysequenceedit -B build-keysequenceedit
cmake --build build-keysequenceedit
ctest --test-dir build-keysequenceedit --output-on-failure
```

The CTest entry uses the offscreen platform, so it does not require a desktop.
To exercise the platform window backend, run the built `tst_keysequenceedit`
executable directly in a desktop session without `QT_QPA_PLATFORM=offscreen`.

These focused tests do not launch NekoBox, persist its shortcut configuration,
trigger its profile/group delete actions, or register global hotkeys.
