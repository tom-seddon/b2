#include <shared/system.h>
#include <shared/system.h>
#include "conf.h"

#if BBCMICRO_DEBUGGER

#include "nlohmann_json_wrapper.h"
#include "SymbolTable.h"
#include <shared/debug.h>
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

static std::string TrimWhitespace(const std::string &str) {
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

static const std::string ADDRESS_SUFFIXES = "address_suffixes";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolTable() {
    // No default group needed - all loads create named groups

    // Ensure builtin parsers are registered
    SymbolParserRegistry::InitializeBuiltinParsers();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::~SymbolTable() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::Clear() {
    //m_address_to_symbols.clear();
    //m_name_to_addresses.clear();
    m_groups.clear();
    this->InvalidateCache();
    LOGF(SYMBOLS, "Symbol table cleared\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromFile(const std::string &filepath, const SymbolParser *parser) {
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
    //std::string actual_group_name = group_name.empty() ? "Global" : group_name;

    if (!this->LoadFromString(content, filepath, parser)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromString(const std::string &content, const std::string &filepath, const SymbolParser *parser) {
    size_t group_id;
    LoadedSymbolGroup *lsg;
    {
        SymbolGroup new_group;

        new_group.file_path = filepath;
        if (parser) {
            new_group.file_format_name = parser->GetFormatName();
        }

        lsg = this->AddLoadedSymbolGroup(std::move(new_group), &group_id);
    }

    //size_t group_id = m_groups.size();
    //m_groups.push_back(std::make_unique<LoadedSymbolGroup>());
    //LoadedSymbolGroup *loaded_group = m_groups.back().get();

    //loaded_group->group.file_path = filepath;
    //if (parser) {
    //    loaded_group->group.file_format_name = parser->GetFormatName();
    //}

    size_t old_count = GetSymbolCount();

    // Detect format and load with appropriate parser
    bool success = LoadFromContent(content, group_id);

    if (success) {
        size_t new_count = GetSymbolCount();
        LOGF(SYMBOLS, "Successfully loaded %zu symbols (%zu symbols total)\n",
             new_count - old_count, new_count);
    } else {
        LOGF(SYMBOLS, "ERROR: Failed to parse symbol file: %s\n", filepath.c_str());
    }

    return success;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// SymbolParserRegistry implementation
std::vector<std::unique_ptr<const SymbolTable::SymbolParser>> SymbolTable::SymbolParserRegistry::s_parsers;

void SymbolTable::SymbolParserRegistry::RegisterParser(std::unique_ptr<const SymbolParser> parser) {
    ASSERT(!FindParserByFormatName(parser->GetFormatName()));

    s_parsers.push_back(std::move(parser));
}

const std::vector<std::unique_ptr<const SymbolTable::SymbolParser>> &SymbolTable::SymbolParserRegistry::GetParsers() {
    return s_parsers;
}

const SymbolTable::SymbolParser *SymbolTable::SymbolParserRegistry::FindParserByFormatName(const std::string &format_name) {
    for (const std::unique_ptr<const SymbolParser> &parser : s_parsers) {
        if (parser->GetFormatName() == format_name) {
            return parser.get();
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Concrete parser implementations

class ViceParser : public SymbolTable::SymbolParser {
  public:
    std::string GetFormatName() const override {
        return "VICE";
    }

    std::vector<std::string> GetSuggestedFileExtensions() const {
        return {".vice", ".lbl", ".sym"};
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

    std::vector<Symbol> ParseContent(const std::string &content) const override {
        LOGF(SYMBOLS, "Parsing VICE label format\n");

        std::vector<Symbol> symbols;

        std::istringstream stream(content);
        std::string line;
        size_t line_number = 0;

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
                    Symbol symbol;
                    symbol.line_number = line_number;

                    // Extract memory context (if present) and address
                    std::string context_str = matches[1].str();
                    symbol.address = (uint16_t)std::stoul(matches[2].str(), nullptr, 16);
                    symbol.name = TrimWhitespace(matches[3].str());

                    // Ignore any VICE format context prefixes - contexts are assigned only through UI

                    // Strip leading dot from VICE format symbols
                    if (symbol.name.length() > 0 && symbol.name[0] == '.') {
                        symbol.name = symbol.name.substr(1);
                    }

                    symbols.push_back(std::move(symbol));
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

        LOGF(SYMBOLS, "Parsed %zu lines, loaded %zu symbols\n", line_number, symbols.size());
        return symbols;
    }
};

class AcmeParser : public SymbolTable::SymbolParser {
  public:
    std::string GetFormatName() const override {
        return "ACME";
    }

    std::vector<std::string> GetSuggestedFileExtensions() const {
        return {".lbl", ".sym"};
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

    std::vector<Symbol> ParseContent(const std::string &content) const override {
        LOGF(SYMBOLS, "Parsing ACME label format\n");

        std::vector<Symbol> symbols;

        std::istringstream stream(content);
        std::string line;
        size_t line_number = 0;

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
                    Symbol symbol;
                    symbol.line_number = line_number;
                    symbol.name = TrimWhitespace(matches[1].str());

                    std::string addr_str = TrimWhitespace(matches[2].str());

                    if (addr_str[0] == '$') {
                        // Hexadecimal address with $ prefix
                        symbol.address = (uint16_t)std::stoul(addr_str.substr(1), nullptr, 16);
                    } else {
                        // Decimal address
                        symbol.address = (uint16_t)std::stoul(addr_str, nullptr, 10);
                    }

                    symbols.push_back(std::move(symbol));
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

        LOGF(SYMBOLS, "Parsed %zu lines, loaded %zu symbols\n", line_number, symbols.size());
        return symbols;
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

const SymbolTable::SymbolParser *SymbolTable::DetectBestParser(const std::string &content) {
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
    LoadedSymbolGroup *lsg = m_groups[group_id].get();

    const SymbolParser *parser = SymbolParserRegistry::FindParserByFormatName(lsg->group.file_format_name);
    if (!parser) {
        parser = DetectBestParser(content);
    }

    if (!parser) {
        LOGF(SYMBOLS, "WARNING: No suitable parser found\n");
        return false;
    }

    this->InvalidateCache();

    LOGF(SYMBOLS, "Using %s parser for content loading\n", parser->GetFormatName().c_str());
    lsg->symbols = parser->ParseContent(content);

    if (lsg->symbols.empty()) {
        LOGF(SYMBOLS, "WARNING: No symbols loaded from file\n");
        return true;
    }

    auto &&symbol_it = lsg->symbols.begin();
    while (symbol_it != lsg->symbols.end()) {
        Symbol *symbol = &*symbol_it;

        if (symbol->name.empty()) {
            LOGF(SYMBOLS, "WARNING: Invalid symbol at line %zu: address=$%X, name='%s'\n",
                 symbol->line_number, symbol->address, symbol->name.c_str());

            symbol_it = lsg->symbols.erase(symbol_it);
            continue;
        }

        //symbol->group_id = group_id;

        //// Check for duplicates - we allow multiple symbols per address, even from same group
        //auto existing_addr = m_address_to_symbols.find(symbol->address);
        //if (existing_addr != m_address_to_symbols.end()) {
        //    // Check if exact same name from same group already exists
        //    bool exact_duplicate_found = false;
        //    for (const Symbol &existing : existing_addr->second) {
        //        if (existing.name == symbol->name && existing.group_id == symbol->group_id) {
        //            LOGF(SYMBOLS, "WARNING: Exact duplicate symbol '%s' at $%04X from group %zu at line %zu, skipping\n",
        //                 symbol->name.c_str(), symbol->address, symbol->group_id, symbol->line_number);
        //            exact_duplicate_found = true;
        //            break;
        //        }
        //    }
        //    if (exact_duplicate_found) {
        //        continue; // Skip exact duplicates
        //    }

        //    // Log addition of new symbol at existing address
        //    bool same_group_different_name = false;
        //    for (const Symbol &existing : existing_addr->second) {
        //        if (existing.group_id == symbol->group_id && existing.name != symbol->name) {
        //            same_group_different_name = true;
        //            break;
        //        }
        //    }

        //    if (same_group_different_name) {
        //        LOGF(SYMBOLS, "INFO: Adding symbol '%s' at $%04X (overrides earlier symbols from same group. Parser: %s)\n",
        //             symbol->name.c_str(), symbol->address, parser->GetFormatName().c_str());
        //    } else {
        //        LOGF(SYMBOLS, "INFO: Adding symbol '%s' at $%04X from group %zu (total at address: %zu)\n",
        //             symbol->name.c_str(), symbol->address, symbol->group_id, existing_addr->second.size() + 1);
        //    }
        //}

        //// Add symbol to address mapping (append to vector)
        //m_address_to_symbols[symbol->address].push_back(*symbol);

        //// Add symbol to name mapping (multimap allows duplicates)
        //m_name_to_addresses.insert({symbol->name, symbol->address});

        ++symbol_it;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetSymbolCount() const {
    size_t n = 0;

    for (const std::unique_ptr<LoadedSymbolGroup> &lsg : m_groups) {
        n += lsg->symbols.size();
    }

    return n;
    //size_t count = 0;
    //for (const auto &pair : m_address_to_symbols) {
    //    count += pair.second.size();
    //}
    //return count;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetEnabledSymbolCount() const {
    size_t n = 0;

    for (const std::unique_ptr<LoadedSymbolGroup> &lsg : m_groups) {
        if (lsg->group.enabled) {
            n += lsg->symbols.size();
        }
    }

    return n;
    //size_t count = 0;
    //for (const auto &pair : m_address_to_symbols) {
    //    for (const Symbol &symbol : pair.second) {
    //        if (symbol.group_id < m_groups.size() && m_groups[symbol.group_id]->group.enabled) {
    //            count++;
    //        }
    //    }
    //}
    //return count;
}

size_t SymbolTable::GetSymbolCountForGroup(size_t group_id) const {
    ASSERT(group_id < m_groups.size());
    return m_groups[group_id]->symbols.size();
    //if (group_id >= m_groups.size()) {
    //    return 0;
    //}

    //size_t count = 0;
    //for (const auto &pair : m_address_to_symbols) {
    //    for (const Symbol &symbol : pair.second) {
    //        if (symbol.group_id == group_id) {
    //            count++;
    //        }
    //    }
    //}
    //return count;
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
    this->EnsureCacheReady(type);

    // Find ANY enabled symbol with this name (not just the "best" one for display)
    auto &&name_and_addresses_it = m_cache_name_to_addresses.find(name);
    if (name_and_addresses_it == m_cache_name_to_addresses.end()) {
        return false;
    }

    // just go for the first address for now.
    ASSERT(!name_and_addresses_it->second.empty());
    const AddressForSymbol *addr = &name_and_addresses_it->second[0];

    //auto &&address_and_at_address_it = m_cache_address_to_symbols.find(addr);
    //ASSERT(address_and_at_address_it != m_cache_address_to_symbols.end());

    //ASSERT(!address_and_at_address_it->second.grouped.empty());
    //const SymbolsInGroup *in_group = &address_and_at_address_it->second.grouped[0];

    *addr_ptr = addr->address;

    if (!addr->lsg->address_suffix_dso_masks.empty()) {
        const LoadedSymbolGroup::DSOMask *mask = &addr->lsg->address_suffix_dso_masks[0];

        *dso_ptr &= ~mask->mask;
        *dso_ptr |= mask->value;
    }

    return true;

    //ASSERT(!in_group->symbols.empty()));
    //auto range = m_cache_name_to_addresses.equal_range(name);
    //for (auto it = range.first; it != range.second; ++it) {
    //    uint16_t address = it->second;

    //    // Check if there's an enabled symbol with this exact name at this address
    //    auto it = m_cache_address_to_symbols.find(address);
    //    if (addr_it != m_cache_address_to_symbols.end()) {

    //        for (const Symbol &symbol : addr_it->second) {
    //            if (symbol.name == name && symbol.group_id < m_groups.size()) {
    //                const LoadedSymbolGroup *lsg = m_groups[symbol.group_id].get();
    //                if (lsg->group.enabled) {
    //                    *addr_ptr = address;

    //                    if (!lsg->address_suffix_dso_masks.empty()) {
    //                        const LoadedSymbolGroup::DSOMask *mask = &lsg->address_suffix_dso_masks[0];

    //                        *dso_ptr &= ~mask->mask;
    //                        *dso_ptr |= mask->value;
    //                    }

    //                    return true;
    //                }
    //            }
    //        }
    //    }
    //}
    //return false;
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

//void SymbolTable::PrintStats() const {
//    LOGF(SYMBOLS, "Symbol table statistics:\n");
//    LOGF(SYMBOLS, "  Total symbols: %zu\n", GetSymbolCount());
//    LOGF(SYMBOLS, "  Enabled symbols: %zu\n", GetEnabledSymbolCount());
//    LOGF(SYMBOLS, "  Unique addresses with symbols: %zu\n", m_address_to_symbols.size());
//
//    if (GetSymbolCount() > 0) {
//        // Find address range
//        uint16_t min_addr = 0xFFFF;
//        uint16_t max_addr = 0;
//        for (const auto &pair : m_address_to_symbols) {
//            min_addr = std::min(min_addr, pair.first);
//            max_addr = std::max(max_addr, pair.first);
//        }
//        LOGF(SYMBOLS, "  Address range: $%04X - $%04X\n", min_addr, max_addr);
//
//        // Show addresses with multiple symbols
//        size_t multi_symbol_addresses = 0;
//        for (const auto &pair : m_address_to_symbols) {
//            if (pair.second.size() > 1) {
//                multi_symbol_addresses++;
//            }
//        }
//        if (multi_symbol_addresses > 0) {
//            LOGF(SYMBOLS, "  Addresses with multiple symbols: %zu\n", multi_symbol_addresses);
//        }
//    }
//}

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

//size_t SymbolTable::AddGroup(const SymbolGroup &group) {
//    m_groups.push_back(group);
//    this->InvalidateCache();
//    return m_groups.size() - 1;
//}

//size_t SymbolTable::AddGroup(const std::string &name, const std::string &description, const std::string &file_path) {
//    SymbolGroup group(name, description, file_path);
//    return this->AddGroup(group);
//}

bool SymbolTable::RemoveGroup(size_t group_id) {
    if (group_id >= m_groups.size()) {
        return false;
    }

    // Remove all symbols belonging to this group
    //this->ClearGroup(group_id);

    // Remove the group
    m_groups.erase(m_groups.begin() + static_cast<std::vector<SymbolGroup>::difference_type>(group_id));

    //// Update group IDs for all symbols with group_id > removed group
    //for (auto &addr_pair : m_address_to_symbols) {
    //    for (Symbol &symbol : addr_pair.second) {
    //        if (symbol.group_id > group_id) {
    //            symbol.group_id--;
    //        }
    //    }
    //}

    this->InvalidateCache();
    return true;
}

void SymbolTable::EnableGroup(size_t group_id, bool enabled) {
    if (group_id < m_groups.size()) {
        m_groups[group_id]->group.enabled = enabled;
        this->InvalidateCache();
    }
}

const SymbolGroup *SymbolTable::GetGroupByIndex(size_t index) const {
    ASSERT(index < m_groups.size());
    return &m_groups[index]->group;
}

size_t SymbolTable::GetNumGroups() const {
    return m_groups.size();
}

//void SymbolTable::ClearGroup(size_t group_id) {
//    // Remove all symbols belonging to this group
//    for (auto addr_it = m_address_to_symbols.begin(); addr_it != m_address_to_symbols.end();) {
//        std::vector<Symbol> &symbols = addr_it->second;
//
//        // Remove symbols from this group from the vector
//        symbols.erase(
//            std::remove_if(symbols.begin(), symbols.end(),
//                           [group_id](const Symbol &symbol) {
//                               return symbol.group_id == group_id;
//                           }),
//            symbols.end());
//
//        // If no symbols left at this address, remove the entry entirely
//        if (symbols.empty()) {
//            addr_it = m_address_to_symbols.erase(addr_it);
//        } else {
//            ++addr_it;
//        }
//    }
//
//    // Remove name mappings for symbols in this group
//    for (auto name_it = m_name_to_addresses.begin(); name_it != m_name_to_addresses.end();) {
//        uint16_t address = name_it->second;
//        const std::string &name = name_it->first;
//
//        // Check if this name/address combo still exists after group removal
//        bool found = false;
//        auto addr_symbols_it = m_address_to_symbols.find(address);
//        if (addr_symbols_it != m_address_to_symbols.end()) {
//            for (const Symbol &symbol : addr_symbols_it->second) {
//                if (symbol.name == name) {
//                    found = true;
//                    break;
//                }
//            }
//        }
//
//        if (!found) {
//            name_it = m_name_to_addresses.erase(name_it);
//        } else {
//            ++name_it;
//        }
//    }
//
//    this->InvalidateCache();
//}

bool SymbolTable::MoveGroup(size_t from_index, size_t to_index) {
    if (from_index >= m_groups.size() || to_index >= m_groups.size() || from_index == to_index) {
        return false;
    }

    // All groups can now be moved - no special restrictions

    //// Create mapping from old position to new position before moving
    //std::vector<size_t> old_to_new_mapping(m_groups.size());

    //// Initialize identity mapping
    //for (size_t i = 0; i < m_groups.size(); ++i) {
    //    old_to_new_mapping[i] = i;
    //}

    //// Calculate the new positions after the move
    //if (from_index < to_index) {
    //    // Moving down: shift everything up between from+1 and to
    //    for (size_t i = from_index + 1; i <= to_index; ++i) {
    //        old_to_new_mapping[i] = i - 1;
    //    }
    //    old_to_new_mapping[from_index] = to_index;
    //} else {
    //    // Moving up: shift everything down between to and from-1
    //    for (size_t i = to_index; i < from_index; ++i) {
    //        old_to_new_mapping[i] = i + 1;
    //    }
    //    old_to_new_mapping[from_index] = to_index;
    //}

    // Move the group in the vector
    {
        std::unique_ptr<LoadedSymbolGroup> group_to_move = std::move(m_groups[from_index]);
        m_groups.erase(m_groups.begin() + static_cast<std::vector<SymbolGroup>::difference_type>(from_index));
        m_groups.insert(m_groups.begin() + static_cast<std::vector<SymbolGroup>::difference_type>(to_index), std::move(group_to_move));
    }

    //// Update all symbol group IDs using position-based mapping
    //for (auto &addr_pair : m_address_to_symbols) {
    //    for (Symbol &symbol : addr_pair.second) {
    //        if (symbol.group_id < old_to_new_mapping.size()) {
    //            symbol.group_id = old_to_new_mapping[symbol.group_id];
    //        }
    //    }
    //}

    this->InvalidateCache();
    return true;
}

//void SymbolTable::ReassignGroupIds(const std::vector<std::string> &original_group_names) {
//    // This method is deprecated - group reordering now uses position-based mapping
//    // instead of name-based mapping to handle duplicate group names correctly.
//    // Kept for backward compatibility but no longer used by MoveGroup.
//    (void)original_group_names; // Suppress unused parameter warning
//}
//
//void SymbolTable::ReassignGroupIds() {
//    // This method is deprecated - group reordering now uses position-based mapping
//    // built into MoveGroup() instead of separate name-based reassignment.
//    // Kept for backward compatibility but no longer used.
//}

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

    m_groups[group_id]->group.name = trimmed_name;
    LOGF(SYMBOLS, "Updated group %zu name to: %s\n", group_id, trimmed_name.c_str());
    return true;
}

void SymbolTable::SetGroupAddressSuffixes(size_t group_id, std::vector<std::string> new_address_suffixes) {
    ASSERT(group_id < m_groups.size());

    m_groups[group_id]->group.address_suffixes = std::move(new_address_suffixes);

    this->InvalidateCache();
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

const std::string *SymbolTable::GetSymbolNameForAddress(uint16_t address, uint32_t dso, const std::shared_ptr<const BBCMicroType> &type) const {
    this->EnsureCacheReady(type);

    auto it = m_cache_address_to_symbols.find(address);
    if (it == m_cache_address_to_symbols.end()) {
        return nullptr;
    }

    for (const SymbolsInGroup &in_group : it->second.grouped) {
        if (in_group.lsg->address_suffix_dso_masks.empty()) {
            return &in_group.symbols[0]->name;
        } else {
            for (const LoadedSymbolGroup::DSOMask &mask : in_group.lsg->address_suffix_dso_masks) {
                if ((dso & mask.mask) == mask.value) {
                    return &in_group.symbols[0]->name;
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

    for (const std::unique_ptr<LoadedSymbolGroup> &lsg : m_groups) {
        lsg->address_suffix_dso_masks.clear();

        for (const std::string &address_suffix : lsg->group.address_suffixes) {
            uint32_t dso = 0;

            if (ParseAddressSuffix(&dso, type, address_suffix.c_str(), nullptr)) {
                LoadedSymbolGroup::DSOMask mask;

                mask.mask = GetDSOMaskForOverrides(dso) & type->dso_mask;
                mask.value = dso & type->dso_mask;

                lsg->address_suffix_dso_masks.push_back(mask);
            } else {
                // Should have been called out in the UI. Nothing to be done at
                // this stage but ignore it.
            }
        }

        // TODO eliminate redundant masks.
    }

    m_cache_address_to_symbols.clear();
    m_cache_name_to_addresses.clear();

    for (const std::unique_ptr<LoadedSymbolGroup> &lsg : m_groups) {
        if (!lsg->group.enabled) {
            continue;
        }

        for (size_t i = 0; i < lsg->symbols.size(); ++i) {
            // go in reverse order, so later symbols have priority.
            const Symbol *symbol = &lsg->symbols[lsg->symbols.size() - 1 - i];

            // Handle the address->symbol lookup.
            {
                SymbolsAtAddress *at_address = &m_cache_address_to_symbols[symbol->address];

                // going in group order - so if there's an entry for this group,
                // it'll be the last one in the grouped list.
                SymbolsInGroup *in_group;
                if (at_address->grouped.empty() || at_address->grouped.back().lsg != lsg.get()) {
                    in_group = &at_address->grouped.emplace_back();
                    in_group->lsg = lsg.get();
                } else {
                    in_group = &at_address->grouped.back();
                }

                ASSERT(in_group->lsg == lsg.get());

                in_group->symbols.push_back(symbol);
            }

            // Handle the symbol->address lookup.
            {
                AddressForSymbol addr;

                addr.lsg = lsg.get();
                addr.address = symbol->address;

                m_cache_name_to_addresses[symbol->name].push_back(addr);
            }
        }
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

struct PersistentSymbolTableData {
    std::vector<SymbolGroup> groups;
};
JSON_SERIALIZE(PersistentSymbolTableData, groups);

std::shared_ptr<JSON> SymbolTable::SaveToJSON() const {
    PersistentSymbolTableData p_std;

    for (const std::unique_ptr<LoadedSymbolGroup> &lsg : m_groups) {
        p_std.groups.push_back(lsg->group);
    }

    return std::make_shared<JSON>(p_std);
}

bool SymbolTable::LoadFromJSON(const std::shared_ptr<JSON> &j) {
    if (!j) {
        return false;
    }

    PersistentSymbolTableData p_std;
    std::string error;
    if (!j->Load(&p_std, &error)) {
        LOGF(SYMBOLS, "ERROR: Failed to load symbol groups from JSON: %s\n", error.c_str());
        return false;
    }

    m_groups.clear();
    for (SymbolGroup &group : p_std.groups) {
        this->AddLoadedSymbolGroup(std::move(group), nullptr);
    }

    this->ReloadAllGroups();
    return true;
}

void SymbolTable::ReloadAllGroups() {
    // Clear all symbols but keep groups
    //m_address_to_symbols.clear();
    //m_name_to_addresses.clear();
    this->InvalidateCache();

    // Reload each group from its source file
    for (size_t i = 0; i < m_groups.size(); ++i) {
        const SymbolGroup *group = &m_groups[i]->group;

        // Skip groups without source files (but always load disabled groups for counting)
        if (group->file_path.empty()) {
            continue;
        }

        // Check if file exists
        std::ifstream test_file(group->file_path);
        if (!test_file.is_open()) {
            LOGF(SYMBOLS, "WARNING: Cannot reload group '%s' - file not found: %s\n",
                 group->name.c_str(), group->file_path.c_str());
            continue;
        }
        test_file.close();

        // Read and parse the file
        std::ifstream file(group->file_path);
        std::ostringstream content_stream;
        content_stream << file.rdbuf();
        std::string content = content_stream.str();
        file.close();

        // Load symbols into this group (preserving enabled state)
        LOGF(SYMBOLS, "Reloading group '%s' from: %s (enabled: %s)\n",
             group->name.c_str(), group->file_path.c_str(), BOOL_STR(group->enabled));
        this->LoadFromContent(content, i);
    }

    LOGF(SYMBOLS, "Reloaded %zu symbols across %zu groups\n", GetSymbolCount(), m_groups.size());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::LoadedSymbolGroup *SymbolTable::AddLoadedSymbolGroup(SymbolGroup new_group, size_t *group_index) {
    auto &&lsg = std::make_unique<LoadedSymbolGroup>();

    lsg->group = std::move(new_group);

    if (group_index) {
        *group_index = m_groups.size();
    }

    SymbolTable::LoadedSymbolGroup *lsg_ptr = lsg.get();
    m_groups.push_back(std::move(lsg));

    return lsg_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
