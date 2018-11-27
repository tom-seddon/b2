#ifndef HEADER_933F80A7CAAF44EEBA8D2EC51C7A2A6E// -*- mode:c++ -*-
#define HEADER_933F80A7CAAF44EEBA8D2EC51C7A2A6E

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandContextStack;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SettingsUI {
public:
    SettingsUI();
    virtual ~SettingsUI()=0;

    SettingsUI(const SettingsUI &)=delete;
    SettingsUI &operator=(const SettingsUI &)=delete;
    SettingsUI(SettingsUI &&)=delete;
    SettingsUI &operator=(SettingsUI &&)=delete;

    // default impl returns 0.
    virtual uint32_t GetExtraImGuiWindowFlags() const;

    virtual void DoImGui(CommandContextStack *cc_stack)=0;

    // Return true to have the config saved when this UI window is closed.
    virtual bool OnClose()=0;

    // default impl returns false.
    virtual bool WantsKeyboardFocus() const;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
