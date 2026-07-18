# Vendored library patches

`apply_patches.py` is a PlatformIO **pre-build** hook (`extra_scripts = pre:patches/apply_patches.py`
in `platformio.ini`) that overwrites files inside downloaded libraries with our patched copies before
each build. This keeps the patch in version control and independent of the `.pio/libdeps` cache (which
is git-ignored and wiped on library reinstall).

Carried over from the companion Shutter Hub project, which hit and fixed this on real hardware — see
[../../docs/decisions/0005-apple-homekit-homespan.md](../../docs/decisions/0005-apple-homekit-homespan.md).

## HomeSpan.cpp — newlib-nano `%m` sscanf hang

**Library:** `homespan/HomeSpan @ ~1.9.1` (pinned — see the HomeKit ADR).

**Symptom:** with the HomeKit bridge enabled, the device is never discoverable in Apple Home and
manual codes drop instantly. HomeSpan's poll task reaches "Device not yet Paired" and then does
nothing — no `_hap._tcp`, port 1201 closed, connect callback never fires. No crash/reboot.

**Root cause:** `Span::checkConnect()` validates the mDNS hostname with

```c
char *d;
sscanf(hostName, "%m[A-Za-z0-9-]", &d);
if (... || strlen(hostName) != strlen(d)) { LOG0("PROGRAM HALTED!"); while(1); }
```

The `%m` allocating conversion is a POSIX/glibc extension that **newlib-nano does not implement**, so
`d` is never populated, the length check is falsely true, and HomeSpan hits its `while(1)`. Any valid
hostname trips this — it isn't specific to this device's hostname.

**Patch:** `patches/HomeSpan.cpp` replaces the `%m` sscanf + validation with an equivalent nano-safe
character loop (search for `[PATCH]`). Everything else is upstream 1.9.1 verbatim.

### Refreshing this patch after a HomeSpan version bump
1. Build once so PlatformIO downloads the new HomeSpan into `.pio/libdeps/<env>/HomeSpan/`.
2. Diff `patches/HomeSpan.cpp` against the new upstream `src/HomeSpan.cpp`.
3. Re-apply the `[PATCH]` hunk, copy the result back to `patches/HomeSpan.cpp`.
