// b2 imconfig.h
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

// b2 already has a copy, built with STBI_WINDOWS_UTF8 definend.
#define IMGUI_DISABLE_STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8

#ifdef IMGUI_ENABLE_TEST_ENGINE
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1
#define IMGUI_TEST_ENGINE_ENABLE_IMPLOT 0

// Avoid a warning, as imgui_te_imconfig.h always defines it.
#undef IMGUI_ENABLE_TEST_ENGINE

#include <imgui_test_engine/imgui_te_imconfig.h>

#ifndef IMGUI_ENABLE_TEST_ENGINE
// The above assumption needs revisiting.
#error
#endif

#endif

#include <imconfig.h>
