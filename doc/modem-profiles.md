# Modem profiles  

Vendor profiles centralize optional AT-command sequences that differ by module family. Public API: `ModemProfileId`, `GsmModem::set_modem_profile`, and read-back fields after `initialize()`.

## Profiles

| ID | When to use | CNMI / notes |
|----|-------------|--------------|
| `Generic` (default) | Unknown or standard 27.005 modems | Same as pre-1.7.2: `2,1,0,1,0` then fallbacks |
| `Simcom` | SIMCom (e.g. SIM800); auto-match on `CGMI` / model prefix `SIM8` | Same CNMI as Generic today |
| `Quectel` | Quectel EC/BG/M95…; auto-match on manufacturer or `EC*` models | Tries `2,1,0,2,1` first, then Generic fallbacks |
| `Auto` | Let library query `AT+CGMI` + `AT+CGMM` during `initialize` | Picks Simcom / Quectel / Generic |

List messages uses `AT+CMGL` for all built-in profiles today.

## Usage (C++)

```cpp
comettextel::GsmModem modem;
modem.set_modem_profile(comettextel::ModemProfileId::Auto);
std::error_code ec = modem.open_and_init("COM3");
// modem.resolved_modem_profile(), modem_manufacturer(), modem_model()
```

Set the profile **before** `initialize()` / `open_and_init()`.

## Usage (C ABI v4)

```c
ct_modem *m = ct_modem_create();
ct_modem_set_profile(m, CT_MODEM_PROFILE_AUTO);
ct_modem_open(m, "COM3", 115200);
int resolved = 0;
ct_modem_get_resolved_profile(m, &resolved);
char mfg[64], model[64];
ct_modem_get_identity(m, mfg, (int)sizeof mfg, model, (int)sizeof model);
```

## Contributing a profile  

1. Capture `AT+CGMI`, `AT+CGMR`, `AT+CGMM`, and a failing command transcript.
2. Add match tokens and CNMI / post-init rows in `src/modem/modem_profiles.cpp`.
3. Add a scripted test in `tests/modem_integration_test.cpp`.
4. Document quirks in this file.

See [architecture.md](architecture.md) for source layout.