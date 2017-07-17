#ifndef HEADER_B357E0ABA50647C99600BD9BFB481E8A// -*- mode:c++ -*-
#define HEADER_B357E0ABA50647C99600BD9BFB481E8A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>
#include "BeebKeymap.h"

class BeebWindow;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class KeymapsUI {
public:
    static const char NOT_EDITABLE_ICON[];
    static const char SCANCODES_KEYMAP_ICON[];
    static const char KEYSYMS_KEYMAP_ICON[];

    static std::unique_ptr<KeymapsUI> Create();

    KeymapsUI();
    virtual ~KeymapsUI()=0;

    KeymapsUI(const KeymapsUI &)=delete;
    KeymapsUI &operator=(const KeymapsUI &)=delete;

    virtual void SetWindowDetails(BeebWindow *beeb_window)=0;

    virtual void SetCurrentBeebKeymap(const BeebKeymap *beeb_keymap)=0;
    virtual const BeebKeymap *GetCurrentBeebKeymap() const=0;
    virtual void DoImGui()=0;
    virtual bool WantsKeyboardFocus() const=0;
    virtual bool DidConfigChange() const=0;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
