# PlatformIO pre-build hook: regenerate src/web_ui_bundle.h from html/v2
# (see tools/embed-ui.py) so `pio run` embeds the same UI build.sh does.
import os
import subprocess
import sys

Import("env")  # noqa: F821 - provided by PlatformIO

root = env.subst("$PROJECT_DIR")  # noqa: F821
subprocess.check_call([sys.executable, os.path.join(root, "tools", "embed-ui.py")], cwd=root)
