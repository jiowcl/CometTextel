/**
 * @file profile.hpp
 * @brief Internal modem profile table and detection.
 */

#pragma once

#include "comettextel/modem_profile.hpp"

#include <cstddef>
#include <string_view>

namespace comettextel::detail::modem {

/**
 * @brief Modem profile structure.
 */
struct ModemProfile {
    ModemProfileId id;
    const char* display_name;
    const char* const* cnmi_commands;
    std::size_t cnmi_command_count;
    const char* list_messages_command;
    const char* const* cpms_commands;
    std::size_t cpms_command_count;
    const char* const* post_init_commands;
    std::size_t post_init_command_count;
    const char* const* manufacturer_match;
    std::size_t manufacturer_match_count;
    const char* const* model_match;
    std::size_t model_match_count;
};

/**
 * @brief Gets the profile for the given ID.
 * @param id The ID of the profile to get.
 * @return The profile.
 */
[[nodiscard]] const ModemProfile& profile_for(ModemProfileId id) noexcept;

/**
 * @brief Detects the profile for the given manufacturer and model.
 * @param manufacturer The manufacturer to detect the profile for.
 * @param model The model to detect the profile for.
 * @return The profile ID.
 */
[[nodiscard]] ModemProfileId detect_profile(std::string_view manufacturer,
                                            std::string_view model) noexcept;

/**
 * @brief Compares two strings ignoring case.
 * @param haystack The haystack to search.
 * @param needle The needle to search.
 * @return True if the haystack and needle are equal ignoring case.
 */
[[nodiscard]] bool view_iequals_ascii(std::string_view haystack, std::string_view needle) noexcept;

/**
 * @brief Gets the first response line.
 * @param response The response to get the first line from.
 * @return The first response line.
 */
[[nodiscard]] std::string_view first_response_line(std::string_view response) noexcept;

} // namespace comettextel::detail::modem