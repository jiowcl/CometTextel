# CometTextel — Python (thin FFI)

Thin **ctypes** binding to the same C ABI as the C SDK and CometTextel.NET (`c_api.h`).  
This is **not** a second native library: it runtime-loads `comettextel.dll` / `libcomettextel.so`.

Covers **PDU** helpers and **GsmModem** (`open` / `send` / `list` / `delete`).  
Hardware modem tests are optional; smoke tests run without a device.

![Python](https://img.shields.io/badge/language-python-blue.svg)

## Layout

```text
sdk/python/
├── comettextel/              # package (ctypes + PDU + modem)
├── examples/
│   ├── pdu_example.py        # encode / decode + self-check
│   └── modem_example.py      # list / send / delete
├── tests/
│   ├── test_pdu.py
│   └── test_modem.py
└── README.md
```

## Prerequisites

| Item | Detail |
|------|--------|
| Python | 3.10+ (3.13 verified) |
| Native | C SDK shared library (`comettextel.dll` or `libcomettextel.so`) |
| Modem (optional) | AT modem in **PDU mode** (e.g. `COM3`, `/dev/ttyUSB0`) |

Download `comettextel-c-sdk-*` from CI Artifacts or a GitHub Release, or use a local CMake build.

### Finding the library

Search order:

1. Explicit path passed to `comettextel.pdu.load_library(...)`
2. Environment variable **`COMETTEXTEL_LIB`** (shared-library **file** or a directory containing it — including versioned Linux sonames such as `libcomettextel.so.1.3.0`)
3. Current working directory
4. Nearby build / artifact folders (`build-c-sdk/Release`, `artifact/comettextel-c-sdk-*/…`)

```powershell
# Windows
$env:COMETTEXTEL_LIB = "D:\path\to\comettextel.dll"
```

```bash
# Linux
export COMETTEXTEL_LIB=/path/to/libcomettextel.so
```

## PDU example

```powershell
cd sdk\python
python examples\pdu_example.py
python examples\pdu_example.py 886912345678 "Hello" 886932000000
python examples\pdu_example.py 886912345678 "測試中文簡訊" 886932000000
```

## Modem example

```powershell
python examples\modem_example.py list COM3
python examples\modem_example.py send COM3 886932000000 886912345678 "Hello"
python examples\modem_example.py delete COM3 1
```

`list` rejoins **complete** concatenated SMS sets (`concat_seq == 0` / `Message.is_reassembled_concat`).

## Tests

```powershell
cd sdk\python
pip install pytest
pytest -q
```

## API sketch

```python
from comettextel import DCS_UCS2, GsmModem, decode, encode_submit_segments

parts = encode_submit_segments("886912345678", "Hello", "886932000000", DCS_UCS2)
msg = decode(parts[0])

with GsmModem() as modem:
    modem.open("COM3", 115200)
    modem.send("886912345678", "Hello from Python", smsc="886932000000")
    for m in modem.list():
        print(m.index, m.peer_address, m.user_data, m.is_reassembled_concat)
```

All text at the native boundary is **UTF-8**. Do not reimplement PDU codecs in Python; if the C ABI changes, update this package only.

## See also

- [`include/comettextel/c_api.h`](../../include/comettextel/c_api.h)
- [`sdk/c/README.md`](../c/README.md)
- [`sdk/freebasic/README.md`](../freebasic/README.md)
- [`sdk/purebasic/README.md`](../purebasic/README.md)
- [`examples/c_api_example.c`](../../examples/c_api_example.c)
