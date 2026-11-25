#include "quiet_hours.h"

bool quiet_hours_active(int hour, bool on, uint8_t from, uint8_t to)
{
    if (!on || hour < 0 || from == to) return false;
    if (from < to) return hour >= from && hour < to;
    return hour >= from || hour < to; /* cruza la medianoche */
}
