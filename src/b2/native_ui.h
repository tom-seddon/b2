#ifndef HEADER_B805CD134B3642B4926B919E556B2ED5
#define HEADER_B805CD134B3642B4926B919E556B2ED5

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include <functional>
#include <memory>

class MessageList;
class Messages;
struct SDL_Surface;
struct SDL_Window;

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

class RecentPaths {
  public:
    RecentPaths();

    void AddPath(const char *path);

    size_t GetNumPaths() const;

    // Most recently used file, if any, is at index 0.
    const std::string &GetPathByIndex(size_t index) const;

    void RemovePathByIndex(size_t index);

  protected:
  private:
    size_t m_max_num_paths;
    std::vector<std::string> m_paths;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// these are automatically added to a list.
class SelectorDialogTag {
  public:
    const uint8_t guid[16];
    const std::string name;

    SelectorDialogTag(uint8_t guid0, uint8_t guid1, uint8_t guid2, uint8_t guid3, uint8_t guid4, uint8_t guid5, uint8_t guid6, uint8_t guid7, uint8_t guid8, uint8_t guid9, uint8_t guid10, uint8_t guid11, uint8_t guid12, uint8_t guid13, uint8_t guid14, uint8_t guid15, std::string name);
    ~SelectorDialogTag();

    SelectorDialogTag(const SelectorDialogTag &) = delete;
    SelectorDialogTag &operator=(const SelectorDialogTag &) = delete;
    SelectorDialogTag(SelectorDialogTag &&) = delete;
    SelectorDialogTag &operator=(SelectorDialogTag &&) = delete;
};

const std::vector<const SelectorDialogTag *> *GetAllSelectorDialogTags();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

RecentPaths *GetRecentPathsByTag(const SelectorDialogTag *tag);
void SetRecentPathsByTag(const SelectorDialogTag *tag, RecentPaths recents);

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
    explicit SelectorDialog(const SelectorDialogTag *tag);
    virtual ~SelectorDialog() = 0;

    // return value is valid only until next LoadRecentPathsSettings.
    RecentPaths *GetRecentPaths() const;
    //void SetRecentPathsTag(std::string tag);
    void AddLastPathToRecentPaths();

    bool Open(SDL_Window *parent, std::string *path);

  protected:
    virtual std::string HandleOpen(SDL_Window *parent) = 0;

    std::string m_last_path;

    const SelectorDialogTag *const m_tag;
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

    explicit FileDialog(const SelectorDialogTag *tag);

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
    explicit OpenFileDialog(const SelectorDialogTag *tag);

  protected:
    std::string HandleOpen(SDL_Window *parent) override;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SaveFileDialog : public FileDialog {
  public:
    explicit SaveFileDialog(const SelectorDialogTag *tag);

  protected:
    std::string HandleOpen(SDL_Window *parent) override;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This became redundant at one point. Probably
// https://github.com/tom-seddon/b2/commit/152a25a9bc4cf0303c4e43efb0962445e48c2dca
//
// The Windows and macOS implementations are still present, for now,
// but they'll probably need a pass if hoping to resurrect this.
//
// The Gtk code doesn't currently support it at all.

// class FolderDialog : public SelectorDialog {
//   public:
//     explicit FolderDialog(std::string tag);

//   protected:
//     std::string HandleOpen(SDL_Window *parent) override;

//   private:
// };

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
