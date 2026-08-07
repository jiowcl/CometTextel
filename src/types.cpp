/**
 * @file types.cpp
 * @brief Error category implementation.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/types.hpp"

namespace comettextel {

/**
 * @brief Get the name of the error category.
 * @return The name of the error category.
 */
const char* ErrorCategory::name() const noexcept
{
    return "comettextel";
}

/**
 * @brief Get the error message for the given error condition.
 * @param condition The error condition.
 * @return The error message for the given error condition.
 */
std::string ErrorCategory::message(int condition) const
{
    switch (static_cast<Errc>(condition)) {
    case Errc::Ok:
        return "success";
    case Errc::InvalidArgument:
        return "invalid argument";
    case Errc::NotOpen:
        return "serial port is not open";
    case Errc::AlreadyOpen:
        return "serial port is already open";
    case Errc::IoFailure:
        return "I/O failure";
    case Errc::Timeout:
        return "operation timed out";
    case Errc::ModemRejected:
        return "modem rejected the command";
    case Errc::EncodeFailure:
        return "PDU encode failure";
    case Errc::DecodeFailure:
        return "PDU decode failure";
    case Errc::Unsupported:
        return "unsupported operation on this platform";
    }

    return "unknown comettextel error";
}

/**
 * @brief Get the error category for the comettextel library.
 * @return The error category for the comettextel library.
 */
const std::error_category& error_category() noexcept
{
    static const ErrorCategory category;
    
    return category;
}

} // namespace comettextel
