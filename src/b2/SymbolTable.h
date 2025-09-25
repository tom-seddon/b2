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
#include <shared/json.h>

// stupid windows.h crap.
#ifdef MoveFile
#undef MoveFile
#endif

#include <shared/enum_decl.h>
#include "SymbolTable.inl"
#include <shared/enum_end.h>

struct BBCMicroType;
struct LogSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Symbol {
    // TODO: may want to expand this to something else at some point, since
    // symbols could be anything. But, for now, they are assumed to represent
    // addreses, so 16 bits makes sense.
    uint16_t address = 0;

    //
    std::string name;

    //
    size_t line_number = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// For 6502 interop purposes, the group index basically has to be a 1-byte
// value, so the limit is inherently 256.
//
// This is unlikely to change.
//
// The recommended data type is unsigned, for ok interop with ImGui::PushID.
static constexpr unsigned MAX_NUM_SYMBOL_FILE_GROUPS = 256;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SymbolGroup {
    // Name of this group. For human readability purposes only.
    std::string name;

    // Updated automatically.
    uint8_t index = 0;

    // Updated automatically as file groups are changed.
    bool used = false;

    // Updated automatically as files are enabled or disabled.
    SymbolGroupState state = SymbolGroupState_Indeterminate;
};
JSON_SERIALIZE(SymbolGroup, name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SymbolFile {
    // Source file path.
    std::string file_path; // source file path

    // Name of parser in the registry (going by GetFormatName), or "" for
    // auto-detect.
    //
    // (If name not found, will auto-detect in that case too - but this
    // behaviour is up for debate.)
    std::string file_format_name;

    // Enabled flag.
    bool enabled = true;

    // Group index, manually assigned by the user. Files in the same group can
    // be enabled/disabled as one.
    //
    // (Eventually this will probably be used to allow similar bulk
    // enable/disable of symbol groups from 6502 code.)
    uint8_t group_index = 0;

    // Address suffixes for which symbols in this file apply. Serialised as-is.
    std::vector<std::string> address_suffixes;
};
JSON_SERIALIZE(SymbolFile, file_path, enabled, address_suffixes, file_format_name, group_index);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SymbolTable {
  public:
    SymbolTable();
    ~SymbolTable();

    SymbolTable(const SymbolTable &src);

    // The symbol table is not otherwise genrerally copyable.
    SymbolTable &operator=(const SymbolTable &src) = delete;
    SymbolTable(SymbolTable &&) = delete;
    SymbolTable &operator=(SymbolTable &&) = delete;

    class SymbolParser;

    // Core functionality
    void Clear();
    bool LoadFromFile(const std::string &filepath, const SymbolParser *parser, const LogSet *logs);
    bool LoadFromString(const std::string &content, const std::string &filepath, const SymbolParser *parser, const LogSet *logs);
    size_t GetSymbolCount() const;
    size_t GetEnabledSymbolCount() const;
    size_t GetSymbolCountForFile(size_t file_index) const;

    // File management
    bool RemoveFile(size_t file_index);
    void EnableFile(size_t file_index, bool enabled);
    size_t GetNumFiles() const;
    const SymbolFile *GetFileByIndex(size_t file_index) const;
    bool MoveFile(size_t from_index, size_t to_index);

    // Group metadata editing
    void SetFileGroupIndex(size_t file_index, uint8_t group_index);
    void SetFileAddressSuffixes(size_t file_index, std::vector<std::string> new_address_suffixes);
    const SymbolGroup *GetSymbolGroupByIndex(uint8_t group_index) const;
    void SetGroupEnabled(uint8_t group_index, bool enabled);

    // The group name has no impact on anything, so it can be freely changed.
    std::string *GetGroupMutableName(uint8_t group_index);

    // Context-aware symbol lookup (symbols without explicit contexts are universal)

    // returned pointer remains valid only until next SymbolTable function call.
    const std::string *GetSymbolNameForAddress(uint16_t address, uint32_t dso, const std::shared_ptr<const BBCMicroType> &type) const;

    // Legacy lookup methods (for backwards compatibility)
    // GetSymbolForAddress uses symbol precedence policy for DISPLAY:
    //   1. Prefer symbols from enabled filess
    //   2. Among enabled files, prefer first loaded file (lower file_index)
    //   3. Within same file, prefer later loaded symbols (e.g., CC65 "_main" over "__MY_RAM_START__")
    // GetAddressForSymbol finds ANY symbol with the given name (ignores display precedence)
    bool GetAddressForSymbol(uint16_t *addr_ptr, uint32_t *dso_ptr, const std::shared_ptr<const BBCMicroType> &type, const std::string &name) const;

    // Base class for symbol file parsers
    class SymbolParser {
      public:
        virtual ~SymbolParser() = default;
        virtual std::string GetFormatName() const = 0;
        virtual std::vector<std::string> GetSuggestedFileExtensions() const = 0;
        virtual bool MatchesLine(const std::string &line) const = 0;
        virtual bool ParseSymbolsFromContent(std::vector<Symbol> *symbols, const std::string &content, const std::string &file_path, const LogSet *logs) const = 0;
    };

    // Parser registry system
    class SymbolParserRegistry {
      public:
        static void RegisterParser(std::unique_ptr<const SymbolParser> parser);
        static const std::vector<std::unique_ptr<const SymbolParser>> &GetParsers();
        static void InitializeBuiltinParsers();                                            // Initialize VICE and ACME parsers
        static const SymbolParser *FindParserByFormatName(const std::string &format_name); //returns nullptr if not found

      private:
        static std::vector<std::unique_ptr<const SymbolParser>> s_parsers;
    };

    // Persistence support
    std::shared_ptr<JSON> SaveToJSON() const;
    bool LoadFromJSON(const std::shared_ptr<JSON> &j, const LogSet *logs);
    void ReloadAllFiles(const LogSet *logs); // Reload all files from their source files

    // Debugging/utility
    //void PrintStats() const;

  private:
    struct LoadedSymbolFile {
        SymbolFile file;

        std::vector<Symbol> symbols;

        // Processed address suffixes, dependent on the current BBCMicroType.
        struct DSOMask {
            uint32_t mask = 0;
            uint32_t value = 0;
        };
        std::vector<DSOMask> address_suffix_dso_masks;
    };

    std::vector<std::unique_ptr<LoadedSymbolFile>> m_lsfs;
    mutable SymbolGroup m_groups[MAX_NUM_SYMBOL_FILE_GROUPS];

    struct SymbolsInFile {
        const LoadedSymbolFile *lsf = nullptr;
        std::vector<const Symbol *> symbols;
    };

    struct SymbolsAtAddress {
        std::vector<SymbolsInFile> per_file;
        //std::vector<const Symbol *> all;//TODO
    };

    struct AddressForSymbol {
        const LoadedSymbolFile *lsf = nullptr;
        uint16_t address = 0;
    };

    mutable std::map<uint16_t, SymbolsAtAddress> m_cache_address_to_symbols;                // Multiple symbols per address
    mutable std::map<std::string, std::vector<AddressForSymbol>> m_cache_name_to_addresses; // Multiple addresses per name

    mutable std::shared_ptr<const BBCMicroType> m_cache_type;
    mutable bool m_group_properties_valid = false;

    // Format detection and loading
    const SymbolParser *DetectBestParser(const std::string &content);
    bool LoadFromContent(const std::string &content, size_t file_index, const LogSet *logs);

    // Helper methods
    bool IsValidAddress(uint32_t addr) const;
    void InvalidateEverything() const;
    void InvalidateGroupProperties() const;
    void EnsureCacheReady(const std::shared_ptr<const BBCMicroType> &type) const;
    LoadedSymbolFile *AddLoadedSymbolFile(SymbolFile new_file, size_t *file_index);
    void EnsureGroupPropertiesValid() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
