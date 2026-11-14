/**
 * @file modem_profile.hpp
 * @brief Vendor modem profiles for AT-command quirks (CNMI, list, init).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include "comettextel/export.hpp"

#include <cstdint>

namespace comettextel {

/**
 * @brief Built-in modem vendor profiles.
 *
 * @ref ModemProfileId::Generic matches pre-1.7.2 behavior. @ref ModemProfileId::Auto
 * queries @c AT+CGMI / @c AT+CGMM during @ref GsmModem::initialize and picks the
 * best match (falls back to Generic).
 */
enum class ModemProfileId : std::uint8_t {
    Generic = 0, ///< Default CNMI / @c AT+CMGL sequence
    Simcom,      ///< SIMCom modules (e.g. SIM800 series)
    Quectel,     ///< Quectel cellular modules
    Auto,        ///< Detect from @c CGMI+CGMM on initialize
};

/**
 * @brief Human-readable profile label for logging.
 * @param id Profile id (unknown values map to @c "generic").
 */
[[nodiscard]] COMETTEXTEL_API const char* modem_profile_name(ModemProfileId id) noexcept;

} // namespace comettextel