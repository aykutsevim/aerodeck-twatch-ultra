"""AeroDeck: inject AD_FW_VERSION from `git describe` (pre: script)."""
import subprocess

Import("env")  # noqa: F821

try:
    version = subprocess.check_output(
        ["git", "describe", "--always", "--dirty"],
        cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
        stderr=subprocess.DEVNULL,
    ).decode().strip() or "dev"
except Exception:
    version = "dev"

env.Append(CPPDEFINES=[("AD_FW_VERSION", '\\"%s\\"' % version)])  # noqa: F821
print("fw_version: " + version)
