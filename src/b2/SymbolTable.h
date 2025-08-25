#ifndef HEADER_5F4E8A8B_34F4_4B1E_9B2D_8E2A3D1F6C4A
#define HEADER_5F4E8A8B_34F4_4B1E_9B2D_8E2A3D1F6C4A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <unordered_map>
#include "nlohmann_json_wrapper.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SymbolTable {
  public:
    struct Symbol {
        uint16_t address;
        std::string name;
        std::string comment; // optional - for future use
        size_t group_id;     // which group this symbol belongs to

        Symbol()
            : address(0)
            , group_id(0) {
        }
        Symbol(uint16_t addr, const std::string &symbol_name, size_t group = 0)
            : address(addr)
            , name(symbol_name)
            , group_id(group) {
        }
    };

    struct SymbolGroup {
        std::string name;
        std::string description;
        std::string file_path; // source file for this group
        bool enabled;
        std::set<char> memory_contexts; // which memory contexts this group applies to (e.g. 'o', 'f', 'm')

        // Address suffixes for which this symbol group applies. Serialised
        // as-is.
        std::vector<std::string> address_suffixes;

        // Processed address suffixes, dependent on the current BBCMicroType.
        struct DSOMask {
            uint32_t mask = 0;
            uint32_t value = 0;
        };
        std::vector<DSOMask> address_suffix_dso_masks;

        SymbolGroup()
            : enabled(true) {
        }

        SymbolGroup(const std::string &group_name, const std::string &desc = "", const std::string &path = "")
            : name(group_name)
            , description(desc)
            , file_path(path)
            , enabled(true) {
        }

        static const std::string ADDRESS_SUFFIXES;

        // Custom JSON serialization (save only essential fields)
        nlohmann::json to_json() const {
            nlohmann::json j{
                {"file_path", file_path},
                {"enabled", enabled},
                {"name", name}, // Save custom group name to preserve user choice
                {ADDRESS_SUFFIXES, this->address_suffixes},
            };

            // Custom handling for std::set<char>
            nlohmann::json contexts_array = nlohmann::json::array();
            for (char c : memory_contexts) {
                contexts_array.push_back(std::string(1, c));
            }
            j["memory_contexts"] = contexts_array;

            return j;
        }

        void from_json(const nlohmann::json &j) {
            j.at("file_path").get_to(file_path);
            j.at("enabled").get_to(enabled);

            // Custom handling for std::set<char>
            memory_contexts.clear();
            if (j.contains("memory_contexts") && j["memory_contexts"].is_array()) {
                for (const auto &item : j["memory_contexts"]) {
                    if (item.is_string()) {
                        std::string str = item.get<std::string>();
                        if (!str.empty()) {
                            memory_contexts.insert(str[0]);
                        }
                    }
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
                    if (memory_contexts.empty()) {
                        name = "Global";
                    } else {
                        name = filename;
                    }
                }
            }

            // Always generate description from file path
            if (file_path.empty()) {
                description = "Unknown symbol group";
            } else {
                description = "Loaded from " + file_path;
            }

            if (j.contains(ADDRESS_SUFFIXES)) {
                try {
                    this->address_suffixes = j[ADDRESS_SUFFIXES].get<std::vector<std::string>>();
                } catch (nlohmann::json::exception &) {
                }
            }
        }
    };

    SymbolTable();
    ~SymbolTable();

    // Core functionality
    void Clear();
    bool LoadFromFile(const std::string &filepath, const std::string &group_name = "", const std::set<char> &memory_contexts = {});
    size_t GetSymbolCount() const;
    size_t GetEnabledSymbolCount() const;
    size_t GetSymbolCountForGroup(size_t group_id) const;

    // Group management
    size_t AddGroup(const SymbolGroup &group);
    size_t AddGroup(const std::string &name, const std::string &description = "", const std::string &file_path = "");
    bool RemoveGroup(size_t group_id);
    void EnableGroup(size_t group_id, bool enabled);
    const SymbolGroup *GetGroup(size_t group_id) const;
    const std::vector<SymbolGroup> &GetAllGroups() const;
    void ClearGroup(size_t group_id);
    bool MoveGroup(size_t from_index, size_t to_index);
    void ReassignGroupIds();
    void ReassignGroupIds(const std::vector<std::string> &original_group_names);

    // Group metadata editing
    bool SetGroupName(size_t group_id, const std::string &new_name);
    bool SetGroupContexts(size_t group_id, const std::set<char> &new_contexts);

    // Context-aware symbol lookup (symbols without explicit contexts are universal)
    const Symbol *GetSymbolForAddress(uint16_t address, char memory_context) const;
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
    uint16_t GetAddressForSymbol(const std::string &name) const;
    //bool HasSymbolForAddress(uint16_t address) const;
    bool HasSymbol(const std::string &name) const;

    // Base class for symbol file parsers
    class SymbolParser {
      public:
        virtual ~SymbolParser() = default;
        virtual std::string GetFormatName() const = 0;
        virtual bool MatchesLine(const std::string &line) const = 0;
        virtual bool ParseContent(const std::string &content, size_t group_id, SymbolTable *table) = 0;
    };

    // Parser registry system
    class SymbolParserRegistry {
      public:
        static void RegisterParser(std::unique_ptr<SymbolParser> parser);
        static const std::vector<std::unique_ptr<SymbolParser>> &GetParsers();
        static void InitializeBuiltinParsers(); // Initialize VICE and ACME parsers

      private:
        static std::vector<std::unique_ptr<SymbolParser>> s_parsers;
    };

    // Format detection and loading
    SymbolParser *DetectBestParser(const std::string &content);
    bool LoadFromContent(const std::string &content, size_t group_id);

    // Legacy format-specific loaders (now used by parser implementations)
    bool LoadViceFormat(const std::string &content, size_t group_id = 0);
    bool LoadAcmeFormat(const std::string &content, size_t group_id = 0);

    // Persistence support
    nlohmann::json SaveToJSON() const;
    bool LoadFromJSON(const nlohmann::json &j);
    void ReloadAllGroups(); // Reload all groups from their source files

    // Debugging/utility
    void PrintStats() const;

  private:
    std::vector<SymbolGroup> m_groups;
    std::map<uint16_t, std::vector<Symbol>> m_address_to_symbols; // Multiple symbols per address
    std::multimap<std::string, uint16_t> m_name_to_addresses;     // Multiple addresses per name

    // Context-aware lookup maps for performance
    // Maps memory_context -> address -> vector of symbols
    mutable std::unordered_map<char, std::map<uint16_t, std::vector<Symbol *>>> m_context_to_address_cache;
    mutable bool m_cache_dirty;

    // Helper methods
    std::string TrimWhitespace(const std::string &str) const;
    bool IsValidAddress(uint32_t addr) const;
    void InvalidateCache() const;
    void RebuildCache() const;
    //bool IsSymbolVisibleInContext(const Symbol &symbol, char memory_context) const;
    //const Symbol *GetFirstEnabledSymbolAt(uint16_t address) const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Custom JSON serialization for SymbolGroup (save only essential fields)
// name and description are auto-generated on load

#endif
