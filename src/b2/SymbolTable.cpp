#include <shared/system.h>
#include "conf.h"

#if BBCMICRO_DEBUGGER

#include "SymbolTable.h"
#include <shared/debug.h>
#include <shared/log.h>
#include <regex>
#include <algorithm>
#include <beeb/type.h>
#include <shared/file_io.h>
#include <sstream>
#include "misc.h"
#include <shared/strings.h>
#include "native_ui.h"

#include <shared/enum_def.h>
#include "SymbolTable.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class StringType>
static StringType TrimWhitespace(const StringType &str) {
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

//static const std::string ADDRESS_SUFFIXES = "address_suffixes";

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

SymbolTable::SymbolTable(const SymbolTable &src) {
    for (const std::unique_ptr<LoadedSymbolFile> &src_lsf : src.m_lsfs) {
        m_lsfs.push_back(std::make_unique<LoadedSymbolFile>(*src_lsf));
    }

    // ...and the cached stuff sort itself out on first use.
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::Clear() {
    //m_address_to_symbols.clear();
    //m_name_to_addresses.clear();
    m_lsfs.clear();
    this->InvalidateEverything();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromFile(const std::string &filepath, const SymbolParser *parser, const LogSet *logs, size_t *file_index_ptr) {
    std::string content;
    if (!LoadTextFile(&content, filepath, logs)) {
        return false;
    }

    if (!this->LoadFromString(content, filepath, parser, logs, file_index_ptr)) {
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromString(const std::string &content, const std::string &filepath, const SymbolParser *parser, const LogSet *logs, size_t *file_index_ptr) {
    size_t file_index;
    {
        SymbolFile new_file;

        new_file.file_path = filepath;
        if (parser) {
            new_file.file_format_name = parser->GetFormatName();
        }

        this->AddLoadedSymbolFile(std::move(new_file), &file_index);
    }

    size_t old_count = GetSymbolCount();

    // Detect format and load with appropriate parser
    bool success = this->LoadFromContent(content, file_index, logs);

    if (success) {
        if (logs) {
            size_t new_count = GetSymbolCount();
            logs->i.f("Successfully loaded %zu symbols (%zu symbols total)\n", new_count - old_count, new_count);
        }

        if (file_index_ptr) {
            *file_index_ptr = file_index;
        }
    }

    return success;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::SymbolParser::SymbolParser(const Guid &guid_)
    : guid(guid_) {
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
        if (strcasecmp(parser->GetFormatName().c_str(), format_name.c_str()) == 0) {
            return parser.get();
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void LogParseError(const std::string &file_path, size_t line_number, const LogSet *logs) {
    if (logs) {
        logs->e.EnsureBOL();
        logs->e.f("Error in line: %zu of %s\n", line_number, file_path.c_str());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Concrete parser implementations

class BeebAsmParser : public SymbolTable::SymbolParser {
    struct ParseState {
        const char *c = nullptr;
        const std::string *file_path = nullptr;
        size_t line_number = 1;
        const LogSet *logs = nullptr;
    };

  public:
    BeebAsmParser()
        : SymbolParser({0xf5, 0xa9, 0x9d, 0xca, 0x28, 0x34, 0x40, 0x86, 0x9f, 0x4b, 0xfd, 0xba, 0x78, 0xf1, 0x65, 0x2f}) {
    }

    std::string GetFormatName() const override {
        return "BeebAsm";
    }

    std::string GetDisplayName() const override {
        return "BeebAsm";
    }

    std::vector<std::string> GetSuggestedFileExtensions() const override {
        return {".labels", ".txt"};
    }

    bool MatchesLine(const std::string &line) const override {
        if (line.substr(0, 3) == "[{'") {
            return true;
        } else {
            return false;
        }
    }

    bool ParseSymbolsFromContent(std::vector<Symbol> *symbols, const std::string &content, const std::string &file_path, const LogSet *logs) const override {
        ParseState ps;

        ps.c = content.c_str();
        ps.file_path = &file_path;
        ps.logs = logs;

        if (!this->SkipSpacesAndConsumeMatch(&ps, '[')) {
            return this->Error(ps, "didn't find opening [");
        }

        if (!this->SkipSpacesAndConsumeMatch(&ps, '{')) {
            return this->Error(ps, "didn't find opening {");
        }

        for (;;) {
            if (!this->SkipSpacesAndConsumeMatch(&ps, '\'')) {
                return this->Error(ps, "didn't find opening '");
            }

            Symbol symbol;
            symbol.line_number = ps.line_number;

            // Just treat everything up to the closing ' as the symbol and pass any
            // UTF-8 issues to further along the chain.
            //
            // BeebAsm doesn't do any quoting for the symbol names, but ' isn't
            // valid in symbol names anyway.
            const char *symbol_begin = ps.c;
            while (*ps.c != 0 && *ps.c != '\'') {
                ++ps.c;
            }

            // BeebAsm labels always have a "." prefix. b2 treats this as a
            // syntactic element, necesasry for disambiguation in the original
            // source file,but not part of the name (any more than the ";"
            // suffix might be in other assemblers).
            //
            // So it will accept names that don't start with ., but it'll strip
            // out the leading . when there is one.
            if (*symbol_begin == '.') {
                ++symbol_begin;
            }

            // the label terminated either way.
            symbol.name.assign(symbol_begin, ps.c);

            if (*ps.c != '\'') {
                return this->Error(ps, "didn't find closing ' for symbol: %s", symbol.name.c_str());
            }

            ++ps.c; //skip '

            if (!this->SkipSpacesAndConsumeMatch(&ps, ':')) {
                return this->Error(ps, "didn't find : for symbol: %s", symbol.name.c_str());
            }

            this->SkipSpaces(&ps);
            if (!isdigit(*ps.c)) {
                return this->Error(ps, "invalid non-digit value for symbol: %s", symbol.name.c_str());
            }

            const char *value_begin = ps.c;
            while (isdigit(*ps.c)) {
                ++ps.c;
            }

            // the value terminated either way.
            std::string value_str(value_begin, ps.c);

            if (*ps.c == 0) {
                return this->Error(ps, "reached eof during value for symbol: %s", symbol.name.c_str());
            }

            // take the L when there is one.
            if (*ps.c == 'L') {
                ++ps.c;
            }

            // interpret the value.
            uint32_t value;
            if (!GetUInt32FromString(&value, value_str)) {
                // unlikely to occur with the current code, since it already carefully checked that it's all digits...
                return this->Error(ps, "invalid value for symbol \"%s\": %s", symbol.name.c_str(), value_str.c_str());
            }

            // actually looks like that was a valid symbol.
            symbol.address = (uint16_t)value;
            symbols->push_back(symbol);

            this->SkipSpaces(&ps);
            if (*ps.c == ',') {
                ++ps.c; //skip ,
                // more entries to come, you hope.
                continue;
            } else if (*ps.c == '}') {
                // end of input signalled.
                ++ps.c; //skip }
                break;
            } else {
                return this->Error(ps, "syntax error after symbol: %s", symbol.name.c_str());
            }
        }

        if (!this->SkipSpacesAndConsumeMatch(&ps, ']')) {
            return this->Error(ps, "didn't find closing ]");
        }

        this->SkipSpaces(&ps);
        if (*ps.c != 0) {
            return this->Error(ps, "syntax error after closing ]");
        }

        return true;
    }

  protected:
  private:
    bool Error(const ParseState &ps, const char *fmt, ...) const PRINTF_LIKE(3, 4) {
        if (ps.logs) {
            va_list v;
            va_start(v, fmt);
            ps.logs->e.v(fmt, v);
            va_end(v);
            LogParseError(*ps.file_path, ps.line_number, ps.logs);
        }

        return false;
    }

    bool SkipSpacesAndConsumeMatch(ParseState *ps, char term) const {
        this->SkipSpaces(ps);

        if (*ps->c == term) {
            ++ps->c;
            return true;
        } else {
            return false;
        }
    }

    void SkipSpaces(ParseState *ps) const {
        while (*ps->c != 0 && isspace(*ps->c)) {
            if (*ps->c == '\r' || *ps->c == '\n') {
                ++ps->line_number;

                // Skip an extra byte if it looks like a 2-byte line ending.
                if ((ps->c[1] == '\r' || ps->c[1] == '\n') && ps->c[1] != *ps->c) {
                    ++ps->c;
                }
            }
            ++ps->c;
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ViceParser : public SymbolTable::SymbolParser {
  public:
    ViceParser()
        : SymbolParser({0xbf, 0xb6, 0xd5, 0xe6, 0xe9, 0x79, 0x46, 0x82, 0x98, 0xd2, 0x59, 0x0f, 0x53, 0xc9, 0x2f, 0xc7}) {
    }

    std::string GetDisplayName() const override {
        return "VICE";
    }

    std::string GetFormatName() const override {
        return "VICE";
    }

    std::vector<std::string> GetSuggestedFileExtensions() const override {
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

    bool ParseSymbolsFromContent(std::vector<Symbol> *symbols, const std::string &content, const std::string &file_path, const LogSet *logs) const override {
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

                    symbols->push_back(std::move(symbol));
                } catch (const std::exception &e) {
                    if (logs) {
                        logs->e.f("Parse error: %s\n", e.what());
                        LogParseError(file_path, line_number, logs);
                    }
                    return false;
                }
            } else {
                // Only log non-empty, non-comment lines that don't match
                std::string trimmed = TrimWhitespace(line);
                if (!trimmed.empty()) {
                    if (logs) {
                        logs->e.f("Unrecognised format: %s\n", trimmed.c_str());
                        LogParseError(file_path, line_number, logs);
                    }
                }
            }
        }

        return true;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class AcmeParser : public SymbolTable::SymbolParser {
  public:
    AcmeParser()
        : SymbolParser({0x46, 0x9b, 0x95, 0xd1, 0x75, 0x16, 0x44, 0xd5, 0xa2, 0x0a, 0x9a, 0xbe, 0x3e, 0x36, 0xa5, 0x94}) {
    }

    std::string GetDisplayName() const override {
        return "ACME";
    }

    std::string GetFormatName() const override {
        return "ACME";
    }

    std::vector<std::string> GetSuggestedFileExtensions() const override {
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

    bool ParseSymbolsFromContent(std::vector<Symbol> *symbols, const std::string &content, const std::string &file_path, const LogSet *logs) const override {
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

                    symbols->push_back(std::move(symbol));
                } catch (const std::exception &e) {
                    if (logs) {
                        logs->e.f("Parse error: %s\n", e.what());
                        LogParseError(file_path, line_number, logs);
                    }
                    return false;
                }
            } else {
                // Only log non-empty, non-comment lines that don't match
                std::string trimmed = TrimWhitespace(line);
                if (!trimmed.empty()) {
                    if (logs) {
                        logs->e.f("Unrecognised format: %s\n", trimmed.c_str());
                        LogParseError(file_path, line_number, logs);
                    }
                }
            }
        }

        return true;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TassLabelsParser : public SymbolTable::SymbolParser {
  public:
    TassLabelsParser()
        : SymbolParser({0x1b, 0xd0, 0x2f, 0xe8, 0x5a, 0x96, 0x47, 0xcd, 0x83, 0x23, 0x9e, 0x70, 0xe1, 0xe6, 0x92, 0x61}) {
    }

    std::string GetFormatName() const override {
        return "64tass_labels";
    }

    std::string GetDisplayName() const override {
        return "64tass labels";
    }

    std::vector<std::string> GetSuggestedFileExtensions() const override {
        return {".lbl", ".sym"};
    }

    bool MatchesLine(const std::string &) const override {
        return false;
    }

    bool ParseSymbolsFromContent(std::vector<Symbol> *symbols, const std::string &content, const std::string &file_path, const LogSet *logs) const override {
        size_t line_number = 0;
        bool good = ForEachLine(content, [symbols, &file_path, logs, &line_number](const std::string_view &line) -> bool {
            ++line_number;

            if (line.empty()) {
                return true;
            }

            std::string_view::size_type name_end = line.find_first_of('=');
            if (name_end == std::string_view::npos) {
                if (logs) {
                    logs->e.f("Invalid syntax: %-*s\n", (int)line.size(), line.data());
                    LogParseError(file_path, line_number, logs);
                }
                return false;
            }

            std::string_view::size_type value_begin = name_end + 1;

            if (name_end > 0 && line[name_end - 1] == ':') {
                // It's :=.
                --name_end;
            }

            std::string_view name = TrimWhitespace(line.substr(0, name_end));
            std::string value_str(TrimWhitespace(line.substr(value_begin)));

            if (name.empty() || value_str.empty()) {
                if (logs) {
                    logs->e.f("Invalid syntax: %-*s\n", (int)line.size(), line.data());
                    LogParseError(file_path, line_number, logs);
                }
                return false;
            }

            bool got_value = false;
            uint64_t value = 0;
            if (value_str[0] == '$') {
                if (GetUInt64FromString(&value, value_str.c_str() + 1, 16)) {
                    got_value = true;
                } else {
                    if (logs) {
                        logs->e.f("Invalid hex value: %s\n", value_str.c_str());
                        LogParseError(file_path, line_number, logs);
                        return false;
                    }
                }
            } else if (value_str[0] == '"') {
                // Ignore string values.
            } else if (value_str == "true" || value_str == "false") {
                // Ignore boolean values.
            } else {
                // Assume decimal?
                if (GetUInt64FromString(&value, value_str.c_str(), 10)) {
                    got_value = true;
                } else {
                    if (logs) {
                        logs->e.f("Invalid hex value: %s\n", value_str.c_str());
                        LogParseError(file_path, line_number, logs);
                        return false;
                    }
                }
            }

            if (got_value) {
                if (value > 0xffff) {
                    // For now, silently ignore values that are too large.
                } else {
                    Symbol symbol;

                    symbol.line_number = line_number;
                    symbol.name = name;
                    symbol.address = (uint16_t)value;

                    symbols->push_back(std::move(symbol));
                }
            }

            return true;
        });

        return good;
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::SymbolParserRegistry::InitializeBuiltinParsers() {
    if (s_parsers.empty()) {
        RegisterParser(std::make_unique<ViceParser>());
        RegisterParser(std::make_unique<AcmeParser>());
        RegisterParser(std::make_unique<BeebAsmParser>());
        RegisterParser(std::make_unique<TassLabelsParser>());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const SymbolTable::SymbolParser *SymbolTable::DetectBestParser(const std::string &content) {
    const auto &parsers = SymbolParserRegistry::GetParsers();
    ASSERT(!parsers.empty());

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
        if (parser_scores[i] > max_score) {
            max_score = parser_scores[i];
            best_parser_index = i;
        }
    }

    // Check if best parser meets confidence threshold
    if (lines_checked > 0 && max_score >= (confidence_threshold * lines_checked)) {
        found_parser = true;
    }

    return found_parser ? parsers[best_parser_index].get() : nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SymbolTable::LoadFromContent(const std::string &content, size_t file_index, const LogSet *logs) {
    LoadedSymbolFile *lsf = m_lsfs[file_index].get();

    const SymbolParser *parser = SymbolParserRegistry::FindParserByFormatName(lsf->file.file_format_name);
    if (!parser) {
        parser = DetectBestParser(content);
    }

    if (!parser) {
        if (logs) {
            logs->e.f("No suitable parser found for %s\n", lsf->file.file_path.c_str());
        }
        return false;
    }

    this->InvalidateEverything();

    lsf->symbols.clear();

    if (!parser->ParseSymbolsFromContent(&lsf->symbols, content, lsf->file.file_path, logs)) {
        return false;
    }

    if (lsf->symbols.empty()) {
        if (logs) {
            logs->w.f("No symbols loaded from file: %s\n", lsf->file.file_path.c_str());
        }
    }

    auto &&symbol_it = lsf->symbols.begin();
    while (symbol_it != lsf->symbols.end()) {
        Symbol *symbol = &*symbol_it;

        if (symbol->name.empty()) {
            if (logs) {
                logs->e.f("Invalid symbol in file: line %zu of %s\n", symbol->line_number, lsf->file.file_path.c_str());
            }

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

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_lsfs) {
        n += lsf->symbols.size();
    }

    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t SymbolTable::GetEnabledSymbolCount() const {
    size_t n = 0;

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_lsfs) {
        if (lsf->file.enabled) {
            n += lsf->symbols.size();
        }
    }

    return n;
}

size_t SymbolTable::GetSymbolCountForFile(size_t file_index) const {
    ASSERT(file_index < m_lsfs.size());
    return m_lsfs[file_index]->symbols.size();
}

uint64_t SymbolTable::GetSymbolsChangedCounter() const {
    return m_symbols_changed_counter;
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
        const DSOMask *mask = &addr->lsf->address_suffix_dso_masks[0];

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
    if (file_index >= m_lsfs.size()) {
        return false;
    }

    m_lsfs.erase(m_lsfs.begin() + static_cast<std::vector<SymbolFile>::difference_type>(file_index));

    this->InvalidateEverything();
    return true;
}

void SymbolTable::EnableFile(size_t file_index, bool enabled) {
    ASSERT(file_index < m_lsfs.size());
    SymbolFile *file = &m_lsfs[file_index]->file;

    if (file->enabled != enabled) {
        file->enabled = enabled;

        this->InvalidateEverything();
    }
}

const SymbolFile *SymbolTable::GetFileByIndex(size_t file_index) const {
    ASSERT(file_index < m_lsfs.size());
    return &m_lsfs[file_index]->file;
}

size_t SymbolTable::GetNumFiles() const {
    return m_lsfs.size();
}

bool SymbolTable::MoveFile(size_t from_index, size_t to_index) {
    if (from_index >= m_lsfs.size() || to_index >= m_lsfs.size() || from_index == to_index) {
        return false;
    }

    {
        std::unique_ptr<LoadedSymbolFile> file_to_move = std::move(m_lsfs[from_index]);
        m_lsfs.erase(m_lsfs.begin() + static_cast<std::vector<SymbolFile>::difference_type>(from_index));
        m_lsfs.insert(m_lsfs.begin() + static_cast<std::vector<SymbolFile>::difference_type>(to_index), std::move(file_to_move));
    }

    this->InvalidateEverything();
    return true;
}

size_t SymbolTable::GetNumSymbolsInFile(size_t file_index) const {
    ASSERT(file_index < m_lsfs.size());
    return m_lsfs[file_index]->symbols.size();
}

const Symbol *SymbolTable::GetSymbolInFileByIndex(size_t file_index, size_t symbol_index) const {
    ASSERT(file_index < m_lsfs.size());
    ASSERT(symbol_index < m_lsfs[file_index]->symbols.size());
    return &m_lsfs[file_index]->symbols[symbol_index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// File Metadata Editing Methods

void SymbolTable::SetFileGroupIndex(size_t file_index, uint8_t group_index) {
    ASSERT(file_index < m_lsfs.size());
    SymbolFile *file = &m_lsfs[file_index]->file;

    if (file->group_index != group_index) {
        file->group_index = group_index;

        this->InvalidateEverything();
    }
}

void SymbolTable::SetFileAddressSuffixes(size_t file_index, std::vector<std::string> new_address_suffixes) {
    ASSERT(file_index < m_lsfs.size());

    m_lsfs[file_index]->file.address_suffixes = std::move(new_address_suffixes);

    this->InvalidateEverything();
}

void SymbolTable::SetFileAddressSuffixMode(size_t file_index, SymbolFileAddressSuffixMode address_suffix_mode) {
    ASSERT(file_index < m_lsfs.size());

    m_lsfs[file_index]->file.address_suffix_mode = address_suffix_mode;

    this->InvalidateEverything();
}

const SymbolGroup *SymbolTable::GetSymbolGroupByIndex(uint8_t group_index) const {
    this->EnsureGroupPropertiesValid();

    return &m_groups[group_index];
}

void SymbolTable::SetGroupEnabled(uint8_t group_index, bool enabled) {
    for (size_t file_index = 0; file_index < m_lsfs.size(); ++file_index) {
        if (m_lsfs[file_index]->file.group_index == group_index) {
            this->EnableFile(file_index, enabled);
        }
    }
}

std::string *SymbolTable::GetGroupMutableName(uint8_t group_index) {
    return &m_groups[group_index].name;
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
        if (!in_file.dso_masks) {
            return &in_file.symbols[0]->name;
        } else {
            for (const DSOMask &mask : *in_file.dso_masks) {
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

SymbolDetails SymbolTable::GetSymbolDetailsForAddress(uint16_t address, uint32_t dso, const std::shared_ptr<const BBCMicroType> &type) const {
    SymbolDetails details;

    this->EnsureCacheReady(type);

    auto it = m_cache_address_to_symbols.find(address);
    if (it == m_cache_address_to_symbols.end()) {
        return details;
    }

    // Iterate through all files that have symbols at this address
    for (const SymbolsInFile &in_file : it->second.per_file) {
        // Check if this file's symbols apply to the current context (dso)
        bool applies_to_context = false;

        if (in_file.lsf->address_suffix_dso_masks.empty()) {
            // No context restrictions - applies everywhere
            applies_to_context = true;
        } else {
            // Check if any of the address suffixes match the current dso
            for (const DSOMask &mask : in_file.lsf->address_suffix_dso_masks) {
                if ((dso & mask.mask) == mask.value) {
                    applies_to_context = true;
                    break;
                }
            }
        }

        // Only include symbols that apply to the current context
        if (applies_to_context) {
            // Add all symbols from this file at this address
            for (const Symbol *symbol : in_file.symbols) {
                SymbolDetails::SymbolInFile sif;
                sif.symbol_name = symbol->name;
                sif.file_path = in_file.lsf->file.file_path;
                sif.address_suffixes = in_file.lsf->file.address_suffixes;
                details.symbols.push_back(std::move(sif));
            }
        }
    }

    return details;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Cache Management Methods

void SymbolTable::InvalidateEverything() const {
    m_cache_type.reset();

    this->InvalidateGroupProperties();
}

void SymbolTable::InvalidateGroupProperties() const {
    m_group_properties_valid = false;
}

struct VectorBoolLessThan {
    inline bool operator()(const std::vector<bool> &a, const std::vector<bool> &b) const {
        if (a.size() < b.size()) {
            return true; //a<b
        } else if (b.size() < a.size()) {
            return false; //b<a
        }

        for (size_t i = 0; i < a.size(); ++i) {
            bool ai = a[i], bi = b[i];

            if (!ai && bi) {
                return true; //a<b
            } else if (ai && !bi) {
                return false; //b<a
            }
        }

        return false; //a==b
    }
};

void SymbolTable::EnsureCacheReady(const std::shared_ptr<const BBCMicroType> &type) const {
    if (m_cache_type == type) {
        return;
    }

    ++m_symbols_changed_counter;

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_lsfs) {
        lsf->address_suffix_dso_masks.clear();

        for (const std::string &address_suffix : lsf->file.address_suffixes) {
            uint32_t dso = 0;

            if (ParseAddressSuffix(&dso, type, address_suffix.c_str(), nullptr)) {
                DSOMask mask;

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
    m_interned_dso_mask_table.clear();

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_lsfs) {
        if (!lsf->file.enabled) {
            continue;
        }

        std::vector<bool> dso_masks_used;
        dso_masks_used.resize(lsf->address_suffix_dso_masks.size());

        // Outre map used to assemble the minimal set of DSO mask tables.
        //
        // The key is 1 bit per LSF's DSO mask - 1 if that DSO is in the value, 0 if it isn't.
        std::map<std::vector<bool>, std::unique_ptr<std::vector<DSOMask>>, VectorBoolLessThan> dsos_by_dso_used_flags;

        for (size_t symbol_index = 0; symbol_index < lsf->symbols.size(); ++symbol_index) {
            // go in reverse order, so later symbols have priority.
            const Symbol *symbol = &lsf->symbols[lsf->symbols.size() - 1 - symbol_index];

            // Handle the address->symbol lookup.
            {
                SymbolsAtAddress *at_address = &m_cache_address_to_symbols[symbol->address];

                // going in file order - so if there's an entry for this file,
                // it'll be the last one in the per-file list.
                SymbolsInFile *in_file;
                if (at_address->per_file.empty() || at_address->per_file.back().lsf != lsf.get()) {
                    in_file = &at_address->per_file.emplace_back();
                    in_file->lsf = lsf.get();

                    switch (in_file->lsf->file.address_suffix_mode) {
                    default:
                        ASSERT(false);
                        break;

                    case SymbolFileAddressSuffixMode_Exclusive:
                        in_file->dso_masks = &in_file->lsf->address_suffix_dso_masks;
                        break;

                    case SymbolFileAddressSuffixMode_Inclusive:
                        {
                            bool any = false;
                            for (size_t dso_mask_index = 0; dso_mask_index < in_file->lsf->address_suffix_dso_masks.size(); ++dso_mask_index) {
                                bool affects = DoesDSOAffectAddress(type, in_file->lsf->address_suffix_dso_masks[dso_mask_index].value, symbol->address);
                                dso_masks_used[dso_mask_index] = affects;
                                any = any || affects;
                            }

                            if (any) {
                                std::unique_ptr<std::vector<DSOMask>> *masks = &dsos_by_dso_used_flags[dso_masks_used];

                                if (!*masks) {
                                    *masks = std::make_unique<std::vector<DSOMask>>();

                                    for (size_t i = 0; i < in_file->lsf->address_suffix_dso_masks.size(); ++i) {
                                        if (dso_masks_used[i]) {
                                            (*masks)->push_back(in_file->lsf->address_suffix_dso_masks[i]);
                                        }
                                    }
                                }

                                in_file->dso_masks = masks->get();
                            }
                        }

                        break;
                    }

                    if (in_file->dso_masks) {
                        if (in_file->dso_masks->empty()) {
                            // no point actually pointing to it.
                            in_file->dso_masks = nullptr;
                        }
                    }
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

        // Copy the minimal set of DSO mask tables.
        //
        // TODO: there's potentially some duplication here, but not much point trying too hard. There'll be only so many combinations.
        for (auto &flags_and_masks : dsos_by_dso_used_flags) {
            m_interned_dso_mask_table.push_back(std::move(flags_and_masks.second));
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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PersistentSymbolTableData, groups, groups2);

nlohmann::json SymbolTable::SaveToJSON() const {
    PersistentSymbolTableData p_std;

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_lsfs) {
        p_std.groups.push_back(lsf->file);
    }

    for (const SymbolGroup &group : m_groups) {
        p_std.groups2.push_back(group);
    }

    return p_std;
}

bool SymbolTable::LoadFromJSON(const nlohmann::json &j, const LogSet *logs) {
    PersistentSymbolTableData p_std;
    std::string error;
    if (!LoadJSON(&p_std, j, &error)) {
        logs->e.f("Failed to load symbol file data from JSON: %s\n", error.c_str());
        return false;
    }

    m_lsfs.clear();
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
    this->InvalidateEverything();

    // Reload each file from its source file
    for (size_t i = 0; i < m_lsfs.size(); ++i) {
        const SymbolFile *file = &m_lsfs[i]->file;

        std::string content;
        if (!LoadTextFile(&content, file->file_path, logs)) {
            continue;
        }

        // Load symbols into this group (preserving enabled state)
        this->LoadFromContent(content, i, logs);
    }

    if (logs) {
        logs->i.f("Reloaded %zu symbols across %zu files\n", GetSymbolCount(), m_lsfs.size());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SymbolTable::LoadedSymbolFile *SymbolTable::AddLoadedSymbolFile(SymbolFile new_file, size_t *file_index) {
    auto &&lsf = std::make_unique<LoadedSymbolFile>();

    lsf->file = std::move(new_file);

    if (file_index) {
        *file_index = m_lsfs.size();
    }

    SymbolTable::LoadedSymbolFile *lsf_ptr = lsf.get();
    m_lsfs.push_back(std::move(lsf));

    return lsf_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SymbolTable::EnsureGroupPropertiesValid() const {
    if (m_group_properties_valid) {
        return;
    }

    ++m_symbols_changed_counter;

    SymbolGroupState *states[MAX_NUM_SYMBOL_FILE_GROUPS] = {};

    for (unsigned group_index = 0; group_index < MAX_NUM_SYMBOL_FILE_GROUPS; ++group_index) {
        SymbolGroup *group = &m_groups[group_index];

        group->index = (uint8_t)group_index;
        ASSERT(group->index == group_index);

        group->used = false;
    }

    for (const std::unique_ptr<LoadedSymbolFile> &lsf : m_lsfs) {
        SymbolGroup *group = &m_groups[lsf->file.group_index];

        group->used = true;

        SymbolGroupState **state_ptr = &states[lsf->file.group_index];

        if (!*state_ptr) {
            *state_ptr = &group->state;

            if (lsf->file.enabled) {
                **state_ptr = SymbolGroupState_Enabled;
            } else {
                **state_ptr = SymbolGroupState_Disabled;
            }
        } else {
            switch (**state_ptr) {
            case SymbolGroupState_Disabled:
                if (lsf->file.enabled) {
                    **state_ptr = SymbolGroupState_Indeterminate;
                }
                break;

            case SymbolGroupState_Enabled:
                if (!lsf->file.enabled) {
                    **state_ptr = SymbolGroupState_Indeterminate;
                }
                break;

            default:
                ASSERT(false);
                [[fallthrough]];
            case SymbolGroupState_Indeterminate:
                // No way out of this state.
                break;
            }
        }
    }

    for (unsigned group_index = 0; group_index < MAX_NUM_SYMBOL_FILE_GROUPS; ++group_index) {
        if (!states[group_index]) {
            m_groups[group_index].state = SymbolGroupState_Disabled;
        }
    }

    m_group_properties_valid = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
