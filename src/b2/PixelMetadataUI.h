#ifndef HEADER_E19FA05C0CEE4BE0ABE2108EDC9F3E7A// -*- mode:c++ -*-
#define HEADER_E19FA05C0CEE4BE0ABE2108EDC9F3E7A

#include "conf.h"

#if VIDEO_TRACK_METADATA

#include "SettingsUI.h"

class BeebWindow;
class CommandContextStack;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class PixelMetadataUI:
    public SettingsUI
{
public:
    explicit PixelMetadataUI(BeebWindow *beeb_window);

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif

#endif
