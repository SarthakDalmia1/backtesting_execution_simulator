#pragma once

#include "core/types.hpp"
#include "data/market_data_feed.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

namespace backtest {

// ============================================================================
// CSV Tick Reader
// ============================================================================
// Reads tick data from CSV files
// Expected format: timestamp,symbol,bid,ask,bid_size,ask_size,last,last_size,volume
class CSVTickReader : public MarketDataFeed {
public:
    explicit CSVTickReader(const std::string& filename, char delimiter = ',')
        : filename_(filename), delimiter_(delimiter), line_number_(0), is_open_(false) {}
    
    ~CSVTickReader() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    bool open() {
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
    
    bool next_tick(Tick& tick) override {
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
    
    void reset() override {
        if (file_.is_open()) {
            file_.clear();
            file_.seekg(0);
            
            // Skip header
            std::string header;
            std::getline(file_, header);
            line_number_ = 1;
        }
    }
    
    bool has_data() const override {
        return is_open_ && file_.good() && !file_.eof();
    }
    
    size_t lines_read() const { return line_number_; }

private:
    bool parse_line(const std::string& line, Tick& tick) {
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
    
    std::string filename_;
    char delimiter_;
    std::ifstream file_;
    size_t line_number_;
    bool is_open_;
};

// ============================================================================
// Binary Tick Reader (for high-performance reading)
// ============================================================================
// Reads ticks in a compact binary format
struct BinaryTick {
    int64_t timestamp;
    char symbol[MAX_SYMBOL_LENGTH];
    int64_t bid_price;
    int64_t ask_price;
    int64_t bid_size;
    int64_t ask_size;
    int64_t last_price;
    int64_t last_size;
    uint64_t volume;
};

// Size depends on MAX_SYMBOL_LENGTH (16) + 9 * 8 bytes = 88 bytes (may have padding)

class BinaryTickReader : public MarketDataFeed {
public:
    explicit BinaryTickReader(const std::string& filename)
        : filename_(filename), is_open_(false), ticks_read_(0), total_ticks_(0) {}
    
    ~BinaryTickReader() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    bool open() {
        file_.open(filename_, std::ios::binary);
        if (!file_.is_open()) {
            return false;
        }
        
        // Read header (total tick count)
        file_.read(reinterpret_cast<char*>(&total_ticks_), sizeof(total_ticks_));
        is_open_ = true;
        
        return true;
    }
    
    bool next_tick(Tick& tick) override {
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
    
    void reset() override {
        if (file_.is_open()) {
            file_.clear();
            file_.seekg(sizeof(uint64_t));  // Skip header
            ticks_read_ = 0;
        }
    }
    
    bool has_data() const override {
        return is_open_ && ticks_read_ < total_ticks_;
    }
    
    size_t total_ticks() const override { return total_ticks_; }

private:
    std::string filename_;
    std::ifstream file_;
    bool is_open_;
    size_t ticks_read_;
    uint64_t total_ticks_;
};

// ============================================================================
// Binary Tick Writer
// ============================================================================
class BinaryTickWriter {
public:
    explicit BinaryTickWriter(const std::string& filename)
        : filename_(filename), ticks_written_(0) {}
    
    ~BinaryTickWriter() {
        close();
    }
    
    bool open() {
        file_.open(filename_, std::ios::binary);
        if (!file_.is_open()) {
            return false;
        }
        
        // Write placeholder for total tick count
        uint64_t placeholder = 0;
        file_.write(reinterpret_cast<const char*>(&placeholder), sizeof(placeholder));
        
        return true;
    }
    
    bool write(const Tick& tick) {
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
    
    void close() {
        if (file_.is_open()) {
            // Go back and write actual tick count
            file_.seekp(0);
            file_.write(reinterpret_cast<const char*>(&ticks_written_), sizeof(ticks_written_));
            file_.close();
        }
    }
    
    size_t ticks_written() const { return ticks_written_; }

private:
    std::string filename_;
    std::ofstream file_;
    uint64_t ticks_written_;
};

}  // namespace backtest
