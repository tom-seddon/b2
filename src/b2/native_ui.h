#ifndef HEADER_B805CD134B3642B4926B919E556B2ED5
#define HEADER_B805CD134B3642B4926B919E556B2ED5

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <shared/guid.h>
#include <nlohmann/json_fwd.hpp>

class MessageList;
class Messages;
struct SDL_Surface;
struct SDL_Window;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// To simplify use with Dear ImGui, the dialog objects are deliberately cheap to
// create, ignore, and then destroy. Nothing interesting happens unless any of
// the other functions are called.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The failure message box only appears when the main window creation fails. It
// therefore never has a parent.
void FailureMessageBox(const std::string &title, const std::shared_ptr<MessageList> &message_list, size_t num_messages = 10);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetClipboardImage(SDL_Surface *surfaces, Messages *messages);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_OSX

// There's probably somewhere slightly better these could go. But it would
// only be slightly better.

// Returns the result of [NSApp keyWindow].
void *GetKeyWindow();

// Returns [NSEvent doubleClickInterval].
double GetDoubleClickIntervalSeconds();

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Create these as globals. They are automatically added to a list for
// serialization purposes and the name is checked for uniqueness.
class RecentPaths {
  public:
    const std::string name;

    explicit RecentPaths(std::string name);
    ~RecentPaths();

    void Clear();

    void AddPath(std::string path);

    size_t GetNumPaths() const;

    // Most recently used file, if any, is at index 0.
    const std::string &GetPathByIndex(size_t index) const;

    void RemovePathByIndex(size_t index);

  protected:
  private:
    size_t m_max_num_paths;
    std::vector<std::string> m_paths;
};

const std::vector<RecentPaths *> *GetAllRecentPaths();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool LoadSelectorDialogPersistentData(const nlohmann::json &j, std::string *error);
nlohmann::json SaveSelectorDialogPersistentData();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Attempts to close the current modal dialog (if any). Safe to use from a
// background thread.
//
// Returns true if there's no modal dialog any more - inlcuding the case there
// wasn't one originally; returns false if there was some problem closing the
// dialog.
bool CloseModalDialog();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SelectorDialog {
  public:
    explicit SelectorDialog(const Guid &guid);
    virtual ~SelectorDialog() = 0;

    // return value is valid only until next LoadRecentPathsSettings.
    //RecentPaths *GetRecentPaths() const;
    //void SetRecentPathsTag(std::string tag);
    void AddLastPathToRecentPaths(RecentPaths *paths);

    bool Open(SDL_Window *parent, std::string *path);

  protected:
    virtual std::string HandleOpen(SDL_Window *parent) = 0;

    std::string m_last_path;

    const Guid m_guid;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class FileDialog : public SelectorDialog {
  public:
    struct Filter {
        std::string title;

        // Each extension must start with a '.', for symmetry with
        // PathGetExtension and so on.
        std::vector<std::string> extensions;
    };

    explicit FileDialog(const Guid &guid);

    void AddFilter(std::string title, std::vector<std::string> extensions);
    void AddAllFilesFilter();

  protected:
    std::string m_default_dir;
    std::string m_default_name;
    std::vector<Filter> m_filters;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class OpenFileDialog : public FileDialog {
  public:
    explicit OpenFileDialog(const Guid &guid);

  protected:
    std::string HandleOpen(SDL_Window *parent) override;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SaveFileDialog : public FileDialog {
  public:
    explicit SaveFileDialog(const Guid &guid);

    // Set the suggested name. The dialog will open at the last path used, with
    // the name part of the path suggested.
    void SetSuggestedName(const std::string &path);

    // Set the Save As... path. The dialog will open at that path with that name
    // suggested.
    //
    // TODO: didn't end up using this.
    //void SetSaveAsPath(std::string save_as_path);

  protected:
    std::string HandleOpen(SDL_Window *parent) override;

  private:
    std::string m_suggested_name;
    //std::string m_save_as_path;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
