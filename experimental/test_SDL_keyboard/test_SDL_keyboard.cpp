#include <shared/system.h>
#include <shared/testing.h>
#include <shared/log.h>
#include <SDL.h>
#include <vector>

LOG_DEFINE(OUTPUT,"",&log_printer_stdout_and_debugger)

int main(int argc,char *argv[]) {
    (void)argc,(void)argv;

    TEST_EQ_II(SDL_Init(SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER),0);

    SDL_Window *w=SDL_CreateWindow("keyboard test",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,250,250,SDL_WINDOW_INPUT_FOCUS);
    TEST_NON_NULL(w);

    //std::vector<SDL_Joystick *> joysticks;

    {
        for(int i=0;i<SDL_NumJoysticks();++i) {
            LOGF(OUTPUT,"%d: ",i);
            LOGI(OUTPUT);
            LOGF(OUTPUT,"Name: %s\n",SDL_JoystickNameForIndex(i));

            char guid_str[33];
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i),guid_str,sizeof guid_str);
            LOGF(OUTPUT,"GUID: %s\n",guid_str);

            SDL_Joystick *joystick=SDL_JoystickOpen(i);
            if(!joystick) {
                LOGF(OUTPUT,"(failed to open: %s)\n",SDL_GetError());
                continue;
            }

            LOGF(OUTPUT,"Features: %d axes, %d balls, %d buttons, %d hats\n",SDL_JoystickNumAxes(joystick),SDL_JoystickNumBalls(joystick),SDL_JoystickNumButtons(joystick),SDL_JoystickNumHats(joystick));
            LOGF(OUTPUT,"Instance ID: %d\n",SDL_JoystickInstanceID(joystick));

            SDL_JoystickClose(joystick);

            if(SDL_IsGameController(i)) {
                LOGF(OUTPUT,"Game Controller Name: %s\n",SDL_GameControllerNameForIndex(i));
            }
        }
    }

    LOGF(OUTPUT,"Joystick events state: %d\n",SDL_JoystickEventState(SDL_QUERY));

    SDL_StartTextInput();

    SDL_Event ev;
    while(SDL_WaitEvent(&ev)) {
        switch(ev.type) {
        case SDL_QUIT:
            goto done;

        case SDL_KEYUP:
        case SDL_KEYDOWN:
            {
                LOGF(OUTPUT,"%s: state=%s repeat=%u scancode=%s (%u; 0x%X) keycode=%s (%u; 0x%X) mod=0x%X\n",
                    ev.type==SDL_KEYUP?"SDL_KEYUP":"SDL_KEYDOWN",
                    ev.key.state==SDL_PRESSED?"SDL_PRESSED":"SDL_RELEASED",
                    ev.key.repeat,
                    SDL_GetScancodeName(ev.key.keysym.scancode),ev.key.keysym.scancode,ev.key.keysym.scancode,
                    SDL_GetKeyName(ev.key.keysym.sym),ev.key.keysym.sym,ev.key.keysym.sym,
                    ev.key.keysym.mod);
            }
            break;

        case SDL_TEXTINPUT:
            {
                LOGF(OUTPUT,"SDL_TEXTINPUT: \"%s\"\n",ev.text.text);
            }
            break;
        }
    }
done:

    return 0;
}
