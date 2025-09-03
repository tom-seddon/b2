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

struct BBCMicroType;
struct LogSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct Symbol {
    //
    uint16_t address = 0;

    //
    std::string name;

    //
    size_t line_number = 0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SymbolFile {
    std::string name;             //TODO: should really be called "group name"
    std::string file_path;        // source file path
    std::string file_format_name; // or "" for auto-detect
    bool enabled = true;
    uint8_t tag = 0;

    // Address suffixes for which symbols in this file apply. Serialised as-is.
    std::vector<std::string> address_suffixes;
};
JSON_SERIALIZE(SymbolFile, name, file_path, enabled, address_suffixes, file_format_name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SymbolTable {
  public:
    SymbolTable();
    ~SymbolTable();

    class SymbolParser;

    // Core functionality
    void Clear();
    bool LoadFromFile(const std::string &filepath, const SymbolParser *parser, const LogSet *logs);
    bool LoadFromString(const std::string &content, const std::string &filepath, const SymbolParser *parser);
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
    bool SetFileGroupName(size_t file_index, const std::string &new_name);
    void SetFileAddressSuffixes(size_t file_index, std::vector<std::string> new_address_suffixes);

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
        virtual std::vector<Symbol> ParseContent(const std::string &content) const = 0;
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

    // Format detection and loading
    const SymbolParser *DetectBestParser(const std::string &content);
    bool LoadFromContent(const std::string &content, size_t file_index);

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

    std::vector<std::unique_ptr<LoadedSymbolFile>> m_files;

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

    // Helper methods
    bool IsValidAddress(uint32_t addr) const;
    void InvalidateCache() const;
    void EnsureCacheReady(const std::shared_ptr<const BBCMicroType> &type) const;
    LoadedSymbolFile *AddLoadedSymbolFile(SymbolFile new_file, size_t *file_index);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
