/**
 * @file modem_profiles.cpp
 * @brief Profile table, detection, and public name helper.
 */

#include "comettextel/modem_profile.hpp"

#include "modem/detail/profile.hpp"

#include <cctype>
#include <cstring>

namespace comettextel::detail::modem {

namespace {

/**
 * @brief Generic CNMI sequence.
 */
constexpr const char* kGenericCnmi[] = {
    "AT+CNMI=2,1,0,1,0\r",
    "AT+CNMI=1,1,0,1,0\r",
    "AT+CNMI=2,0,0,1,0\r",
};

/**
 * @brief Quectel CNMI sequence.
 */
constexpr const char* kQuectelCnmi[] = {
    "AT+CNMI=2,1,0,2,1\r",
    "AT+CNMI=2,1,0,1,0\r",
    "AT+CNMI=1,1,0,1,0\r",
    "AT+CNMI=2,0,0,1,0\r",
};

/**
 * @brief Simcom manufacturer tokens.
 */
constexpr const char* kSimcomMfg[] = {"SIMCOM", "SIMCOM_LTD"};

/**
 * @brief Simcom model tokens.
 */
constexpr const char* kSimcomModel[] = {"SIM8", "SIM9", "A76", "SIM5"};

/**
 * @brief Quectel manufacturer tokens.
 */
constexpr const char* kQuectelMfg[] = {"QUECTEL"};

/**
 * @brief Quectel model tokens.
 */
constexpr const char* kQuectelModel[] = {"EC", "BC", "BG", "M26", "M95", "UC"};

/**
 * @brief Simcom CPMS sequence.
 */
constexpr const char* kSimcomCpms[] = {
    "AT+CPMS=\"SM\",\"SM\",\"SM\"\r",
    "AT+CPMS=\"ME\",\"ME\",\"ME\"\r",
};

/**
 * @brief Quectel CPMS sequence.
 */
constexpr const char* kQuectelCpms[] = {
    "AT+CPMS=\"ME\",\"ME\",\"ME\"\r",
    "AT+CPMS=\"SM\",\"SM\",\"SM\"\r",
    "AT+CPMS=\"MT\",\"MT\",\"MT\"\r",
};

/**
 * @brief Profiles table.
 */
const ModemProfile kProfiles[] = {
    {
        ModemProfileId::Generic,
        "generic",
        kGenericCnmi,
        sizeof(kGenericCnmi) / sizeof(kGenericCnmi[0]),
        "AT+CMGL\r",
        nullptr,
        0,
        nullptr,
        0,
        nullptr,
        0,
    },
    {
        ModemProfileId::Simcom,
        "simcom",
        kGenericCnmi,
        sizeof(kGenericCnmi) / sizeof(kGenericCnmi[0]),
        "AT+CMGL=4\r",
        kSimcomCpms,
        sizeof(kSimcomCpms) / sizeof(kSimcomCpms[0]),
        nullptr,
        0,
        kSimcomMfg,
        sizeof(kSimcomMfg) / sizeof(kSimcomMfg[0]),
        kSimcomModel,
        sizeof(kSimcomModel) / sizeof(kSimcomModel[0]),
    },
    {
        ModemProfileId::Quectel,
        "quectel",
        kQuectelCnmi,
        sizeof(kQuectelCnmi) / sizeof(kQuectelCnmi[0]),
        "AT+CMGL=4\r",
        kQuectelCpms,
        sizeof(kQuectelCpms) / sizeof(kQuectelCpms[0]),
        nullptr,
        0,
        kQuectelMfg,
        sizeof(kQuectelMfg) / sizeof(kQuectelMfg[0]),
        kQuectelModel,
        sizeof(kQuectelModel) / sizeof(kQuectelModel[0]),
    },
};

/**
 * @brief Matches any of the tokens.
 * @param text The text to match.
 * @param tokens The tokens to match.
 * @param count The number of tokens.
 * @return True if any of the tokens match.
 */
[[nodiscard]] bool matches_any(std::string_view text,
                               const char* const* tokens,
                               std::size_t count) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        if (view_iequals_ascii(text, tokens[i])) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Contains the token ASCII.
 * @param haystack The haystack to search.
 * @param needle The needle to search.
 * @return True if the needle is found in the haystack.
 */
[[nodiscard]] bool contains_token_ascii(std::string_view haystack, std::string_view needle) noexcept
{
    if (needle.empty()) {
        return true;
    }
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const unsigned char a = static_cast<unsigned char>(haystack[i + j]);
            const unsigned char b = static_cast<unsigned char>(needle[j]);
            if (std::toupper(a) != std::toupper(b)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Matches any of the substrings.
 * @param text The text to match.
 * @param tokens The substrings to match.
 * @param count The number of substrings.
 * @return True if any of the substrings match.
 */
[[nodiscard]] bool matches_any_substr(std::string_view text,
                                      const char* const* tokens,
                                      std::size_t count) noexcept
{
    for (std::size_t i = 0; i < count; ++i) {
        if (contains_token_ascii(text, tokens[i])) {
            return true;
        }
    }
    return false;
}

} // namespace

/**
 * @brief Compares two strings ignoring case.
 * @param haystack The haystack to search.
 * @param needle The needle to search.
 * @return True if the haystack and needle are equal ignoring case.
 */
bool view_iequals_ascii(std::string_view haystack, std::string_view needle) noexcept
{
    if (haystack.size() != needle.size()) {
        return false;
    }
    for (std::size_t i = 0; i < haystack.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(haystack[i]);
        const unsigned char b = static_cast<unsigned char>(needle[i]);
        if (std::toupper(a) != std::toupper(b)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Gets the first response line.
 * @param response The response to get the first line from.
 * @return The first response line.
 */
std::string_view first_response_line(std::string_view response) noexcept
{
    std::size_t pos = 0;
    while (pos < response.size() && (response[pos] == '\r' || response[pos] == '\n')) {
        ++pos;
    }
    if (pos >= response.size()) {
        return {};
    }
    const std::size_t end = response.find_first_of("\r\n", pos);
    if (end == std::string_view::npos) {
        return response.substr(pos);
    }
    return response.substr(pos, end - pos);
}

/**
 * @brief Gets the profile for the given ID.
 * @param id The ID of the profile to get.
 * @return The profile.
 */
const ModemProfile& profile_for(const ModemProfileId id) noexcept
{
    for (const ModemProfile& profile : kProfiles) {
        if (profile.id == id) {
            return profile;
        }
    }
    return kProfiles[0];
}

/**
 * @brief Detects the profile for the given manufacturer and model.
 * @param manufacturer The manufacturer to detect the profile for.
 * @param model The model to detect the profile for.
 * @return The profile ID.
 */
ModemProfileId detect_profile(const std::string_view manufacturer,
                              const std::string_view model) noexcept
{
    for (const ModemProfile& profile : kProfiles) {
        if (profile.id == ModemProfileId::Generic) {
            continue;
        }
        if (profile.model_match_count > 0 &&
            matches_any_substr(model, profile.model_match, profile.model_match_count)) {
            return profile.id;
        }
    }
    for (const ModemProfile& profile : kProfiles) {
        if (profile.id == ModemProfileId::Generic) {
            continue;
        }
        if (profile.manufacturer_match_count > 0 &&
            (matches_any(manufacturer, profile.manufacturer_match, profile.manufacturer_match_count) ||
             matches_any_substr(manufacturer, profile.manufacturer_match, profile.manufacturer_match_count))) {
            return profile.id;
        }
        if (profile.manufacturer_match_count > 0 &&
            matches_any_substr(model, profile.manufacturer_match, profile.manufacturer_match_count)) {
            return profile.id;
        }
    }
    return ModemProfileId::Generic;
}

} // namespace comettextel::detail::modem

namespace comettextel {

/**
 * @brief Gets the name of the modem profile.
 * @param id The ID of the profile to get the name of.
 * @return The name of the profile.
 */
const char* modem_profile_name(const ModemProfileId id) noexcept
{
    if (id == ModemProfileId::Auto) {
        return "auto";
    }
    return detail::modem::profile_for(id).display_name;
}

} // namespace comettextel