// https://msdn.microsoft.com/en-us/library/windows/desktop/ff819477(v=vs.85).aspx

#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>
#include <shared/system.h>
#include <shared/testing.h>
#include "testing_windows.h"

//#pragma comment(lib, "mfreadwrite")
//#pragma comment(lib, "mfplat")
//#pragma comment(lib, "mfuuid")

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

HRESULT InitializeSinkWriter(
    IMFSinkWriter **ppWriter, 
    DWORD *pStreamIndex, 
    const WCHAR *url,
    DWORD width,
    DWORD height,
    DWORD fps)
{
    *ppWriter = NULL;
    *pStreamIndex = NULL;

    IMFSinkWriter   *pSinkWriter = NULL;
    IMFMediaType    *pMediaTypeOut = NULL;   
    IMFMediaType    *pMediaTypeIn = NULL;   
    DWORD           streamIndex;     

    HRESULT hr = MFCreateSinkWriterFromURL(url, NULL, NULL, &pSinkWriter);

    // Set the output media type.
    TEST_HR(MFCreateMediaType(&pMediaTypeOut));   
    TEST_HR(pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));     
    TEST_HR(pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_WMV3));   
    TEST_HR(pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 800000));   
    TEST_HR(pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
    TEST_HR(MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, width, height));   
    TEST_HR(MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, fps, 1));   
    TEST_HR(MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));   
    TEST_HR(pSinkWriter->AddStream(pMediaTypeOut, &streamIndex));   

    // Set the input media type.
    TEST_HR(MFCreateMediaType(&pMediaTypeIn));   
    TEST_HR(pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));   
    TEST_HR(pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));     
    TEST_HR(pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
    TEST_HR(MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, width, height));   
    TEST_HR(MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, fps, 1));   
    TEST_HR(MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));   
    TEST_HR(pSinkWriter->SetInputMediaType(streamIndex, pMediaTypeIn, NULL));   

    // Tell the sink writer to start accepting data.
    TEST_HR(pSinkWriter->BeginWriting());
    //if (SUCCEEDED(hr))
    //{
    //    hr = pSinkWriter->BeginWriting();
    //}

    // Return the pointer to the caller.
    if (SUCCEEDED(hr))
    {
        *ppWriter = pSinkWriter;
        (*ppWriter)->AddRef();
        *pStreamIndex = streamIndex;
    }

    SafeRelease(&pSinkWriter);
    SafeRelease(&pMediaTypeOut);
    SafeRelease(&pMediaTypeIn);
    return hr;
}


// Format constants
const UINT32 VIDEO_WIDTH = 640;
const UINT32 VIDEO_HEIGHT = 480;
const UINT32 VIDEO_FPS = 30;
const UINT64 VIDEO_FRAME_DURATION = 10 * 1000 * 1000 / VIDEO_FPS;
//const UINT32 VIDEO_BIT_RATE = 800000;
//const GUID   VIDEO_ENCODING_FORMAT = MFVideoFormat_WMV3;
//const GUID   VIDEO_INPUT_FORMAT = MFVideoFormat_RGB32;
const UINT32 VIDEO_PELS = VIDEO_WIDTH * VIDEO_HEIGHT;
const UINT32 VIDEO_FRAME_COUNT = 20 * VIDEO_FPS;

// Buffer to hold the video frame data.
DWORD videoFrameBuffer[VIDEO_PELS];

HRESULT WriteFrame(
    IMFSinkWriter *pWriter, 
    DWORD streamIndex, 
    const LONGLONG& rtStart        // Time stamp.
)
{
    IMFSample *pSample = NULL;
    IMFMediaBuffer *pBuffer = NULL;

    const LONG cbWidth = 4 * VIDEO_WIDTH;
    const DWORD cbBuffer = cbWidth * VIDEO_HEIGHT;

    BYTE *pData = NULL;

    // Create a new memory buffer.
    HRESULT hr = MFCreateMemoryBuffer(cbBuffer, &pBuffer);

    // Lock the buffer and copy the video frame to the buffer.
    if (SUCCEEDED(hr))
    {
        hr = pBuffer->Lock(&pData, NULL, NULL);
    }
    if (SUCCEEDED(hr))
    {
        hr = MFCopyImage(
            pData,                      // Destination buffer.
            cbWidth,                    // Destination stride.
            (BYTE*)videoFrameBuffer,    // First row in source image.
            cbWidth,                    // Source stride.
            cbWidth,                    // Image width in bytes.
            VIDEO_HEIGHT                // Image height in pixels.
        );
    }
    if (pBuffer)
    {
        pBuffer->Unlock();
    }

    // Set the data length of the buffer.
    if (SUCCEEDED(hr))
    {
        hr = pBuffer->SetCurrentLength(cbBuffer);
    }

    // Create a media sample and add the buffer to the sample.
    if (SUCCEEDED(hr))
    {
        hr = MFCreateSample(&pSample);
    }
    if (SUCCEEDED(hr))
    {
        hr = pSample->AddBuffer(pBuffer);
    }

    // Set the time stamp and the duration.
    if (SUCCEEDED(hr))
    {
        hr = pSample->SetSampleTime(rtStart);
    }
    if (SUCCEEDED(hr))
    {
        hr = pSample->SetSampleDuration(VIDEO_FRAME_DURATION);
    }

    // Send the sample to the Sink Writer.
    if (SUCCEEDED(hr))
    {
        hr = pWriter->WriteSample(streamIndex, pSample);
    }

    SafeRelease(&pSample);
    SafeRelease(&pBuffer);
    return hr;
}

void msdn_main()
{
    // Set all pixels to green
    for (DWORD i = 0; i < VIDEO_PELS; ++i)
    {
        videoFrameBuffer[i] = 0x0000FF00;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        hr = MFStartup(MF_VERSION);
        if (SUCCEEDED(hr))
        {
            IMFSinkWriter *pSinkWriter = NULL;
            DWORD stream;

            hr = InitializeSinkWriter(&pSinkWriter, &stream, L"output.wmv", VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS);
            if (SUCCEEDED(hr))
            {
                // Send frames to the sink writer.
                LONGLONG rtStart = 0;


                for (DWORD i = 0; i < VIDEO_FRAME_COUNT; ++i)
                {
                    hr = WriteFrame(pSinkWriter, stream, rtStart);
                    if (FAILED(hr))
                    {
                        break;
                    }
                    rtStart += VIDEO_FRAME_DURATION;
                }
            }
            if (SUCCEEDED(hr))
            {
                hr = pSinkWriter->Finalize();
            }
            SafeRelease(&pSinkWriter);
            MFShutdown();
        }
        CoUninitialize();
    }
}
