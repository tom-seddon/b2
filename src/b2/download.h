#ifndef HEADER_5AF55ADF49FE4BCAB904ABE8B6B49980
#define HEADER_5AF55ADF49FE4BCAB904ABE8B6B49980

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Download the given file.
//
// If result is 0, the request succeeded, and *result is the name of
// the local copy of the downloaded file.
//
// If the result is -1, the request failed, and *result is a suitable
// error message.
//
// Either way, *result points to a buffer allocated with malloc. Use
// free to free it.
int DownloadFile(char **result,const char *url,int force_download);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
