#include <shared/system.h>
#include <shared/debug.h>
#include "SymbolTable.h"
#include "memory_contexts.h"
#include <shared/log.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <vector>
#include <beeb/type.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(SYMBOLS, "SYMBOLS", &log_printer_stdout_and_debugger, false);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::Symbol::Symbol(uint16_t addr, const std::string &symbol_name, size_t group)
    : address(addr)
    , name(symbol_name)
    , group_id(group) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolGroup::SymbolGroup(std::string group_name, std::string path)
    : name(std::move(group_name))
    , file_path(std::move(path)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string ADDRESS_SUFFIXES = "address_suffixes";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Custom JSON serialization (save only essential fields)
nlohmann::json SymbolTable::SymbolGroup::to_json() const {
    nlohmann::json j{
        {"file_path", file_path},
        {"enabled", enabled},
        {"name", name}, // Save custom group name to preserve user choice
        {ADDRESS_SUFFIXES, this->address_suffixes},
    };

    return j;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::SymbolGroup::from_json(const nlohmann::json &j) {
    j.at("file_path").get_to(file_path);
    j.at("enabled").get_to(enabled);

    if (j.contains(ADDRESS_SUFFIXES)) {
        try {
            this->address_suffixes = j[ADDRESS_SUFFIXES].get<std::vector<std::string>>();
        } catch (nlohmann::json::exception &) {
        }
    }

    // Use saved name if available, otherwise auto-generate for backward compatibility
    if (j.contains("name") && j["name"].is_string()) {
        // Use saved custom group name
        name = j["name"].get<std::string>();
    } else {
        // Auto-generate name from file_path for backward compatibility
        if (file_path.empty()) {
            name = "Unknown";
        } else {
            // Extract filename without extension for name
            std::string filename = file_path;
            size_t last_slash = filename.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                filename = filename.substr(last_slash + 1);
            }
            size_t last_dot = filename.find_last_of('.');
            if (last_dot != std::string::npos) {
                filename = filename.substr(0, last_dot);
            }

            // Use "Global" for simple loads, filename for enhanced loads
            // (We'll detect this based on whether contexts are empty - simple loads have no contexts)
            if (this->address_suffixes.empty()) {
                name = "Global";
            } else {
                name = filename;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolTable() {
    // No default group needed - all loads create named groups
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::~SymbolTable() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::Clear() {
    m_address_to_symbols.clear();
    m_name_to_addresses.clear();
    m_groups.clear();
    this->InvalidateCache();
    LOGF(SYMBOLS, "Symbol table cleared\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromFile(const std::string &filepath, const std::string &group_name, std::vector<std::string> address_suffixes) {
    LOGF(SYMBOLS, "Loading symbols from: %s\n", filepath.c_str());

    //// Validate all memory contexts before proceeding
    //for (char context : memory_contexts) {
    //    if (!MemoryContexts::IsValidContext(context)) {
    //        LOGF(SYMBOLS, "ERROR: Invalid memory context '%c' in file: %s\n", context, filepath.c_str());
    //        return false;
    //    }
    //}

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

    // Always create a new group for each file loaded
    std::string actual_group_name = group_name.empty() ? "Global" : group_name;

    // Create new group - names are just display labels, can be duplicated
    SymbolGroup new_group(actual_group_name, filepath);
    new_group.address_suffixes = std::move(address_suffixes);
    size_t group_id = this->AddGroup(new_group);

    size_t old_count = GetSymbolCount();

    // Detect format and load with appropriate parser
    bool success = LoadFromContent(content, group_id);

    if (success) {
        size_t new_count = GetSymbolCount();
        LOGF(SYMBOLS, "Successfully loaded %zu symbols into group '%s' (%zu symbols total)\n",
             new_count - old_count, actual_group_name.c_str(), new_count);
    } else {
        LOGF(SYMBOLS, "ERROR: Failed to parse symbol file: %s\n", filepath.c_str());
    }

    return success;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// SymbolParserRegistry implementation
std::vector<std::unique_ptr<SymbolTable::SymbolParser>> SymbolTable::SymbolParserRegistry::s_parsers;

void SymbolTable::SymbolParserRegistry::RegisterParser(std::unique_ptr<SymbolParser> parser) {
    s_parsers.push_back(std::move(parser));
}

const std::vector<std::unique_ptr<SymbolTable::SymbolParser>> &SymbolTable::SymbolParserRegistry::GetParsers() {
    return s_parsers;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Concrete parser implementations

class ViceParser : public SymbolTable::SymbolParser {
  public:
    std::string GetFormatName() const override {
        return "VICE";
    }

    bool MatchesLine(const std::string &line) const override {
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            return false;
        }

        // Check for VICE format: "al FFFF symbol_name"
        std::regex pattern(R"(^\s*al\s+(?:[A-Za-z0-9]:)?[0-9a-fA-F]{4,6}\s+.+)");
        return std::regex_match(line, pattern);
    }

    bool ParseContent(const std::string &content, size_t group_id, SymbolTable *table) override {
        return table->LoadViceFormat(content, group_id);
    }
};

class AcmeParser : public SymbolTable::SymbolParser {
  public:
    std::string GetFormatName() const override {
        return "ACME";
    }

    bool MatchesLine(const std::string &line) const override {
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            return false;
        }

        // Check for ACME format: "symbol_name = address"
        std::regex pattern(R"(^\s*[a-zA-Z_][a-zA-Z0-9_]*\s*=\s*\$?[0-9a-fA-F]+\s*$)");
        return std::regex_match(line, pattern);
    }

    bool ParseContent(const std::string &content, size_t group_id, SymbolTable *table) override {
        return table->LoadAcmeFormat(content, group_id);
    }
};

void SymbolTable::SymbolParserRegistry::InitializeBuiltinParsers() {
    if (s_parsers.empty()) {
        RegisterParser(std::make_unique<ViceParser>());
        RegisterParser(std::make_unique<AcmeParser>());
        LOGF(SYMBOLS, "Initialized %zu builtin symbol parsers\n", s_parsers.size());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolParser *SymbolTable::DetectBestParser(const std::string &content) {
    // Ensure builtin parsers are registered
    SymbolParserRegistry::InitializeBuiltinParsers();

    const auto &parsers = SymbolParserRegistry::GetParsers();
    if (parsers.empty()) {
        LOGF(SYMBOLS, "ERROR: No symbol parsers registered!\n");
        return nullptr;
    }

    std::istringstream stream(content);
    std::string line;
    int lines_checked = 0;

    // Track score for each parser
    std::vector<int> parser_scores(parsers.size(), 0);

    // Check first 20 non-empty, non-comment lines
    while (std::getline(stream, line) && lines_checked < 20) {
        // Skip empty lines and comments for counting
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        lines_checked++;

        // Test line against all registered parsers
        for (size_t i = 0; i < parsers.size(); ++i) {
            if (parsers[i]->MatchesLine(line)) {
                parser_scores[i]++;
            }
        }
    }

    // Find parser with highest score (must be >50% confidence)
    double confidence_threshold = 0.5;
    int max_score = 0;
    size_t best_parser_index = 0;
    bool found_parser = false;

    for (size_t i = 0; i < parser_scores.size(); ++i) {
        LOGF(SYMBOLS, "Parser '%s': %d/%d matches (%.1f%%)\n",
             parsers[i]->GetFormatName().c_str(), parser_scores[i], lines_checked,
             lines_checked > 0 ? (double)parser_scores[i] / lines_checked * 100 : 0);

        if (parser_scores[i] > max_score) {
            max_score = parser_scores[i];
            best_parser_index = i;
        }
    }

    // Check if best parser meets confidence threshold
    if (lines_checked > 0 && max_score >= (confidence_threshold * lines_checked)) {
        found_parser = true;
        LOGF(SYMBOLS, "Selected parser '%s' with %.1f%% confidence\n",
             parsers[best_parser_index]->GetFormatName().c_str(),
             (double)max_score / lines_checked * 100);
    } else {
        LOGF(SYMBOLS, "No parser met confidence threshold (need >%.0f%% match)\n",
             confidence_threshold * 100);
    }

    return found_parser ? parsers[best_parser_index].get() : nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromContent(const std::string &content, size_t group_id) {
    // Use new registry-based detection to get the best parser directly
    SymbolParser *best_parser = DetectBestParser(content);

    if (best_parser) {
        LOGF(SYMBOLS, "Using %s parser for content loading\n", best_parser->GetFormatName().c_str());
        return best_parser->ParseContent(content, group_id, this);
    }

    // Fallback to VICE format if no parser found
    LOGF(SYMBOLS, "WARNING: No suitable parser found\n");
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadViceFormat(const std::string &content, size_t group_id) {
    LOGF(SYMBOLS, "Parsing VICE label format\n");

    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;
    size_t symbols_loaded = 0;

    // VICE format: "al 00FFFF ._some_symbol" or "al C:FFFF ._some_symbol"
    // Extended regex to handle memory context prefixes
    std::regex pattern(R"(^\s*al\s+(?:([A-Za-z0-9]):)?([0-9a-fA-F]{4,6})\s+(.+?)\s*$)");

    while (std::getline(stream, line)) {
        line_number++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        std::smatch matches;
        if (std::regex_match(line, matches, pattern)) {
            try {
                // Extract memory context (if present) and address
                std::string context_str = matches[1].str();
                uint32_t addr = std::stoul(matches[2].str(), nullptr, 16);
                std::string name = TrimWhitespace(matches[3].str());

                // Ignore any VICE format context prefixes - contexts are assigned only through UI

                // Strip leading dot from VICE format symbols
                if (name.length() > 0 && name[0] == '.') {
                    name = name.substr(1);
                }

                if (IsValidAddress(addr) && !name.empty()) {
                    Symbol symbol((uint16_t)addr, name, group_id);

                    // Check for duplicates - we allow multiple symbols per address, even from same group
                    auto existing_addr = m_address_to_symbols.find(symbol.address);
                    if (existing_addr != m_address_to_symbols.end()) {
                        // Check if exact same name from same group already exists
                        bool exact_duplicate_found = false;
                        for (const Symbol &existing : existing_addr->second) {
                            if (existing.name == symbol.name && existing.group_id == symbol.group_id) {
                                LOGF(SYMBOLS, "WARNING: Exact duplicate symbol '%s' at $%04X from group %zu at line %zu, skipping\n",
                                     symbol.name.c_str(), symbol.address, symbol.group_id, line_number);
                                exact_duplicate_found = true;
                                break;
                            }
                        }
                        if (exact_duplicate_found) {
                            continue; // Skip exact duplicates
                        }

                        // Log addition of new symbol at existing address
                        bool same_group_different_name = false;
                        for (const Symbol &existing : existing_addr->second) {
                            if (existing.group_id == symbol.group_id && existing.name != symbol.name) {
                                same_group_different_name = true;
                                break;
                            }
                        }

                        if (same_group_different_name) {
                            LOGF(SYMBOLS, "INFO: Adding symbol '%s' at $%04X (overrides earlier symbols from same group, CC65-style)\n",
                                 symbol.name.c_str(), symbol.address);
                        } else {
                            LOGF(SYMBOLS, "INFO: Adding symbol '%s' at $%04X from group %zu (total at address: %zu)\n",
                                 symbol.name.c_str(), symbol.address, symbol.group_id, existing_addr->second.size() + 1);
                        }
                    }

                    // Add symbol to address mapping (append to vector)
                    m_address_to_symbols[symbol.address].push_back(symbol);

                    // Add symbol to name mapping (multimap allows duplicates)
                    m_name_to_addresses.insert({symbol.name, symbol.address});
                    this->InvalidateCache();
                    symbols_loaded++;
                } else {
                    LOGF(SYMBOLS, "WARNING: Invalid symbol at line %zu: address=$%X, name='%s'\n",
                         line_number, addr, name.c_str());
                }
            } catch (const std::exception &e) {
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

bool SymbolTable::LoadAcmeFormat(const std::string &content, size_t group_id) {
    LOGF(SYMBOLS, "Parsing ACME label format\n");

    std::istringstream stream(content);
    std::string line;
    size_t line_number = 0;
    size_t symbols_loaded = 0;

    // ACME format: "symbol_name = address" where address can be:
    // - $FFFF (hexadecimal with $ prefix)
    // - 1234 (decimal number)
    std::regex pattern(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(\$?[0-9a-fA-F]+)\s*$)");

    while (std::getline(stream, line)) {
        line_number++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        std::smatch matches;
        if (std::regex_match(line, matches, pattern)) {
            try {
                std::string name = TrimWhitespace(matches[1].str());
                std::string addr_str = TrimWhitespace(matches[2].str());

                uint32_t addr;
                if (addr_str[0] == '$') {
                    // Hexadecimal address with $ prefix
                    addr = std::stoul(addr_str.substr(1), nullptr, 16);
                } else {
                    // Decimal address
                    addr = std::stoul(addr_str, nullptr, 10);
                }

                if (IsValidAddress(addr) && !name.empty()) {
                    Symbol symbol((uint16_t)addr, name, group_id);

                    // Check for duplicates - we allow multiple symbols per address, even from same group
                    auto existing_addr = m_address_to_symbols.find(symbol.address);
                    if (existing_addr != m_address_to_symbols.end()) {
                        // Check if exact same name from same group already exists
                        bool exact_duplicate_found = false;
                        for (const Symbol &existing : existing_addr->second) {
                            if (existing.name == symbol.name && existing.group_id == symbol.group_id) {
                                LOGF(SYMBOLS, "WARNING: Exact duplicate symbol '%s' at $%04X from group %zu at line %zu, skipping\n",
                                     symbol.name.c_str(), symbol.address, symbol.group_id, line_number);
                                exact_duplicate_found = true;
                                break;
                            }
                        }
                        if (exact_duplicate_found) {
                            continue; // Skip exact duplicates
                        }

                        // Log addition of new symbol at existing address
                        bool same_group_different_name = false;
                        for (const Symbol &existing : existing_addr->second) {
                            if (existing.group_id == symbol.group_id && existing.name != symbol.name) {
                                same_group_different_name = true;
                                break;
                            }
                        }

                        if (same_group_different_name) {
                            LOGF(SYMBOLS, "INFO: Adding symbol '%s' at $%04X (overrides earlier symbols from same group, ACME-style)\n",
                                 symbol.name.c_str(), symbol.address);
                        } else {
                            LOGF(SYMBOLS, "INFO: Adding symbol '%s' at $%04X from group %zu (total at address: %zu)\n",
                                 symbol.name.c_str(), symbol.address, symbol.group_id, existing_addr->second.size() + 1);
                        }
                    }

                    // Add symbol to address mapping (append to vector)
                    m_address_to_symbols[symbol.address].push_back(symbol);

                    // Add symbol to name mapping (multimap allows duplicates)
                    m_name_to_addresses.insert({symbol.name, symbol.address});
                    this->InvalidateCache();
                    symbols_loaded++;
                } else {
                    LOGF(SYMBOLS, "WARNING: Invalid symbol at line %zu: address=$%X, name='%s'\n",
                         line_number, addr, name.c_str());
                }
            } catch (const std::exception &e) {
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
    size_t count = 0;
    for (const auto &pair : m_address_to_symbols) {
        count += pair.second.size();
    }
    return count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetEnabledSymbolCount() const {
    size_t count = 0;
    for (const auto &pair : m_address_to_symbols) {
        for (const Symbol &symbol : pair.second) {
            if (symbol.group_id < m_groups.size() && m_groups[symbol.group_id].enabled) {
                count++;
            }
        }
    }
    return count;
}

size_t SymbolTable::GetSymbolCountForGroup(size_t group_id) const {
    if (group_id >= m_groups.size()) {
        return 0;
    }

    size_t count = 0;
    for (const auto &pair : m_address_to_symbols) {
        for (const Symbol &symbol : pair.second) {
            if (symbol.group_id == group_id) {
                count++;
            }
        }
    }
    return count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//const SymbolTable::Symbol *SymbolTable::GetSymbolForAddress(uint16_t address) const {
//    // Return first enabled symbol at this address
//    return this->GetFirstEnabledSymbolAt(address);
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::GetAddressForSymbol(uint16_t *addr_ptr, uint32_t *dso_ptr, const std::shared_ptr<const BBCMicroType> &type, const std::string &name) const {
    // Find ANY enabled symbol with this name (not just the "best" one for display)
    auto range = m_name_to_addresses.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) {
        uint16_t address = it->second;

        // Check if there's an enabled symbol with this exact name at this address
        auto addr_it = m_address_to_symbols.find(address);
        if (addr_it != m_address_to_symbols.end()) {
            for (const Symbol &symbol : addr_it->second) {
                if (symbol.name == name && symbol.group_id < m_groups.size()) {
                    const SymbolGroup *group = &m_groups[symbol.group_id];
                    if (group->enabled) {
                        *addr_ptr = address;

                        if (!group->address_suffix_dso_masks.empty()) {
                            const SymbolGroup::DSOMask *mask = &group->address_suffix_dso_masks[0];

                            *dso_ptr &= ~mask->mask;
                            *dso_ptr |= mask->value;
                        }

                        return true;
                    }
                }
            }
        }
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool SymbolTable::HasSymbolForAddress(uint16_t address) const {
//    return this->GetSymbolForAddress(address) != nullptr;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//bool SymbolTable::HasSymbol(const std::string &name) const {
//    // Check if ANY enabled symbol with this name exists
//    return this->GetAddressForSymbol(name) != 0;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::PrintStats() const {
    LOGF(SYMBOLS, "Symbol table statistics:\n");
    LOGF(SYMBOLS, "  Total symbols: %zu\n", GetSymbolCount());
    LOGF(SYMBOLS, "  Enabled symbols: %zu\n", GetEnabledSymbolCount());
    LOGF(SYMBOLS, "  Unique addresses with symbols: %zu\n", m_address_to_symbols.size());

    if (GetSymbolCount() > 0) {
        // Find address range
        uint16_t min_addr = 0xFFFF;
        uint16_t max_addr = 0;
        for (const auto &pair : m_address_to_symbols) {
            min_addr = std::min(min_addr, pair.first);
            max_addr = std::max(max_addr, pair.first);
        }
        LOGF(SYMBOLS, "  Address range: $%04X - $%04X\n", min_addr, max_addr);

        // Show addresses with multiple symbols
        size_t multi_symbol_addresses = 0;
        for (const auto &pair : m_address_to_symbols) {
            if (pair.second.size() > 1) {
                multi_symbol_addresses++;
            }
        }
        if (multi_symbol_addresses > 0) {
            LOGF(SYMBOLS, "  Addresses with multiple symbols: %zu\n", multi_symbol_addresses);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string SymbolTable::TrimWhitespace(const std::string &str) const {
    const char *whitespace = " \t\r\n";
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//const SymbolTable::Symbol *SymbolTable::GetFirstEnabledSymbolAt(uint16_t address) const {
//    auto it = m_address_to_symbols.find(address);
//    if (it != m_address_to_symbols.end()) {
//        const std::vector<Symbol> &symbols = it->second;
//
//        // Search for the best symbol using precedence policy:
//        // 1. Prefer symbols from enabled groups
//        // 2. Among enabled groups, prefer lower group_id (first loaded group)
//        // 3. Within same group, prefer later loaded symbols (higher index in vector)
//
//        const Symbol *best_symbol = nullptr;
//        size_t best_group_id = SIZE_MAX;
//
//        for (const Symbol &symbol : symbols) {
//            if (symbol.group_id >= m_groups.size() || !m_groups[symbol.group_id].enabled) {
//                continue; // Skip disabled groups
//            }
//
//            // If this is from a higher priority group (lower group_id), use it
//            if (symbol.group_id < best_group_id) {
//                best_symbol = &symbol;
//                best_group_id = symbol.group_id;
//            }
//            // If same group as current best, prefer this one (later in file)
//            else if (symbol.group_id == best_group_id) {
//                best_symbol = &symbol;
//            }
//        }
//
//        return best_symbol;
//    }
//    return nullptr;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Group Management Methods

size_t SymbolTable::AddGroup(const SymbolGroup &group) {
    m_groups.push_back(group);
    this->InvalidateCache();
    return m_groups.size() - 1;
}

//size_t SymbolTable::AddGroup(const std::string &name, const std::string &description, const std::string &file_path) {
//    SymbolGroup group(name, description, file_path);
//    return this->AddGroup(group);
//}

bool SymbolTable::RemoveGroup(size_t group_id) {
    if (group_id >= m_groups.size()) {
        return false;
    }

    // Remove all symbols belonging to this group
    this->ClearGroup(group_id);

    // Remove the group
    m_groups.erase(m_groups.begin() + static_cast<std::vector<SymbolGroup>::difference_type>(group_id));

    // Update group IDs for all symbols with group_id > removed group
    for (auto &addr_pair : m_address_to_symbols) {
        for (Symbol &symbol : addr_pair.second) {
            if (symbol.group_id > group_id) {
                symbol.group_id--;
            }
        }
    }

    this->InvalidateCache();
    return true;
}

void SymbolTable::EnableGroup(size_t group_id, bool enabled) {
    if (group_id < m_groups.size()) {
        m_groups[group_id].enabled = enabled;
        this->InvalidateCache();
    }
}

const SymbolTable::SymbolGroup *SymbolTable::GetGroup(size_t group_id) const {
    if (group_id < m_groups.size()) {
        return &m_groups[group_id];
    }
    return nullptr;
}

const std::vector<SymbolTable::SymbolGroup> &SymbolTable::GetAllGroups() const {
    return m_groups;
}

void SymbolTable::ClearGroup(size_t group_id) {
    // Remove all symbols belonging to this group
    for (auto addr_it = m_address_to_symbols.begin(); addr_it != m_address_to_symbols.end();) {
        std::vector<Symbol> &symbols = addr_it->second;

        // Remove symbols from this group from the vector
        symbols.erase(
            std::remove_if(symbols.begin(), symbols.end(),
                           [group_id](const Symbol &symbol) {
                               return symbol.group_id == group_id;
                           }),
            symbols.end());

        // If no symbols left at this address, remove the entry entirely
        if (symbols.empty()) {
            addr_it = m_address_to_symbols.erase(addr_it);
        } else {
            ++addr_it;
        }
    }

    // Remove name mappings for symbols in this group
    for (auto name_it = m_name_to_addresses.begin(); name_it != m_name_to_addresses.end();) {
        uint16_t address = name_it->second;
        const std::string &name = name_it->first;

        // Check if this name/address combo still exists after group removal
        bool found = false;
        auto addr_symbols_it = m_address_to_symbols.find(address);
        if (addr_symbols_it != m_address_to_symbols.end()) {
            for (const Symbol &symbol : addr_symbols_it->second) {
                if (symbol.name == name) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            name_it = m_name_to_addresses.erase(name_it);
        } else {
            ++name_it;
        }
    }

    this->InvalidateCache();
}

bool SymbolTable::MoveGroup(size_t from_index, size_t to_index) {
    if (from_index >= m_groups.size() || to_index >= m_groups.size() || from_index == to_index) {
        return false;
    }

    // All groups can now be moved - no special restrictions

    // Create mapping from old position to new position before moving
    std::vector<size_t> old_to_new_mapping(m_groups.size());

    // Initialize identity mapping
    for (size_t i = 0; i < m_groups.size(); ++i) {
        old_to_new_mapping[i] = i;
    }

    // Calculate the new positions after the move
    if (from_index < to_index) {
        // Moving down: shift everything up between from+1 and to
        for (size_t i = from_index + 1; i <= to_index; ++i) {
            old_to_new_mapping[i] = i - 1;
        }
        old_to_new_mapping[from_index] = to_index;
    } else {
        // Moving up: shift everything down between to and from-1
        for (size_t i = to_index; i < from_index; ++i) {
            old_to_new_mapping[i] = i + 1;
        }
        old_to_new_mapping[from_index] = to_index;
    }

    // Move the group in the vector
    SymbolGroup group_to_move = m_groups[from_index];
    m_groups.erase(m_groups.begin() + static_cast<std::vector<SymbolGroup>::difference_type>(from_index));
    m_groups.insert(m_groups.begin() + static_cast<std::vector<SymbolGroup>::difference_type>(to_index), group_to_move);

    // Update all symbol group IDs using position-based mapping
    for (auto &addr_pair : m_address_to_symbols) {
        for (Symbol &symbol : addr_pair.second) {
            if (symbol.group_id < old_to_new_mapping.size()) {
                symbol.group_id = old_to_new_mapping[symbol.group_id];
            }
        }
    }

    this->InvalidateCache();
    return true;
}

void SymbolTable::ReassignGroupIds(const std::vector<std::string> &original_group_names) {
    // This method is deprecated - group reordering now uses position-based mapping
    // instead of name-based mapping to handle duplicate group names correctly.
    // Kept for backward compatibility but no longer used by MoveGroup.
    (void)original_group_names; // Suppress unused parameter warning
}

void SymbolTable::ReassignGroupIds() {
    // This method is deprecated - group reordering now uses position-based mapping
    // built into MoveGroup() instead of separate name-based reassignment.
    // Kept for backward compatibility but no longer used.
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Group Metadata Editing Methods

bool SymbolTable::SetGroupName(size_t group_id, const std::string &new_name) {
    if (group_id >= m_groups.size()) {
        return false;
    }

    std::string trimmed_name = new_name;
    // Remove leading/trailing whitespace
    size_t start = trimmed_name.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        trimmed_name = "Global"; // Default to Global if empty/whitespace only
    } else {
        size_t end = trimmed_name.find_last_not_of(" \t\n\r");
        trimmed_name = trimmed_name.substr(start, end - start + 1);

        if (trimmed_name.empty()) {
            trimmed_name = "Global";
        }
    }

    m_groups[group_id].name = trimmed_name;
    LOGF(SYMBOLS, "Updated group %zu name to: %s\n", group_id, trimmed_name.c_str());
    return true;
}

void SymbolTable::SetGroupAddressSuffixes(size_t group_id, std::vector<std::string> new_address_suffixes) {
    ASSERT(group_id < m_groups.size());

    m_groups[group_id].address_suffixes = std::move(new_address_suffixes);
}

//bool SymbolTable::SetGroupContexts(size_t group_id, const std::set<char> &new_contexts) {
//    if (group_id >= m_groups.size()) {
//        return false;
//    }
//
//    // Validate all contexts before setting
//    for (char context : new_contexts) {
//        if (!MemoryContexts::IsValidContext(context)) {
//            LOGF(SYMBOLS, "WARNING: Invalid memory context '%c' ignored in group %zu\n", context, group_id);
//            return false;
//        }
//    }
//
//    m_groups[group_id].memory_contexts = new_contexts;
//
//    // Invalidate cache since context changes affect symbol visibility
//    this->InvalidateCache();
//
//    if (new_contexts.empty()) {
//        LOGF(SYMBOLS, "Updated group %zu contexts to: universal (visible everywhere)\n", group_id);
//    } else {
//        LOGF(SYMBOLS, "Updated group %zu contexts to: ", group_id);
//        for (char c : new_contexts) {
//            LOGF(SYMBOLS, "'%c' ", c);
//        }
//        LOGF(SYMBOLS, "\n");
//    }
//
//    return true;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Context-Aware Symbol Lookup Methods

const SymbolTable::Symbol *SymbolTable::GetSymbolForAddress(uint16_t address, uint32_t dso, const std::shared_ptr<const BBCMicroType> &type) const {
    this->EnsureCacheReady(type);

    auto it = m_address_to_symbols.find(address);
    if (it == m_address_to_symbols.end()) {
        return nullptr;
    }

    for (const Symbol &symbol : it->second) {
        if (symbol.group_id < m_groups.size()) {
            const SymbolGroup *group = &m_groups[symbol.group_id];
            for (const SymbolGroup::DSOMask &mask : group->address_suffix_dso_masks) {
                if ((dso & mask.mask) == mask.value) {
                    return &symbol;
                }
            }
        }
    }

    return nullptr;

    //auto context_it = m_context_to_address_cache.find(memory_context);
    //if (context_it != m_context_to_address_cache.end()) {
    //    auto symbol_it = context_it->second.find(address);
    //    if (symbol_it != context_it->second.end()) {
    //        const std::vector<Symbol *> &symbols = symbol_it->second;
    //        if (!symbols.empty()) {
    //            // Apply precedence policy to find best symbol
    //            const Symbol *best_symbol = nullptr;
    //            size_t best_group_id = SIZE_MAX;

    //            for (const Symbol *symbol : symbols) {
    //                // Skip disabled groups (should not happen due to cache, but be safe)
    //                if (symbol->group_id >= m_groups.size() || !m_groups[symbol->group_id].enabled) {
    //                    continue;
    //                }

    //                // Prefer lower group_id (first loaded group)
    //                if (symbol->group_id < best_group_id) {
    //                    best_symbol = symbol;
    //                    best_group_id = symbol->group_id;
    //                }
    //                // Within same group, prefer later loaded symbol (CC65 style)
    //                else if (symbol->group_id == best_group_id) {
    //                    best_symbol = symbol; // Later in cache = later loaded
    //                }
    //            }

    //            return best_symbol;
    //        }
    //    }
    //}
    //return nullptr;
}

//uint16_t SymbolTable::GetAddressForSymbol(const std::string &name, char memory_context) const {
//    // Find ANY symbol with this name that is visible in the specified context
//    auto range = m_name_to_addresses.equal_range(name);
//    for (auto it = range.first; it != range.second; ++it) {
//        uint16_t address = it->second;
//
//        // Check if there's an enabled symbol with this name visible in this context
//        auto addr_it = m_address_to_symbols.find(address);
//        if (addr_it != m_address_to_symbols.end()) {
//            for (const Symbol &symbol : addr_it->second) {
//                if (symbol.name == name && this->IsSymbolVisibleInContext(symbol, memory_context)) {
//                    return address; // Found a symbol with this name in this context
//                }
//            }
//        }
//    }
//
//    return 0; // Symbol not visible in this context
//}

//bool SymbolTable::HasSymbolForAddress(uint16_t address, char memory_context) const {
//    return this->GetSymbolForAddress(address, memory_context) != nullptr;
//}

//bool SymbolTable::HasSymbol(const std::string &name, char memory_context) const {
//    return this->GetAddressForSymbol(name, memory_context) != 0;
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Cache Management Methods

void SymbolTable::InvalidateCache() const {
    m_cache_type.reset();
}

void SymbolTable::EnsureCacheReady(const std::shared_ptr<const BBCMicroType> &type) const {
    if (m_cache_type == type) {
        return;
    }

    for (SymbolGroup &group : m_groups) {
        group.address_suffix_dso_masks.clear();

        for (const std::string &address_suffix : group.address_suffixes) {
            uint32_t dso = 0;

            if (ParseAddressSuffix(&dso, type, address_suffix.c_str(), nullptr)) {
                SymbolGroup::DSOMask mask;

                mask.mask = GetDSOMaskForOverrides(dso) & type->dso_mask;
                mask.value = dso & type->dso_mask;

                group.address_suffix_dso_masks.push_back(mask);
            } else {
                // Should have been called out in the UI. Nothing to be done at
                // this stage but ignore it.
            }
        }

        // TODO eliminate redundant masks.
    }

    //m_context_to_address_cache.clear();

    //for (auto &addr_pair : m_address_to_symbols) {
    //    uint16_t address = addr_pair.first;
    //    const std::vector<Symbol> &symbols = addr_pair.second;

    //    for (const Symbol &symbol : symbols) {
    //        // Skip disabled groups
    //        if (symbol.group_id >= m_groups.size() || !m_groups[symbol.group_id].enabled) {
    //            continue;
    //        }

    //        const SymbolGroup &group = m_groups[symbol.group_id];

    //        // If group has specific contexts, add to those contexts
    //        if (!group.memory_contexts.empty()) {
    //            for (char context : group.memory_contexts) {
    //                m_context_to_address_cache[context][address].push_back(const_cast<Symbol *>(&symbol));
    //            }
    //        } else {
    //            // Universal symbols: add to ALL common contexts
    //            for (char context : MemoryContexts::UNIVERSAL_CACHE_CONTEXTS) {
    //                m_context_to_address_cache[context][address].push_back(const_cast<Symbol *>(&symbol));
    //            }
    //        }
    //    }
    //}

    m_cache_type = type;
}

//bool SymbolTable::IsSymbolVisibleInContext(const Symbol &symbol, char memory_context) const {
//    if (symbol.group_id >= m_groups.size() || !m_groups[symbol.group_id].enabled) {
//        return false;
//    }
//
//    const SymbolGroup &group = m_groups[symbol.group_id];
//
//    // If group has no specific contexts, it's visible in ALL contexts (universal symbols)
//    if (group.memory_contexts.empty()) {
//        return true;
//    }
//
//    // Check if the symbol's group applies to this memory context
//    return group.memory_contexts.find(memory_context) != group.memory_contexts.end();
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Persistence Methods

nlohmann::json SymbolTable::SaveToJSON() const {
    nlohmann::json j;

    // Save groups (only essential fields, symbols are automatically reloaded from files)
    j["groups"] = nlohmann::json::array();
    for (const auto &group : m_groups) {
        j["groups"].push_back(group.to_json());
    }

    return j;
}

bool SymbolTable::LoadFromJSON(const nlohmann::json &j) {
    try {
        if (j.contains("groups") && j["groups"].is_array()) {
            m_groups.clear();
            for (const auto &group_json : j["groups"]) {
                SymbolGroup group;
                group.from_json(group_json);
                m_groups.push_back(group);
            }

            // Reload all groups from their source files
            this->ReloadAllGroups();
            return true;
        }
    } catch (const std::exception &e) {
        LOGF(SYMBOLS, "ERROR: Failed to load symbol groups from JSON: %s\n", e.what());
    }

    return false;
}

void SymbolTable::ReloadAllGroups() {
    // Clear all symbols but keep groups
    m_address_to_symbols.clear();
    m_name_to_addresses.clear();
    this->InvalidateCache();

    // Reload each group from its source file
    for (size_t i = 0; i < m_groups.size(); ++i) {
        const SymbolGroup &group = m_groups[i];

        // Skip groups without source files (but always load disabled groups for counting)
        if (group.file_path.empty()) {
            continue;
        }

        // Check if file exists
        std::ifstream test_file(group.file_path);
        if (!test_file.is_open()) {
            LOGF(SYMBOLS, "WARNING: Cannot reload group '%s' - file not found: %s\n",
                 group.name.c_str(), group.file_path.c_str());
            continue;
        }
        test_file.close();

        // Read and parse the file
        std::ifstream file(group.file_path);
        std::ostringstream content_stream;
        content_stream << file.rdbuf();
        std::string content = content_stream.str();
        file.close();

        // Load symbols into this group (preserving enabled state)
        LOGF(SYMBOLS, "Reloading group '%s' from: %s (enabled: %s)\n",
             group.name.c_str(), group.file_path.c_str(), group.enabled ? "true" : "false");
        this->LoadFromContent(content, i);
    }

    LOGF(SYMBOLS, "Reloaded %zu symbols across %zu groups\n", GetSymbolCount(), m_groups.size());
}
