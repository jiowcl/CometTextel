# Platform-native shared libraries for wheels

Filled by `scripts/stage_native.py` before `python -m build --wheel`:

| Directory | Contents |
|-----------|----------|
| `win_amd64/` | `comettextel.dll` |
| `linux_x86_64/` | `libcomettextel.so` |

Do not commit binaries. Source / editable installs still use `COMETTEXTEL_LIB` or nearby CMake / C SDK artifacts.
