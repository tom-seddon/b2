#ifndef HEADER_5F4E8A8B_34F4_4B1E_9B2D_8E2A3D1F6C4A
#define HEADER_5F4E8A8B_34F4_4B1E_9B2D_8E2A3D1F6C4A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <map>
#include <string>
#include <memory>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SymbolTable {
public:
    struct Symbol {
        uint16_t address;
        std::string name;
        std::string comment;  // optional - for future use
        
        Symbol() : address(0) {}
        Symbol(uint16_t addr, const std::string& symbol_name) 
            : address(addr), name(symbol_name) {}
    };

    SymbolTable();
    ~SymbolTable();
    
    // Core functionality
    void Clear();
    bool LoadFromFile(const std::string& filepath);
    size_t GetSymbolCount() const;
    
    // Symbol lookup
    const Symbol* GetSymbolForAddress(uint16_t address) const;
    uint16_t GetAddressForSymbol(const std::string& name) const;
    bool HasSymbolForAddress(uint16_t address) const;
    bool HasSymbol(const std::string& name) const;
    
    // Format-specific loaders
    bool LoadViceFormat(const std::string& content);
    
    // Debugging/utility
    void PrintStats() const;

private:
    std::map<uint16_t, Symbol> m_address_to_symbol;
    std::map<std::string, uint16_t> m_name_to_address;
    
    // Helper methods
    std::string TrimWhitespace(const std::string& str) const;
    bool IsValidAddress(uint32_t addr) const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
