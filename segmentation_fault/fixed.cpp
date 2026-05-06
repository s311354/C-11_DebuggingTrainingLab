#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

struct Packet {
    std::string source_id;
    int reading;
};

struct Route {
    Route() : alarm_threshold(0) {}
    Route(const std::string& queue, int threshold)
        : queue_name(queue), alarm_threshold(threshold) {}

    std::string queue_name;
    int alarm_threshold;

    void deliver(const Packet& packet) const {
        std::cout << "delivered " << packet.source_id << "=" << packet.reading
                  << " to " << queue_name << "\n";
    }
};

typedef std::map<std::string, Route> RoutingTable;

RoutingTable build_routes() {
    RoutingTable routes;
    routes.insert(std::make_pair("pump-A", Route("telemetry/pumps", 80)));
    routes.insert(std::make_pair("valve-B", Route("telemetry/valves", 50)));
    routes.insert(std::make_pair("fan-C", Route("telemetry/fans", 70)));
    return routes;
}

const Route* find_route(const RoutingTable& routes, const std::string& source_id) {
    RoutingTable::const_iterator it = routes.find(source_id);
    if (it == routes.end()) {
        return NULL;
    }
    return &it->second;
}

Packet parse_packet(int argc, char* argv[]) {
    Packet packet;
    packet.source_id = argc > 1 ? argv[1] : "heater-X";
    packet.reading = argc > 2 ? std::atoi(argv[2]) : 91;
    return packet;
}

bool dispatch_packet(const RoutingTable& routes, const Packet& packet) {
    const Route* route = find_route(routes, packet.source_id);
    if (route == NULL) {
        std::cerr << "dropping packet from unknown source: "
                  << packet.source_id << "\n";
        return false;
    }

    std::cout << "dispatching " << packet.source_id << " through "
              << route->queue_name << "\n";

    if (packet.reading >= route->alarm_threshold) {
        std::cout << "alarm threshold exceeded\n";
    }

    route->deliver(packet);
    return true;
}

int main(int argc, char* argv[]) {
    RoutingTable routes = build_routes();
    Packet packet = parse_packet(argc, argv);
    return dispatch_packet(routes, packet) ? 0 : 1;
}
