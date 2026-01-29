// b2 imconfig.h
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 0

// b2 already has a copy, built with STBI_WINDOWS_UTF8 definend.
#define IMGUI_DISABLE_STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8

#include <imgui_test_engine/imgui_te_imconfig.h>
#include <imconfig.h>
