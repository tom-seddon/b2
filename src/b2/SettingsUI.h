#ifndef HEADER_933F80A7CAAF44EEBA8D2EC51C7A2A6E // -*- mode:c++ -*-
#define HEADER_933F80A7CAAF44EEBA8D2EC51C7A2A6E

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandTable;
class CommandTable2;

#include <string>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO - bit of a misnomer now. Should be called PanelUI or something.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SettingsUI {
  public:
    SettingsUI();
    virtual ~SettingsUI() = 0;

    SettingsUI(const SettingsUI &) = delete;
    SettingsUI &operator=(const SettingsUI &) = delete;
    SettingsUI(SettingsUI &&) = delete;
    SettingsUI &operator=(SettingsUI &&) = delete;

    // Used by other panels to determine the name of this one.
    const std::string &GetName() const;
    void SetName(std::string name);

    // default impl returns 0.
    virtual uint32_t GetExtraImGuiWindowFlags() const;

    virtual void DoImGui() = 0;

    // Return true to have the config saved when this UI window is closed.
    virtual bool OnClose() = 0;

    // default impl returns NULL.
    virtual const CommandTable *GetCommandTable() const;
    virtual const CommandTable2*GetCommandTable2()const;

  protected:
  private:
    std::string m_name;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
