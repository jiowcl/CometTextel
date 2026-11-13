/**
 * @file urc.hpp
 * @brief Unsolicited result code demultiplexing (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include "comettextel/types.hpp"

#include <string>
#include <vector>

namespace comettextel::detail::modem::urc {

/**
 * @brief Strips recognized URC blocks from @p stream and decodes +CDS status reports.
 * @details Incomplete trailing URC fragments remain in @p stream.
 * @param stream The string to demux.
 * @param out_status_reports The vector to store the status reports in.
 */
void demux(std::string& stream, std::vector<Message>& out_status_reports);

} // namespace comettextel::detail::modem::urc
