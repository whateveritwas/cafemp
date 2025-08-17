#include <time.h> // For struct tm and time_t

// A simplified implementation of timegm for WiiU to address
// pdf-parse.c:(.text.pdf_parse_date+0x168): undefined reference to `timegm'

time_t timegm(struct tm *tm)
{
    // The core logic for converting struct tm to time_t in UTC.
    // This is a common non-standard function, so it's often reimplemented.

    int year = tm->tm_year + 1900;
    int month = tm->tm_mon; // 0-11
    int day = tm->tm_mday;   // 1-31
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;

    // Days in months (non-leap year)
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    long total_days = 0;

    // Add days for full years since epoch (1970)
    for (int y = 1970; y < year; y++) {
        total_days += ( ( (y % 4 == 0) && (y % 100 != 0) ) || (y % 400 == 0) ) ? 366 : 365;
    }

    // Add days for full months in the current year
    for (int m = 0; m < month; m++) {
        total_days += days_in_month[m];
    }
    // Adjust for leap year if current year is leap and month is after February
    if (month > 1 && ( ( (year % 4 == 0) && (year % 100 != 0) ) || (year % 400 == 0) ) ) {
        total_days += 1;
    }

    // Add days for the current month
    total_days += (day - 1); // tm_mday is 1-based

    time_t result = (time_t)(total_days * 86400L + hour * 3600L + min * 60L + sec);
    return result;
}
