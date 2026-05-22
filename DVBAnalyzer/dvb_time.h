#ifndef DVB_TIME_H
#define DVB_TIME_H

#include <cstdint>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------------------------
// Portable, timezone-correct UTC time helpers.
//
// DVB transmits all SI times in UTC (ETSI EN 300 468). The original text path
// used mktime()/localtime(), which silently apply the machine's local timezone
// and corrupt the timestamps. Everything here works purely in UTC so the text
// path and the binary path agree.
// ---------------------------------------------------------------------------

// Days from 1970-01-01 to the given proleptic-Gregorian civil date.
// Howard Hinnant's well-known branch-free algorithm.
inline long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);            // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    return era * 146097LL + static_cast<long long>(doe) - 719468;
}

inline long long makeUtcEpochMillis(int year, unsigned month, unsigned day,
                                    unsigned hour, unsigned minute, unsigned second) {
    const long long days = daysFromCivil(year, month, day);
    const long long secs = days * 86400LL +
                           static_cast<long long>(hour) * 3600 +
                           static_cast<long long>(minute) * 60 +
                           static_cast<long long>(second);
    return secs * 1000LL;
}

// EIT start_time: 16-bit MJD followed by 24 bits of BCD UTC (HH MM SS) = 5 bytes.
// Decoding per ETSI EN 300 468 Annex C.
inline long long mjdBcdToEpochMillis(const uint8_t* p) {
    const int mjd = (p[0] << 8) | p[1];
    const int yp = static_cast<int>((mjd - 15078.2) / 365.25);
    const int mp = static_cast<int>((mjd - 14956.1 - static_cast<int>(yp * 365.25)) / 30.6001);
    const int day = mjd - 14956 - static_cast<int>(yp * 365.25) - static_cast<int>(mp * 30.6001);
    const int k = (mp == 14 || mp == 15) ? 1 : 0;
    const int year = yp + k + 1900;
    const int month = mp - 1 - k * 12;

    auto bcd = [](uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); };
    return makeUtcEpochMillis(year, static_cast<unsigned>(month), static_cast<unsigned>(day),
                              bcd(p[2]), bcd(p[3]), bcd(p[4]));
}

// EIT duration: 24 bits of BCD (HH MM SS) = 3 bytes.
inline long long bcdDurationToSeconds(const uint8_t* p) {
    auto bcd = [](uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); };
    return static_cast<long long>(bcd(p[0])) * 3600 + bcd(p[1]) * 60 + bcd(p[2]);
}

// --- Inverse direction, used by the TS generator to encode test streams ---

inline void epochMillisToMjdBcd(long long millis, uint8_t out[5]) {
    const long long secs = millis / 1000;
    const long long days = secs / 86400;
    const long long rem = secs % 86400;
    const int mjd = static_cast<int>(days + 40587);  // 1970-01-01 == MJD 40587
    out[0] = static_cast<uint8_t>((mjd >> 8) & 0xFF);
    out[1] = static_cast<uint8_t>(mjd & 0xFF);
    auto toBcd = [](int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); };
    out[2] = toBcd(static_cast<int>(rem / 3600));
    out[3] = toBcd(static_cast<int>((rem % 3600) / 60));
    out[4] = toBcd(static_cast<int>(rem % 60));
}

inline void secondsToBcdDuration(long long sec, uint8_t out[3]) {
    auto toBcd = [](int v) { return static_cast<uint8_t>(((v / 10) << 4) | (v % 10)); };
    out[0] = toBcd(static_cast<int>(sec / 3600));
    out[1] = toBcd(static_cast<int>((sec % 3600) / 60));
    out[2] = toBcd(static_cast<int>(sec % 60));
}

// Format epoch milliseconds as a human-readable UTC string.
inline std::string formatUtcTime(long long timeMillis) {
    if (timeMillis <= 0) return "N/A";
    const std::time_t t = static_cast<std::time_t>(timeMillis / 1000);
    std::tm tmBuf{};
#ifdef _MSC_VER
    if (gmtime_s(&tmBuf, &t) != 0) return "Time Error";
#else
    if (gmtime_r(&t, &tmBuf) == nullptr) return "Time Error";
#endif
    std::ostringstream ss;
    ss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << " UTC";
    return ss.str();
}

#endif // DVB_TIME_H
