# CometTextel — Python (thin FFI)

Thin **ctypes** binding to the same C ABI as the C SDK and CometTextel.NET (`c_api.h`).  
This is **not** a second native library: it runtime-loads `comettextel.dll` / `libcomettextel.so`.

**PDU only** in this sample (no modem yet). Tests run when the shared library is available; they skip otherwise.

![Python](https://img.shields.io/badge/language-python-blue.svg)

## Layout

```text
sdk/python/
├── comettextel/          # package (ctypes + PDU helpers)
├── examples/
│   └── pdu_example.py    # encode / decode console sample + self-check
├── tests/
│   └── test_pdu.py
└── README.md
```

## Prerequisites

| Item | Detail |
|------|--------|
| Python | 3.10+ (3.13 verified) |
| Native | C SDK shared library (`comettextel.dll` or `libcomettextel.so`) |

Download `comettextel-c-sdk-*` from CI Artifacts or a GitHub Release, or use a local CMake build.

### Finding the library

Search order:

1. Explicit path passed to `comettextel.pdu.load_library(...)`
2. Environment variable **`COMETTEXTEL_LIB`** (file or directory)
3. Current working directory
4. Nearby build / artifact folders (`build-c-sdk/Release`, `artifact/comettextel-c-sdk-*/…`)

```powershell
# Windows
$env:COMETTEXTEL_LIB = "D:\path\to\comettextel.dll"
# or directory containing the DLL:
$env:COMETTEXTEL_LIB = "D:\path\to\bin"
```

```bash
# Linux
export COMETTEXTEL_LIB=/path/to/libcomettextel.so
```

## Run the example

```powershell
cd sdk\python
python examples\pdu_example.py
python examples\pdu_example.py 886912345678 "Hello" 886932000000
python examples\pdu_example.py 886912345678 "測試中文簡訊" 886932000000
```

No arguments runs a self-check:

- UCS-2 ASCII round-trip
- UCS-2 Chinese round-trip (UTF-8 source; expects real CJK in PDU, not `FFFD`)
- UCS-2 concat (71× `B` → 2 segments with UDH)
- Single-segment `ct_pdu_encode_submit`

## Tests

```powershell
cd sdk\python
pip install pytest
pytest -q
```

## API sketch

```python
from comettextel import DCS_UCS2, decode, encode_submit, encode_submit_segments

parts = encode_submit_segments("886912345678", "Hello", "886932000000", DCS_UCS2)
msg = decode(parts[0])
print(msg.peer_address, msg.user_data, msg.is_concatenated)
```

All text at the native boundary is **UTF-8**. Do not reimplement PDU codecs in Python; if the C ABI changes, update this package only.

## See also

- [`include/comettextel/c_api.h`](../../include/comettextel/c_api.h)
- [`sdk/c/README.md`](../c/README.md)
- [`sdk/purebasic/README.md`](../purebasic/README.md)
- [`examples/c_api_example.c`](../../examples/c_api_example.c)
