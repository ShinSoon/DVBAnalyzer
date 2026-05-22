#include "dvb_eit.h"
#include <iostream>

void EventInfo::print() const {
    std::cout << "      Event ID: " << event_id;
    std::cout << ", Name: \"" << event_name << "\"";
    std::cout << "\n        Start: " << formatTime(start_time_millis);
    std::cout << ", Duration (s): " << duration_seconds;
    std::cout << "\n        Desc: \"" << short_description << "\"" << std::endl;
}