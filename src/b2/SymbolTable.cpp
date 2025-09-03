#include <shared/system.h>
#include "conf.h"

#if BBCMICRO_DEBUGGER

#include "nlohmann_json_wrapper.h"
#include "SymbolTable.h"
#include <shared/debug.h>
#include "memory_contexts.h"
#include <shared/log.h>
#include <regex>
#include <algorithm>
#include <beeb/type.h>
#include <shared/file_io.h>
#include <sstream>

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
    m_files.clear();
    this->InvalidateCache();
    LOGF(SYMBOLS, "Symbol table cleared\n");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromFile(const std::string &filepath, const SymbolParser *parser, const LogSet *logs) {
    LOGF(SYMBOLS, "Loading symbols from: %s\n", filepath.c_str());

    std::string content;
    if (!LoadTextFile(&content, filepath, logs)) {
        return false;
    }

    if (!this->LoadFromString(content, filepath, parser)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromString(const std::string &content, const std::string &filepath, const SymbolParser *parser) {
    size_t file_index;
    LoadedSymbolFile *lsf;
    {
        SymbolFile new_file;

        new_file.file_path = filepath;
        if (parser) {
            new_file.file_format_name = parser->GetFormatName();
        }

        lsf = this->AddLoadedSymbolFile(std::move(new_file), &file_index);
    }

    size_t old_count = GetSymbolCount();

    // Detect format and load with appropriate parser
    bool success = LoadFromContent(content, file_index);

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

bool SymbolTable::LoadFromContent(const std::string &content, size_t file_index) {
    LoadedSymbolFile *lsf = m_files[file_index].get();

    const SymbolParser *parser = SymbolParserRegistry::FindParserByFormatName(lsf->file.file_format_name);
    if (!parser) {
        parser = DetectBestParser(content);
    }

    if (!parser) {
        LOGF(SYMBOLS, "WARNING: No suitable parser found\n");
        return false;
    }

    this->InvalidateCache();

    LOGF(SYMBOLS, "Using %s parser for content loading\n", parser->GetFormatName().c_str());
    lsf->symbols = parser->ParseContent(content);

    if (lsf->symbols.empty()) {
        LOGF(SYMBOLS, "WARNING: No symbols loaded from file\n");
        return true;
    }

    auto &&symbol_it = lsf->symbols.begin();
    while (symbol_it != lsf->symbols.end()) {
        Symbol *symbol = &*symbol_it;

        if (symbol->name.empty()) {
            LOGF(SYMBOLS, "WARNING: Invalid symbol at line %zu: address=$%X, name='%s'\n",
                 symbol->line_number, symbol->address, symbol->name.c_str());

            symbol_it = lsf->symbols.erase(symbol_it);
            continue;
        }

        ++symbol_it;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetSymbolCount() const {
    size_t n = 0;

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_files) {
        n += lsf->symbols.size();
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetEnabledSymbolCount() const {
    size_t n = 0;

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_files) {
        if (lsf->file.enabled) {
            n += lsf->symbols.size();
        }
    }

    return n;
}

size_t SymbolTable::GetSymbolCountForFile(size_t file_index) const {
    ASSERT(file_index < m_files.size());
    return m_files[file_index]->symbols.size();
}

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

    *addr_ptr = addr->address;

    if (!addr->lsf->address_suffix_dso_masks.empty()) {
        const LoadedSymbolFile::DSOMask *mask = &addr->lsf->address_suffix_dso_masks[0];

        *dso_ptr &= ~mask->mask;
        *dso_ptr |= mask->value;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::IsValidAddress(uint32_t addr) const {
    // BBC Micro has 16-bit address space
    return addr <= 0xFFFF;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// File Management Methods

bool SymbolTable::RemoveFile(size_t file_index) {
    if (file_index >= m_files.size()) {
        return false;
    }

    m_files.erase(m_files.begin() + static_cast<std::vector<SymbolFile>::difference_type>(file_index));

    this->InvalidateCache();
    return true;
}

void SymbolTable::EnableFile(size_t file_index, bool enabled) {
    ASSERT(file_index < m_files.size());
    m_files[file_index]->file.enabled = enabled;
    this->InvalidateCache();
}

const SymbolFile *SymbolTable::GetFileByIndex(size_t file_index) const {
    ASSERT(file_index < m_files.size());
    return &m_files[file_index]->file;
}

size_t SymbolTable::GetNumFiles() const {
    return m_files.size();
}

bool SymbolTable::MoveFile(size_t from_index, size_t to_index) {
    if (from_index >= m_files.size() || to_index >= m_files.size() || from_index == to_index) {
        return false;
    }

    {
        std::unique_ptr<LoadedSymbolFile> file_to_move = std::move(m_files[from_index]);
        m_files.erase(m_files.begin() + static_cast<std::vector<SymbolFile>::difference_type>(from_index));
        m_files.insert(m_files.begin() + static_cast<std::vector<SymbolFile>::difference_type>(to_index), std::move(file_to_move));
    }

    this->InvalidateCache();
    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// File Metadata Editing Methods

void SymbolTable::SetFileGroupIndex(size_t file_index, uint8_t group_index) {
    ASSERT(file_index < m_files.size());

    m_files[file_index]->file.group_index = group_index;
}

void SymbolTable::SetFileAddressSuffixes(size_t file_index, std::vector<std::string> new_address_suffixes) {
    ASSERT(file_index < m_files.size());

    m_files[file_index]->file.address_suffixes = std::move(new_address_suffixes);

    this->InvalidateCache();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Context-Aware Symbol Lookup Methods

const std::string *SymbolTable::GetSymbolNameForAddress(uint16_t address, uint32_t dso, const std::shared_ptr<const BBCMicroType> &type) const {
    this->EnsureCacheReady(type);

    auto it = m_cache_address_to_symbols.find(address);
    if (it == m_cache_address_to_symbols.end()) {
        return nullptr;
    }

    for (const SymbolsInFile &in_file : it->second.per_file) {
        if (in_file.lsf->address_suffix_dso_masks.empty()) {
            return &in_file.symbols[0]->name;
        } else {
            for (const LoadedSymbolFile::DSOMask &mask : in_file.lsf->address_suffix_dso_masks) {
                if ((dso & mask.mask) == mask.value) {
                    return &in_file.symbols[0]->name;
                }
            }
        }
    }

    return nullptr;
}

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

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_files) {
        lsf->address_suffix_dso_masks.clear();

        for (const std::string &address_suffix : lsf->file.address_suffixes) {
            uint32_t dso = 0;

            if (ParseAddressSuffix(&dso, type, address_suffix.c_str(), nullptr)) {
                LoadedSymbolFile::DSOMask mask;

                mask.mask = GetDSOMaskForOverrides(dso) & type->dso_mask;
                mask.value = dso & type->dso_mask;

                lsf->address_suffix_dso_masks.push_back(mask);
            } else {
                // Should have been called out in the UI. Nothing to be done at
                // this stage but ignore it.
            }
        }

        // TODO eliminate redundant masks.
    }

    m_cache_address_to_symbols.clear();
    m_cache_name_to_addresses.clear();

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_files) {
        if (!lsf->file.enabled) {
            continue;
        }

        for (size_t i = 0; i < lsf->symbols.size(); ++i) {
            // go in reverse order, so later symbols have priority.
            const Symbol *symbol = &lsf->symbols[lsf->symbols.size() - 1 - i];

            // Handle the address->symbol lookup.
            {
                SymbolsAtAddress *at_address = &m_cache_address_to_symbols[symbol->address];

                // going in file order - so if there's an entry for this file,
                // it'll be the last one in the per-file list.
                SymbolsInFile *in_file;
                if (at_address->per_file.empty() || at_address->per_file.back().lsf != lsf.get()) {
                    in_file = &at_address->per_file.emplace_back();
                    in_file->lsf = lsf.get();
                } else {
                    in_file = &at_address->per_file.back();
                }

                ASSERT(in_file->lsf == lsf.get());

                in_file->symbols.push_back(symbol);
            }

            // Handle the symbol->address lookup.
            {
                AddressForSymbol addr;

                addr.lsf = lsf.get();
                addr.address = symbol->address;

                m_cache_name_to_addresses[symbol->name].push_back(addr);
            }
        }
    }

    m_cache_type = type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Persistence Methods

struct PersistentSymbolTableData {
    std::vector<SymbolFile> groups; //no, the "groups" naming is not ideal, but there it is...

    // As well as the naming being not ideal, this ends up as a large array, as
    // there are always 256 entries in it.
    std::vector<SymbolGroup> groups2;
};
JSON_SERIALIZE(PersistentSymbolTableData, groups, groups2);

std::shared_ptr<JSON> SymbolTable::SaveToJSON() const {
    PersistentSymbolTableData p_std;

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_files) {
        p_std.groups.push_back(lsf->file);
    }

    for (const SymbolGroup &group : m_groups) {
        p_std.groups2.push_back(group);
    }

    return std::make_shared<JSON>(p_std);
}

bool SymbolTable::LoadFromJSON(const std::shared_ptr<JSON> &j, const LogSet *logs) {
    if (!j) {
        return false;
    }

    PersistentSymbolTableData p_std;
    std::string error;
    if (!j->Load(&p_std, &error)) {
        logs->e.f("Failed to load symbol file data from JSON: %s\n", error.c_str());
        return false;
    }

    m_files.clear();
    for (SymbolFile &file : p_std.groups) {
        this->AddLoadedSymbolFile(std::move(file), nullptr);
    }

    for (size_t i = 0; i < MAX_NUM_SYMBOL_FILE_GROUPS; ++i) {
        if (i < p_std.groups2.size()) {
            m_groups[i] = p_std.groups2[i];
        } else {
            m_groups[i] = SymbolGroup();
        }
    }

    this->ReloadAllFiles(logs);
    return true;
}

void SymbolTable::ReloadAllFiles(const LogSet *logs) {
    this->InvalidateCache();

    // Reload each file from its source file
    for (size_t i = 0; i < m_files.size(); ++i) {
        const SymbolFile *file = &m_files[i]->file;

        std::string content;
        if (!LoadTextFile(&content, file->file_path, logs)) {
            continue;
        }

        // Load symbols into this group (preserving enabled state)
        LOGF(SYMBOLS, "Reloading symbols from: %s (enabled: %s)\n", file->file_path.c_str(), BOOL_STR(file->enabled));
        this->LoadFromContent(content, i);
    }

    LOGF(SYMBOLS, "Reloaded %zu symbols across %zu files\n", GetSymbolCount(), m_files.size());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::LoadedSymbolFile *SymbolTable::AddLoadedSymbolFile(SymbolFile new_file, size_t *file_index) {
    auto &&lsf = std::make_unique<LoadedSymbolFile>();

    lsf->file = std::move(new_file);

    if (file_index) {
        *file_index = m_files.size();
    }

    SymbolTable::LoadedSymbolFile *lsf_ptr = lsf.get();
    m_files.push_back(std::move(lsf));

    return lsf_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
