# PlatformIO pre-build script: fixes a Mozzi bug where MonoOutput::portable()
# is defined in AudioOutput.h without `inline` (stereo mode only), causing
# "multiple definition" link errors when more than one .cpp includes Mozzi.
Import("env")
import os

OLD = "StereoOutput MonoOutput::portable() const {"
NEW = "inline StereoOutput MonoOutput::portable() const {"

libdeps = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))
header = os.path.join(libdeps, "Mozzi", "AudioOutput.h")

if os.path.isfile(header):
    with open(header, "r", encoding="utf-8") as f:
        src = f.read()
    if NEW not in src and OLD in src:
        with open(header, "w", encoding="utf-8") as f:
            f.write(src.replace(OLD, NEW, 1))
        print("patch_mozzi: added inline to MonoOutput::portable()")
else:
    print("patch_mozzi: Mozzi AudioOutput.h not found (libdeps not installed yet?)")
