# Modem profiles  

Vendor profiles centralize optional AT-command sequences that differ by module family. Public API: `ModemProfileId`, `GsmModem::set_modem_profile`, and read-back fields after `initialize()`.

## Profiles

| ID | When to use | CNMI / storage / list |
|----|-------------|------------------------|
| `Generic` (default) | Unknown or standard 27.005 modems | CNMI fallbacks; no `CPMS`; `AT+CMGL` |
| `Simcom` | SIMCom (e.g. SIM800); auto-match on `CGMI` / `SIM8`… | Same CNMI as Generic; `CPMS` SM then ME; `AT+CMGL=4` |
| `Quectel` | Quectel EC/BG/M95… | Quectel CNMI; `CPMS` ME/SM/MT; `AT+CMGL=4` |
| `Auto` | Query `AT+CGMI` + `AT+CGMM` on init | Picks Simcom / Quectel / Generic |

`AT+CPMS` runs during `initialize` (first OK wins). Listing uses the profile's
`AT+CMGL` / `AT+CMGL=4` command via `request_message_list()`.

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