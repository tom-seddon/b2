#include <shared/system.h>
#include "SymbolTable.h"
#include <shared/log.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(SYMBOLS, "SYMBOLS", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolTable() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::~SymbolTable() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::Clear() {
    m_address_to_symbol.clear();
    m_name_to_address.clear();
    LOGF(SYMBOLS, "Symbol table cleared\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromFile(const std::string& filepath) {
    LOGF(SYMBOLS, "Loading symbols from: %s\n", filepath.c_str());
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOGF(SYMBOLS, "ERROR: Could not open symbol file: %s\n", filepath.c_str());
        return false;
    }
    
    // Read entire file content
    std::ostringstream content_stream;
    content_stream << file.rdbuf();
    std::string content = content_stream.str();
    file.close();
    
    // Clear existing symbols before loading new ones
    size_t old_count = GetSymbolCount();
    Clear();
    
    // Try to detect format and load
    bool success = false;
    
    // For now, assume VICE format - can add format detection later
    success = LoadViceFormat(content);
    
    if (success) {
        size_t new_count = GetSymbolCount();
        LOGF(SYMBOLS, "Successfully loaded %zu symbols (replaced %zu existing)\n", 
             new_count, old_count);
    } else {
        LOGF(SYMBOLS, "ERROR: Failed to parse symbol file: %s\n", filepath.c_str());
    }
    
    return success;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadViceFormat(const std::string& content) {
    LOGF(SYMBOLS, "Parsing VICE label format\n");
    
    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;
    size_t symbols_loaded = 0;
    
    // VICE format: "al 00FFFF ._some_symbol"
    // More flexible regex to handle variations
    std::regex pattern(R"(^\s*al\s+([0-9a-fA-F]{4,6})\s+(.+?)\s*$)");
    
    while (std::getline(stream, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        
        std::smatch matches;
        if (std::regex_match(line, matches, pattern)) {
            try {
                uint32_t addr = std::stoul(matches[1].str(), nullptr, 16);
                std::string name = TrimWhitespace(matches[2].str());
                
                // Strip leading dot from VICE format symbols
                if (name.length() > 0 && name[0] == '.') {
                    name = name.substr(1);
                }
                
                if (IsValidAddress(addr) && !name.empty()) {
                    Symbol symbol((uint16_t)addr, name);
                    
                    // Check for duplicate addresses or names
                    if (m_address_to_symbol.find(symbol.address) != m_address_to_symbol.end()) {
                        LOGF(SYMBOLS, "WARNING: Duplicate address $%04X at line %zu, overwriting\n", 
                             symbol.address, line_number);
                    }
                    if (m_name_to_address.find(name) != m_name_to_address.end()) {
                        LOGF(SYMBOLS, "WARNING: Duplicate symbol name '%s' at line %zu, overwriting\n", 
                             name.c_str(), line_number);
                    }
                    
                    m_address_to_symbol[symbol.address] = symbol;
                    m_name_to_address[symbol.name] = symbol.address;
                    symbols_loaded++;
                } else {
                    LOGF(SYMBOLS, "WARNING: Invalid symbol at line %zu: address=$%X, name='%s'\n", 
                         line_number, addr, name.c_str());
                }
            } catch (const std::exception& e) {
                LOGF(SYMBOLS, "WARNING: Parse error at line %zu: %s\n", line_number, e.what());
            }
        } else {
            // Only log non-empty, non-comment lines that don't match
            std::string trimmed = TrimWhitespace(line);
            if (!trimmed.empty()) {
                LOGF(SYMBOLS, "WARNING: Unrecognized format at line %zu: '%s'\n", 
                     line_number, trimmed.c_str());
            }
        }
    }
    
    LOGF(SYMBOLS, "Parsed %zu lines, loaded %zu symbols\n", line_number, symbols_loaded);
    return symbols_loaded > 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetSymbolCount() const {
    return m_address_to_symbol.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const SymbolTable::Symbol* SymbolTable::GetSymbolForAddress(uint16_t address) const {
    auto it = m_address_to_symbol.find(address);
    return (it != m_address_to_symbol.end()) ? &it->second : nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t SymbolTable::GetAddressForSymbol(const std::string& name) const {
    auto it = m_name_to_address.find(name);
    return (it != m_name_to_address.end()) ? it->second : 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::HasSymbolForAddress(uint16_t address) const {
    return m_address_to_symbol.find(address) != m_address_to_symbol.end();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::HasSymbol(const std::string& name) const {
    return m_name_to_address.find(name) != m_name_to_address.end();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::PrintStats() const {
    LOGF(SYMBOLS, "Symbol table statistics:\n");
    LOGF(SYMBOLS, "  Total symbols: %zu\n", GetSymbolCount());
    
    if (GetSymbolCount() > 0) {
        // Find address range
        uint16_t min_addr = 0xFFFF;
        uint16_t max_addr = 0;
        for (const auto& pair : m_address_to_symbol) {
            min_addr = std::min(min_addr, pair.first);
            max_addr = std::max(max_addr, pair.first);
        }
        LOGF(SYMBOLS, "  Address range: $%04X - $%04X\n", min_addr, max_addr);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SymbolTable::TrimWhitespace(const std::string& str) const {
    const char* whitespace = " \t\r\n";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::IsValidAddress(uint32_t addr) const {
    // BBC Micro has 16-bit address space
    return addr <= 0xFFFF;
}
