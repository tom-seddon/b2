#ifndef HEADER_AD4879E36B0240B596B67BDB636FED92 // -*- mode:c++ -*-
#define HEADER_AD4879E36B0240B596B67BDB636FED92

extern __declspec(thread) HRESULT g_last_hr;

#define TEST_HR(X) TEST_TRUE(SUCCEEDED(g_last_hr = (X)))

void PrintLastHRESULT(const TestFailArgs *tfa);

#endif
