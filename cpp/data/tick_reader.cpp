#include "tick_reader.hpp"

namespace backtest {

// ============================================================================
// CSVTickReader
// ============================================================================
CSVTickReader::~CSVTickReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool CSVTickReader::open() {
    file_.open(filename_);
    if (!file_.is_open()) {
        return false;
    }

    // Skip header line
    std::string header;
    std::getline(file_, header);
    line_number_ = 1;
    is_open_ = true;

    return true;
}

bool CSVTickReader::next_tick(Tick& tick) {
    if (!is_open_ && !open()) {
        return false;
    }

    std::string line;
    if (!std::getline(file_, line)) {
        return false;
    }

    ++line_number_;
    return parse_line(line, tick);
}

void CSVTickReader::reset() {
    if (file_.is_open()) {
        file_.clear();
        file_.seekg(0);

        // Skip header
        std::string header;
        std::getline(file_, header);
        line_number_ = 1;
    }
}

bool CSVTickReader::parse_line(const std::string& line, Tick& tick) {
    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> tokens;

    while (std::getline(ss, token, delimiter_)) {
        tokens.push_back(token);
    }

    if (tokens.size() < 9) {
        return false;
    }

    try {
        tick.timestamp = std::stoll(tokens[0]);
        tick.symbol = Symbol(tokens[1]);
        tick.bid_price = Price::from_double(std::stod(tokens[2]));
        tick.ask_price = Price::from_double(std::stod(tokens[3]));
        tick.bid_size = Quantity::from_double(std::stod(tokens[4]));
        tick.ask_size = Quantity::from_double(std::stod(tokens[5]));
        tick.last_price = Price::from_double(std::stod(tokens[6]));
        tick.last_size = Quantity::from_double(std::stod(tokens[7]));
        tick.volume = std::stoull(tokens[8]);
    } catch (...) {
        return false;
    }

    return true;
}

// ============================================================================
// BinaryTickReader
// ============================================================================
BinaryTickReader::~BinaryTickReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool BinaryTickReader::open() {
    file_.open(filename_, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }

    // Read header (total tick count)
    file_.read(reinterpret_cast<char*>(&total_ticks_), sizeof(total_ticks_));
    is_open_ = true;

    return true;
}

bool BinaryTickReader::next_tick(Tick& tick) {
    if (!is_open_ && !open()) {
        return false;
    }

    BinaryTick btick;
    if (!file_.read(reinterpret_cast<char*>(&btick), sizeof(btick))) {
        return false;
    }

    tick.timestamp = btick.timestamp;
    std::memcpy(tick.symbol.data, btick.symbol, MAX_SYMBOL_LENGTH);
    tick.bid_price = Price(btick.bid_price);
    tick.ask_price = Price(btick.ask_price);
    tick.bid_size = Quantity(btick.bid_size);
    tick.ask_size = Quantity(btick.ask_size);
    tick.last_price = Price(btick.last_price);
    tick.last_size = Quantity(btick.last_size);
    tick.volume = btick.volume;

    ++ticks_read_;
    return true;
}

void BinaryTickReader::reset() {
    if (file_.is_open()) {
        file_.clear();
        file_.seekg(sizeof(uint64_t));  // Skip header
        ticks_read_ = 0;
    }
}

// ============================================================================
// BinaryTickWriter
// ============================================================================
BinaryTickWriter::~BinaryTickWriter() {
    close();
}

bool BinaryTickWriter::open() {
    file_.open(filename_, std::ios::binary);
    if (!file_.is_open()) {
        return false;
    }

    // Write placeholder for total tick count
    uint64_t placeholder = 0;
    file_.write(reinterpret_cast<const char*>(&placeholder), sizeof(placeholder));

    return true;
}

bool BinaryTickWriter::write(const Tick& tick) {
    if (!file_.is_open()) return false;

    BinaryTick btick;
    btick.timestamp = tick.timestamp;
    std::memcpy(btick.symbol, tick.symbol.data, MAX_SYMBOL_LENGTH);
    btick.bid_price = tick.bid_price.raw;
    btick.ask_price = tick.ask_price.raw;
    btick.bid_size = tick.bid_size.raw;
    btick.ask_size = tick.ask_size.raw;
    btick.last_price = tick.last_price.raw;
    btick.last_size = tick.last_size.raw;
    btick.volume = tick.volume;

    file_.write(reinterpret_cast<const char*>(&btick), sizeof(btick));
    ++ticks_written_;

    return true;
}

void BinaryTickWriter::close() {
    if (file_.is_open()) {
        // Go back and write actual tick count
        file_.seekp(0);
        file_.write(reinterpret_cast<const char*>(&ticks_written_), sizeof(ticks_written_));
        file_.close();
    }
}

}  // namespace backtest
