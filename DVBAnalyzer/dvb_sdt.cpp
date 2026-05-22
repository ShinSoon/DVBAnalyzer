#include "dvb_sdt.h"
#include <iostream>

void ServiceInfo::print() const {
    std::cout << "    Service ID: " << service_id;
    std::cout << ", Name: \"" << service_name << "\"";
    std::cout << ", Provider: \"" << provider_name << "\"";
    std::cout << ", Type: 0x" << std::hex << static_cast<int>(service_type) << std::dec;
    std::cout << ", Running: " << static_cast<int>(running_status);
    std::cout << ", FreeCA: " << (free_ca_mode ? "Yes" : "No");
    std::cout << ", EIT[p/f]: " << (eit_present_following_flag ? "Yes" : "No");
    std::cout << ", EIT[sched]: " << (eit_schedule_flag ? "Yes" : "No");
    std::cout << std::endl;
}