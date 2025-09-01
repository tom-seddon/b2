# Symbol System Architecture

## Overview

This document describes the enhanced symbol system implemented for the b2 BBC Micro emulator debugger. The system supports multiple symbols per address, context-aware symbol lookup, and efficient caching for real-time disassembly display.

## Core Problem Solved

The BBC Micro has complex memory mapping with different ROM banks, shadow RAM, and main RAM. A single address (e.g., `0x8000`) might contain different symbols depending on the memory context:
- `_basic_start` in ROM bank F  
- `_user_code` in main RAM
- `_os_routine` in OS ROM

Additionally, tools like CC65 generate multiple symbols at the same address (e.g., `__SEGMENT_START__` followed by `_main`), where the later symbol is typically more meaningful for debugging.

---

## 1. Multiple Symbols System

### Data Structures

The system uses three primary data structures to support multiple symbols per address:

#### Primary Storage
Maps each memory address to a vector containing ALL symbols at that address. This allows multiple symbols from different sources to coexist at the same location.

#### Name Lookup
A multimap that allows looking up addresses by symbol name. Supports multiple addresses per name for cases where the same symbol name appears in different contexts.

#### Context Cache
A two-level cache system where the first level is memory context (like 'm' for main RAM, 'f' for ROM bank F) and the second level maps addresses to symbols visible in that context. This enables fast context-aware lookups during real-time disassembly.

### Symbol Precedence Policy

When multiple symbols exist at the same address, the system applies a three-tier precedence policy:

1. **Enabled groups first**: Disabled groups are ignored
2. **Lower group_id wins**: First-loaded group takes precedence  
3. **Later symbols within group win**: CC65 style - `_main` overrides `__SEGMENT_START__`

### Examples

**CC65 Style (Same Group)**:
At address 0x8023 in Group 0, if `__SEGMENT_START__` is loaded first and `_basic_warm_start` is loaded later, `_basic_warm_start` wins and will be displayed.

**Multiple Groups**:
At address 0x3000, if `_main` is in Group 0 and `_alt_main` is in Group 1 (both enabled), `_main` wins because it has the lower group ID.

---

## 2. Group System

### Group Concept

The group system has two distinct levels:

**Group Names** (UI Organization):
Display labels like "MOS Symbols" or "User Code" used for organizing entries in the Debug menu. Multiple file entries can share the same group name for bulk operations.

**Individual Entries** (Technical Groups):  
Each loaded symbol file creates a separate entry with its own unique properties: file path, enabled state, memory contexts, and symbol data. Each entry gets its own group ID regardless of whether it shares a display name with other entries.

### Entry Assignment Logic

When loading symbols:

1. **Always create a new entry**: Each loaded file gets a distinct entry in the symbol table
2. **Group ID assignment**: Each entry gets the next available group ID (index in the entries vector)  
3. **Group name assignment**: 
   - Basic loading: If no advanced options are used, the group name defaults to "Global"
   - Advanced options: Custom name provided by user, can be the same as existing group names.
4. **Memory contexts**: Applied individually to each entry, not to the group name

### Group Behavior Examples

**Different Group Names**:
Each file with a different group name appears as a separate line in both the Debug menu and Group Management window.

**Same Group Names** (UI grouping):
Multiple files with the same group name appear as separate entries in Group Management but are consolidated in the Debug menu for bulk enable/disable operations.

---

## 3. Configuration Persistence  

### Selective Saving Strategy

The system saves only essential user-configurable fields to the configuration file, while auto-generating derived fields on load.

#### Saved Fields (Per Entry)
- File path to the symbol file
- Enabled/disabled state  
- Memory contexts for this specific entry
- Custom group name (if provided via advanced options)

#### Auto-Generated Fields (Derived)
- **Group name**: Generated from filename for basic loads, preserved when advanced options specify custom names
- **Description**: Auto-generated description based on file path

#### Benefits
- **Smaller config files**: Significant size reduction by not storing derived data
- **Self-healing**: Names and descriptions regenerate if files move or change
- **Consistency**: Uniform behavior regardless of how symbols were loaded

#### Basic vs Advanced Options Detection
Basic loads (those without memory contexts specified) get generic names, while loads using advanced options preserve custom group names provided by the user.

---

## 4. Memory Context System

### Supported Contexts (38 Total)

The BBC Micro emulator now supports 38 different memory contexts:

#### Core Contexts
- **'m'**: Main RAM
- **'o'**: OS ROM (MOS)
- **'s'**: Shadow RAM  
- **'n'**: ANDY (extra RAM)
- **'h'**: HAZEL (Master extra RAM)
- **'i'**: I/O area

#### Standard ROM Banks
- **'0'-'f'**: ROM banks 0-15 (16 contexts)

#### Parasite (Second Processor)
- **'p'**: Parasite RAM
- **'r'**: Parasite boot ROM

#### ROM Mappers
- **'A'-'P'**: ROM mapper regions 0-15 (16 additional contexts)

### Universal vs Context-Specific Symbols

**Universal Symbols** (loaded without explicit context):
Visible in ALL memory contexts. These symbols appear regardless of which ROM bank or RAM configuration is active.

**Context-Specific Symbols**:
Only visible in their designated memory contexts. For example, a symbol marked for context 'f' only appears when ROM bank F is active.

### Context vs Precedence

Memory contexts affect **ELIGIBILITY**, not **PRECEDENCE RANKING**:

