# Forces pioarduino to rebuild the shared Arduino framework libraries when they are not this
# board's. A custom_sdkconfig build compiles the libraries into the SHARED
# framework-arduinoespressif32-libs package and copies the project's sdkconfig.<env> over
# <chip>/sdkconfig. pioarduino decides whether to rebuild from the stamp in the PROJECT's own
# sdkconfig.defaults only, so after another board's rebuild (or a package reinstall) it links
# whatever the package holds, silently. If the package's sdkconfig is not this project's,
# deleting the project's two generated files makes pioarduino rebuild for this board.
# Run as pre: so it acts before the framework script reads the stamp.
import os

Import("env")


def _same_file(a, b):
    if not (os.path.isfile(a) and os.path.isfile(b)):
        return False
    if os.path.getsize(a) != os.path.getsize(b):
        return False
    with open(a, "rb") as fa, open(b, "rb") as fb:
        return fa.read() == fb.read()


def _guard():
    if not env.GetProjectOption("custom_sdkconfig", ""):
        return

    project_dir = env.subst("$PROJECT_DIR")
    pioenv = env.subst("$PIOENV")
    defaults = os.path.join(project_dir, "sdkconfig.defaults")
    project_cfg = os.path.join(project_dir, "sdkconfig." + pioenv)
    if not os.path.isfile(defaults):
        return  # no stamp: pioarduino rebuilds by itself

    libs_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs")
    board = env.BoardConfig()
    chip = (board.get("build.chip_variant", "") or board.get("build.mcu", "")).lower()
    shared_cfg = os.path.join(libs_dir or "", chip, "sdkconfig")

    if _same_file(project_cfg, shared_cfg):
        return

    print("*** framework_libs_guard: the shared %s libraries were not built for %s; "
          "forcing a rebuild (~15-20 min) ***" % (chip, pioenv))
    for path in (defaults, project_cfg):
        if os.path.isfile(path):
            os.remove(path)


_guard()
