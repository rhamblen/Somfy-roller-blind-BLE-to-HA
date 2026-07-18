# PlatformIO pre-build hook — apply vendored patches to third-party libraries.
#
# HomeSpan 1.9.1: Span::checkConnect() validates its mDNS hostname with
#   sscanf(hostName,"%m[A-Za-z0-9-]",&d)
# but newlib-nano (this arduino-esp32 toolchain) does NOT implement the "%m" allocating
# conversion, so `d` is left unset, the self-check falsely fails, and HomeSpan hits its
# `while(1)` PROGRAM HALTED *on the poll task* — before it advertises _hap._tcp or starts the
# HAP server on port 1201. Result: the bridge is never discoverable or pairable (the v0.7.0
# HomeKit bug). Our patched copy (patches/HomeSpan.cpp) replaces that check with a nano-safe
# character loop. We overwrite the library's file with ours before every build.
#
# Library version is pinned (homespan/HomeSpan @ ~1.9.1 in platformio.ini); if that pin changes,
# refresh patches/HomeSpan.cpp from the new upstream + re-apply the fix (see patches/README.md).
Import("env")  # noqa: F821
import os
import shutil
import filecmp

proj    = env.subst("$PROJECT_DIR")
libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
pioenv  = env["PIOENV"]

patched = os.path.join(proj, "patches", "HomeSpan.cpp")
target  = os.path.join(libdeps, pioenv, "HomeSpan", "src", "HomeSpan.cpp")

if not os.path.isfile(patched):
    print("[patch_homespan] SKIP: patched source missing at %s" % patched)
elif not os.path.isfile(target):
    # Library not installed yet on the very first configure pass; it will be applied on the
    # build pass once HomeSpan is downloaded.
    print("[patch_homespan] SKIP: HomeSpan not installed yet for env '%s'" % pioenv)
elif filecmp.cmp(patched, target, shallow=False):
    print("[patch_homespan] HomeSpan.cpp already patched for env '%s'" % pioenv)
else:
    shutil.copyfile(patched, target)
    print("[patch_homespan] applied patched HomeSpan.cpp for env '%s'" % pioenv)
