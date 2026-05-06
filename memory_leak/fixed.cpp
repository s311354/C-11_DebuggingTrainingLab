#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

struct Frame {
    Frame(std::size_t frame_id, std::size_t capacity_bytes)
        : id(frame_id), capacity(capacity_bytes), size(0), payload(new char[capacity_bytes]) {}

    ~Frame() {
        delete[] payload;
    }

    std::size_t id;
    std::size_t capacity;
    std::size_t size;
    char* payload;
};

static unsigned checksum_of(const std::string& payload) {
    unsigned sum = 0;
    for (std::size_t i = 0; i < payload.size(); ++i) {
        sum += static_cast<unsigned char>(payload[i]);
    }
    return sum % 256;
}

static std::string make_line(std::size_t id, bool valid) {
    std::ostringstream payload;
    payload << "MSG:chunk=" << id << ";sensor=temp;value=" << (20 + (id % 15));

    const std::string data = payload.str();
    unsigned checksum = checksum_of(data);
    if (!valid) {
        checksum = (checksum + 1) % 256;
    }

    std::ostringstream line;
    line << data << "|" << checksum;
    return line.str();
}

class Receiver {
public:
    bool ingest(std::size_t id, const std::string& raw_line) {
        std::unique_ptr<Frame> frame(new Frame(id, 4096));

        if (raw_line.find("MSG:") != 0) {
            return false;
        }

        std::size_t separator = raw_line.rfind('|');
        if (separator == std::string::npos) {
            return false;
        }

        std::string payload = raw_line.substr(0, separator);
        unsigned expected = static_cast<unsigned>(std::strtoul(
            raw_line.substr(separator + 1).c_str(), NULL, 10));

        if (checksum_of(payload) != expected) {
            return false;
        }

        frame->size = payload.size() < frame->capacity ? payload.size() : frame->capacity;
        std::memcpy(frame->payload, payload.data(), frame->size);
        accepted_.push_back(std::move(frame));
        return true;
    }

    std::size_t accepted_count() const {
        return accepted_.size();
    }

private:
    std::vector<std::unique_ptr<Frame> > accepted_;
};

int main(int argc, char* argv[]) {
    const std::size_t total = argc > 1
        ? static_cast<std::size_t>(std::strtoul(argv[1], NULL, 10))
        : 2000;

    Receiver receiver;
    std::size_t rejected = 0;

    for (std::size_t i = 0; i < total; ++i) {
        const bool valid = false;
        if (!receiver.ingest(i, make_line(i, valid))) {
            ++rejected;
        }
    }

    std::cout << "accepted=" << receiver.accepted_count()
              << " rejected=" << rejected << "\n";
    return 0;
}
