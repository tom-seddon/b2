#ifndef HEADER_B805CD134B3642B4926B919E556B2ED5
#define HEADER_B805CD134B3642B4926B919E556B2ED5

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>
#include <functional>
#include <memory>

class MessageList;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void FailureMessageBox(const std::string &title, const std::shared_ptr<MessageList> &message_list, size_t num_messages = 10);

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

void ForEachRecentPaths(std::function<void(const std::string &, const RecentPaths &)> fun);
RecentPaths *GetRecentPathsByTag(const std::string &tag);
void SetRecentPathsByTag(std::string tag, RecentPaths recents);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SelectorDialog {
  public:
    explicit SelectorDialog(std::string tag);
    virtual ~SelectorDialog() = 0;

    // return value is valid only until next LoadRecentPathsSettings.
    RecentPaths *GetRecentPaths() const;
    //void SetRecentPathsTag(std::string tag);
    void AddLastPathToRecentPaths();

    bool Open(std::string *path);

  protected:
    virtual std::string HandleOpen() = 0;

    std::string m_last_path;

  private:
    std::string m_recent_paths_tag;
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

    explicit FileDialog(std::string tag);

    void AddFilter(std::string title, const std::vector<std::string> extensions);
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
    explicit OpenFileDialog(std::string tag);

  protected:
    std::string HandleOpen() override;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SaveFileDialog : public FileDialog {
  public:
    explicit SaveFileDialog(std::string tag);

  protected:
    std::string HandleOpen() override;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class FolderDialog : public SelectorDialog {
  public:
    explicit FolderDialog(std::string tag);

  protected:
    std::string HandleOpen() override;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
