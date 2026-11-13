/**
 * @file cmgl.hpp
 * @brief +CMGL response parsing (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include "comettextel/types.hpp"

#include <vector>

namespace comettextel::detail::modem::cmgl {

/**
 * @brief Parses @c +CMGL entries and reassembles concat sets.
 * @param buffer The response buffer.
 * @return The parsed messages.
 */
[[nodiscard]] std::vector<Message> parse_list(const ResponseBuffer& buffer);

} // namespace comettextel::detail::modem::cmgl
