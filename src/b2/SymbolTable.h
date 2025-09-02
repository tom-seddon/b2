#ifndef HEADER_5F4E8A8B_34F4_4B1E_9B2D_8E2A3D1F6C4A
#define HEADER_5F4E8A8B_34F4_4B1E_9B2D_8E2A3D1F6C4A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#if BBCMICRO_DEBUGGER

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <shared/json.h>

struct BBCMicroType;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SymbolTable {
  public:
    struct Symbol {
        uint16_t address = 0;
        std::string name;
        std::string comment; // optional - for future use
        size_t group_id = 0; // which group this symbol belongs to

        Symbol() = default;
        Symbol(uint16_t addr, const std::string &symbol_name, size_t group);
    };

    struct SymbolGroup {
        std::string name;
        std::string file_path;        // source file for this group
        std::string file_format_name; // or "" for auto-detect
        bool enabled = true;

        // Address suffixes for which this symbol group applies. Serialised
        // as-is.
        std::vector<std::string> address_suffixes;

        // Processed address suffixes, dependent on the current BBCMicroType.
        struct DSOMask {
            uint32_t mask = 0;
            uint32_t value = 0;
        };
        std::vector<DSOMask> address_suffix_dso_masks;

        // if adding more stuff that needs serializing, be sure to update the
        // JSON_SERIALIZE macro below.

        SymbolGroup() = default;
        //SymbolGroup(std::string group_name, std::string path);
    };

    SymbolTable();
    ~SymbolTable();

    class SymbolParser;

    // Core functionality
    void Clear();
    bool LoadFromFile(const std::string &filepath, const SymbolParser *parser);
    size_t GetSymbolCount() const;
    size_t GetEnabledSymbolCount() const;
    size_t GetSymbolCountForGroup(size_t group_id) const;

    // Group management
    size_t AddGroup(const SymbolGroup &group);
    //size_t AddGroup(const std::string &name, const std::string &description = "", const std::string &file_path = "");
    bool RemoveGroup(size_t group_id);
    void EnableGroup(size_t group_id, bool enabled);
    const SymbolGroup *GetGroup(size_t group_id) const;
    const std::vector<SymbolGroup> &GetAllGroups() const;
    void ClearGroup(size_t group_id);
    bool MoveGroup(size_t from_index, size_t to_index);
    //void ReassignGroupIds();
    //void ReassignGroupIds(const std::vector<std::string> &original_group_names);

    // Group metadata editing
    bool SetGroupName(size_t group_id, const std::string &new_name);
    void SetGroupAddressSuffixes(size_t group_id, std::vector<std::string> new_address_suffixes);

    // Context-aware symbol lookup (symbols without explicit contexts are universal)
    const Symbol *GetSymbolForAddress(uint16_t address, uint32_t dso, const std::shared_ptr<const BBCMicroType> &type) const;
    //uint16_t GetAddressForSymbol(const std::string &name, char memory_context) const;
    //bool HasSymbolForAddress(uint16_t address, char memory_context) const;
    //bool HasSymbol(const std::string &name, char memory_context) const;

    // Legacy lookup methods (for backwards compatibility)
    // GetSymbolForAddress uses symbol precedence policy for DISPLAY:
    //   1. Prefer symbols from enabled groups
    //   2. Among enabled groups, prefer first loaded group (lower group_id)
    //   3. Within same group, prefer later loaded symbols (e.g., CC65 "_main" over "__MY_RAM_START__")
    // GetAddressForSymbol finds ANY symbol with the given name (ignores display precedence)
    //const Symbol *GetSymbolForAddress(uint16_t address) const;
    bool GetAddressForSymbol(uint16_t *addr_ptr, uint32_t *dso_ptr, const std::shared_ptr<const BBCMicroType> &type, const std::string &name) const;
    //uint16_t GetAddressForSymbol(const std::string &name) const;
    //bool HasSymbolForAddress(uint16_t address) const;
    //bool HasSymbol(const std::string &name) const;

    // Base class for symbol file parsers
    class SymbolParser {
      public:
        struct ParsedSymbol {
            size_t line_number = 0; //1-based; 0 means invalid
            std::string name;
            uint32_t addr = 0;
        };
        virtual ~SymbolParser() = default;
        virtual std::string GetFormatName() const = 0;
        virtual std::vector<std::string> GetSuggestedFileExtensions() const = 0;
        virtual bool MatchesLine(const std::string &line) const = 0;
        virtual std::vector<ParsedSymbol> ParseContent(const std::string &content) const = 0;
    };

    // Parser registry system
    class SymbolParserRegistry {
      public:
        static void RegisterParser(std::unique_ptr<const SymbolParser> parser);
        static const std::vector<std::unique_ptr<const SymbolParser>> &GetParsers();
        static void InitializeBuiltinParsers(); // Initialize VICE and ACME parsers
        static const SymbolParser *FindParserByFormatName(const std::string &format_name);//returns nullptr if not found

      private:
        static std::vector<std::unique_ptr<const SymbolParser>> s_parsers;
    };

    // Format detection and loading
    const SymbolParser *DetectBestParser(const std::string &content);
    bool LoadFromContent(const std::string &content, size_t group_id);

    // Persistence support
    std::shared_ptr<JSON> SaveToJSON() const;
    bool LoadFromJSON(const std::shared_ptr<JSON> &j);
    void ReloadAllGroups(); // Reload all groups from their source files

    // Debugging/utility
    void PrintStats() const;

  private:
    mutable std::vector<SymbolGroup> m_groups;
    std::map<uint16_t, std::vector<Symbol>> m_address_to_symbols; // Multiple symbols per address
    std::multimap<std::string, uint16_t> m_name_to_addresses;     // Multiple addresses per name

    mutable std::shared_ptr<const BBCMicroType> m_cache_type;

    // Helper methods
    //std::string TrimWhitespace(const std::string &str) const;
    bool IsValidAddress(uint32_t addr) const;
    void InvalidateCache() const;
    void EnsureCacheReady(const std::shared_ptr<const BBCMicroType> &type) const;
    //bool IsSymbolVisibleInContext(const Symbol &symbol, char memory_context) const;
    //const Symbol *GetFirstEnabledSymbolAt(uint16_t address) const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

JSON_SERIALIZE(SymbolTable::SymbolGroup, name, file_path, enabled, address_suffixes, file_format_name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Custom JSON serialization for SymbolGroup (save only essential fields)
// name and description are auto-generated on load

#endif

#endif