1. **Context Filtering**: Determines which symbols are visible in the requested context
2. **Precedence Ranking**: Among visible symbols, determines which is best (based on group_id and load order)

Universal symbols have equal precedence to context-specific symbols - they're not penalized for being universal.

---

## 5. Cache System

### Cache Purpose

The cache system pre-computes which symbols are visible in each memory context, enabling immediate context-aware lookups during real-time disassembly without having to filter symbols on every request.

### Cache Structure

The cache is organized as a two-level map where the first level is memory context character and the second level maps addresses to vectors of symbol pointers visible in that context.

### Cache Rebuilding

The cache uses **lazy rebuilding**:
1. Mark cache dirty when symbols change
2. Rebuild only when needed for lookup
3. Store pointers to symbols to avoid data duplication

**Universal Symbol Caching**:
Universal symbols (those without explicit contexts) are added to ALL 38 memory contexts during cache rebuild.

**Context-Specific Symbol Caching**:
Context-specific symbols are only added to their designated contexts during cache rebuild.

---

## 6. User Interface Integration

### Symbol Group Management Window

A comprehensive management interface that allows users to:
- View all loaded symbol groups in precedence order
- Enable/disable groups individually
- Reorder groups to change precedence
- Edit group names inline
- Edit memory contexts via modal popup
- Delete groups and their symbols
- View symbol counts per group

The window persists its open state between application sessions and integrates with the main popup system.

### Unified Symbol Loading Dialog

A single loading dialog with expandable advanced options that supports:
- Basic loading (universal context, auto-generated group names)
- Advanced options (custom contexts, custom group names)
- Memory context selection with organized UI (common contexts first, advanced contexts in collapsible sections)

### Context Selection UI

A shared helper system for selecting memory contexts that organizes the 38 contexts into logical groups:
- **Most common**: Main RAM ('m') and OS ROM ('o') 
- **Standard ROM banks**: '0'-'f' in two rows
- **Shadow/Extra RAM**: Collapsible section for 's', 'n', 'h'
- **Parasite**: Collapsible section for 'p', 'r'  
- **ROM Mappers**: Collapsible section for 'A'-'P'
- **Other**: Collapsible section for 'i'

### Debug Menu Integration

The system provides a two-tier approach:

**Debug Menu**: Groups entries by display name (e.g., "MOS Symbols") for bulk enable/disable operations. If some entries sharing a name are enabled and others disabled, a tristate checkbox appears.

**Group Management Window**: Shows every individual entry separately, each with its own row, contexts, and controls. Multiple entries can have the same display name but remain distinct for granular management.

---

## 7. Lookup Flow Sequence

The symbol lookup process follows this sequence:

1. Disassembly window requests symbol at specific address in specific context
2. System checks if cache needs rebuilding
3. If cache is dirty, rebuild by filtering all symbols into appropriate contexts
4. Look up pre-filtered symbols from cache for the requested context and address
5. Apply precedence policy to determine best symbol among visible candidates
6. Return winning symbol for display
7. Disassembly shows symbolic name instead of raw address

---

## 8. Key Behavioral Differences

### Display Lookups vs Name Lookups

**Display Lookup**:
Uses precedence policy to find the "best" symbol for display in disassembly windows. Shows the most relevant symbol to the user based on group precedence.

**Name Lookup**:  
Finds any enabled symbol with the requested name, regardless of display precedence. This ensures all loaded symbols remain accessible via the address bar, even when precedence rules favor displaying different symbols.

### Context Display Improvements

When showing multiple contexts in tooltips or debug information, contexts are now displayed in groups of 6 per line with proper formatting and total counts, making large context lists much more readable.

---

## 9. Performance Characteristics

- **Symbol Loading**: Fast (no immediate cache rebuild required)
- **First Lookup After Load**: Slower (triggers cache rebuild)  
- **Subsequent Lookups**: Very fast (cached context filtering + precedence)
- **Memory Usage**: Efficient (pointer-based cache, no symbol duplication)
- **Scalability**: Handles thousands of symbols across dozens of groups without performance degradation

---

## 10. Integration Points

The symbol system integrates seamlessly with:
- **Disassembly Window**: Real-time instruction symbol display with context awareness
- **Address Bar**: Symbol name to address navigation
- **Symbol Loading UI**: Both basic and advanced loading workflows
- **Group Management**: Enable/disable, reorder, and reload functionality
- **Configuration Persistence**: Automatic save/restore of symbol groups and preferences
- **Debug Menu**: Bulk operations on groups with same display name

---

## Conclusion

This symbol system architecture provides:
- ✅ **Multiple symbols per address** from different sources with clear precedence
- ✅ **Context-aware display** supporting all 38 BBC Micro memory contexts
- ✅ **Efficient caching** with lazy rebuilds for optimal performance
- ✅ **Comprehensive management UI** for organizing and controlling symbol groups  
- ✅ **Universal and context-specific** symbol support with equal precedence treatment
- ✅ **Scalable design** suitable for complex debugging scenarios with many symbol files
- ✅ **Persistent configuration** that survives application restarts
- ✅ **Clean, maintainable codebase** with shared UI components and helper functions

The system successfully handles the complexity of BBC Micro memory mapping while providing intuitive symbol management and high-performance real-time disassembly display. Users can load symbols from multiple sources, organize them into logical groups, and have confidence that the most relevant symbols will be displayed in each memory context.