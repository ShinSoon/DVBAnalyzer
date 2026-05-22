#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

#include "dvb_si_parser.h" // Includes all needed headers transitively
#include "ts_generator.h"

#ifdef _WIN32
#include <io.h>
#define ISATTY(fd) _isatty(fd)
#define FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define ISATTY(fd) isatty(fd)
#define FILENO(f) fileno(f)
#endif

namespace {

bool stdinIsInteractive() { return ISATTY(FILENO(stdin)) != 0; }

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

bool parseU16(const std::string& s, uint16_t& out) {
    try {
        const unsigned long v = std::stoul(s);
        if (v > 0xFFFFul) return false;
        out = static_cast<uint16_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

// Parse "<onid> <tsid>" starting at tokens[idx]. Returns false on bad input.
bool parseTs(const std::vector<std::string>& t, size_t idx, TransportStreamId& tsid) {
    return t.size() > idx + 1 &&
           parseU16(t[idx], tsid.original_network_id) &&
           parseU16(t[idx + 1], tsid.transport_stream_id);
}

void printUsage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <data_file>         Parse, then explore interactively.\n"
              << "                                   *.ts  -> binary MPEG-2 transport stream\n"
              << "                                   other -> pipe-delimited text format\n"
              << "  " << prog << " <data_file> --demo  Parse and print a full dump, then exit.\n"
              << "  " << prog << " --gen-ts <out.ts>   Generate a sample binary transport stream.\n";
}

void printHelp() {
    std::cout <<
        "Commands:\n"
        "  streams                          list transport streams (ONID/TSID)\n"
        "  summary                          list streams and their services\n"
        "  services <onid> <tsid>           list services in a transport stream\n"
        "  service  <onid> <tsid> <sid>     show one service's details\n"
        "  epg      <onid> <tsid> <sid>     scheduled events (EPG) for a service\n"
        "  pf       <onid> <tsid> <sid>     present/following events for a service\n"
        "  range    <onid> <tsid> <sid> <start> <end>\n"
        "                                   events overlapping a time window;\n"
        "                                   times as YYYY-MM-DDTHH:MM:SS (UTC)\n"
        "  help                             show this help\n"
        "  quit                             exit\n";
}

void printStreams(const DvbSiParser& parser) {
    const auto streams = parser.getTransportStreams();
    std::cout << "Transport streams (" << streams.size() << "):\n";
    for (const auto& ts : streams) {
        std::cout << "  ONID=" << ts.original_network_id
                  << " TSID=" << ts.transport_stream_id
                  << "  (" << parser.getServices(ts).size() << " service(s))\n";
    }
}

void printSummary(const DvbSiParser& parser) {
    for (const auto& ts : parser.getTransportStreams()) {
        std::cout << "ONID=" << ts.original_network_id
                  << " TSID=" << ts.transport_stream_id << ":\n";
        const auto services = parser.getServices(ts);
        if (services.empty()) {
            std::cout << "    (no services)\n";
        }
        for (const auto& svc : services) {
            std::cout << "    [" << svc.service_id << "] " << svc.service_name
                      << " (" << svc.provider_name << ")\n";
        }
    }
}

void cmdServices(const DvbSiParser& parser, const std::vector<std::string>& t) {
    TransportStreamId tsid;
    if (!parseTs(t, 1, tsid)) { std::cout << "usage: services <onid> <tsid>\n"; return; }
    const auto services = parser.getServices(tsid);
    if (services.empty()) { std::cout << "No services for that transport stream.\n"; return; }
    for (const auto& svc : services) svc.print();
}

void cmdService(const DvbSiParser& parser, const std::vector<std::string>& t) {
    TransportStreamId tsid;
    uint16_t sid = 0;
    if (!parseTs(t, 1, tsid) || t.size() < 4 || !parseU16(t[3], sid)) {
        std::cout << "usage: service <onid> <tsid> <serviceId>\n";
        return;
    }
    ServiceInfo info;
    if (parser.getServiceInfo(tsid, sid, info)) info.print();
    else std::cout << "Service " << sid << " not found.\n";
}

void cmdEvents(const DvbSiParser& parser, const std::vector<std::string>& t, bool scheduled) {
    TransportStreamId tsid;
    uint16_t sid = 0;
    if (!parseTs(t, 1, tsid) || t.size() < 4 || !parseU16(t[3], sid)) {
        std::cout << "usage: " << t[0] << " <onid> <tsid> <serviceId>\n";
        return;
    }
    const auto events = scheduled ? parser.getScheduledEvents(tsid, sid)
                                  : parser.getPresentFollowingEvents(tsid, sid);
    if (events.empty()) { std::cout << "No events.\n"; return; }
    for (const auto& evt : events) evt.print();
}

void cmdRange(const DvbSiParser& parser, const std::vector<std::string>& t) {
    TransportStreamId tsid;
    uint16_t sid = 0;
    if (!parseTs(t, 1, tsid) || t.size() < 6 || !parseU16(t[3], sid)) {
        std::cout << "usage: range <onid> <tsid> <serviceId> <start> <end>\n"
                     "       times as YYYY-MM-DDTHH:MM:SS (UTC)\n";
        return;
    }
    std::string startStr = t[4], endStr = t[5];
    std::replace(startStr.begin(), startStr.end(), 'T', ' ');
    std::replace(endStr.begin(), endStr.end(), 'T', ' ');
    const long long start = parseDateTimeString(startStr);
    const long long end = parseDateTimeString(endStr);
    if (start <= 0 || end <= 0) {
        std::cout << "Could not parse times. Use YYYY-MM-DDTHH:MM:SS.\n";
        return;
    }
    const auto events = parser.getEventsForTimeRange(tsid, sid, start, end);
    if (events.empty()) { std::cout << "No events in that range.\n"; return; }
    for (const auto& evt : events) evt.print();
}

int runInteractive(DvbSiParser& parser) {
    const bool tty = stdinIsInteractive();
    std::cout << "\n";
    printStreams(parser);
    if (tty) std::cout << "\nType 'help' for commands, 'quit' to exit.\n";

    std::string line;
    while (true) {
        if (tty) std::cout << "\ndvb> " << std::flush;
        if (!std::getline(std::cin, line)) break;  // EOF (Ctrl+Z / Ctrl+D)
        // Strip a leading UTF-8 BOM (some shells prepend one to piped input).
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        const auto t = tokenize(line);
        if (t.empty()) continue;
        const std::string& cmd = t[0];

        if (cmd == "quit" || cmd == "exit" || cmd == "q") break;
        else if (cmd == "help" || cmd == "h" || cmd == "?") printHelp();
        else if (cmd == "streams" || cmd == "ts") printStreams(parser);
        else if (cmd == "summary") printSummary(parser);
        else if (cmd == "services") cmdServices(parser, t);
        else if (cmd == "service") cmdService(parser, t);
        else if (cmd == "epg") cmdEvents(parser, t, /*scheduled=*/true);
        else if (cmd == "pf") cmdEvents(parser, t, /*scheduled=*/false);
        else if (cmd == "range") cmdRange(parser, t);
        else std::cout << "Unknown command: '" << cmd << "'. Type 'help'.\n";
    }
    return 0;
}

// Non-interactive full dump of everything parsed.
void runDemo(const DvbSiParser& parser) {
    for (const auto& ts : parser.getTransportStreams()) {
        std::cout << "\n=== ONID=" << ts.original_network_id
                  << " TSID=" << ts.transport_stream_id << " ===\n";
        for (const auto& svc : parser.getServices(ts)) {
            svc.print();
            const auto pf = parser.getPresentFollowingEvents(ts, svc.service_id);
            if (!pf.empty()) {
                std::cout << "    -- Present/Following --\n";
                for (const auto& evt : pf) evt.print();
            }
            const auto sched = parser.getScheduledEvents(ts, svc.service_id);
            if (!sched.empty()) {
                std::cout << "    -- Schedule (EPG) --\n";
                for (const auto& evt : sched) evt.print();
            }
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << "--- DVB SI Parser Simulator ---" << std::endl;

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string arg1 = argv[1];

    // Mode 1: generate a sample binary transport stream.
    if (arg1 == "--gen-ts") {
        if (argc != 3) { printUsage(argv[0]); return 1; }
        return generateSampleTs(argv[2]) ? 0 : 1;
    }

    // Mode 2/3: parse a data file, choosing the path by extension.
    DvbSiParser parser;
    const bool isBinary = endsWith(arg1, ".ts");
    const bool ok = isBinary ? parser.parseDataFromTsFile(arg1)
                             : parser.parseDataFromFile(arg1);
    if (!ok) {
        std::cerr << "Failed to parse data. Exiting." << std::endl;
        return 1;
    }

    // A trailing --demo flag dumps everything and exits (non-interactive).
    bool demo = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--demo") demo = true;
    }
    if (demo) {
        runDemo(parser);
        return 0;
    }

    return runInteractive(parser);
}
