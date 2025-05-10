// dllmain.cpp : Define el punto de entrada de la aplicación DLL.
#include "pch.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#pragma warning(disable:4996)

typedef HRESULT (*OriginalD3DCompile)(
    LPCVOID                pSrcData,
    SIZE_T                 SrcDataSize,
    LPCSTR                 pSourceName,
    const void* pDefines,
    void* pInclude,
    LPCSTR                 pEntrypoint,
    LPCSTR                 pTarget,
    UINT                   Flags1,
    UINT                   Flags2,
    void** ppCode,
    void** ppErrorMsgs
);

typedef HRESULT (*OriginalD3DReflect)(
    LPCVOID pSrcData,
    SIZE_T  SrcDataSize,
    REFIID  pInterface,
    void** ppReflector
);

HMODULE original_lib = NULL;
OriginalD3DCompile ExternalD3DCompile = NULL;
OriginalD3DReflect ExternalD3DReflect = NULL;
FILE* fp = NULL;

extern "C"
{
__declspec(dllexport) HRESULT WINAPI D3DCompile(
    LPCVOID                pSrcData,
    SIZE_T                 SrcDataSize,
    LPCSTR                 pSourceName,
    const void* pDefines,
    void* pInclude,
    LPCSTR                 pEntrypoint,
    LPCSTR                 pTarget,
    UINT                   Flags1,
    UINT                   Flags2,
    void** ppCode,
    void** ppErrorMsgs
)
{
    if (fp != NULL)
    {
        fprintf(fp, "////////////////////////////////////////////////////////////////////////////\r\n");
        fprintf(fp, "Shader: %s\n", pSourceName);
        fprintf(fp, "////////////////////////////////////////////////////////////////////////////\r\n");
        fwrite(pSrcData, SrcDataSize, 1, fp);
        fflush (fp);
    }

    // includes D3DCOMPILE_DEBUG
    Flags1 |= (1 << 0) | (1 << 2);

    return ExternalD3DCompile(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}

__declspec(dllexport) HRESULT WINAPI D3DReflect(
    LPCVOID pSrcData,
    SIZE_T  SrcDataSize,
    REFIID  pInterface,
    void** ppReflector
)
{
    return ExternalD3DReflect(pSrcData, SrcDataSize, pInterface, ppReflector);
}

}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    char shadersFile[256];
    memset(shadersFile, 0, 256);
    sprintf(shadersFile, "C:\\development\\shaders_%lld_%i.log", time(NULL), rand());
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        fp = fopen(shadersFile, "ab+");

        if (fp == NULL) {
            return FALSE;
        }

        fprintf(fp, "=====================================================================================\n");
        fputs("Booting up shader compiler shim!", fp);

        original_lib = LoadLibrary(TEXT("d3dcompiler_47original.dll"));
        if (!original_lib)
            original_lib = LoadLibraryEx(TEXT("d3dcompiler_47original.dll"), 0, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!original_lib)
        {
            fputs("Could not resolve DLL!\r\n", fp);
            fclose(fp);
            fp = NULL;
            return FALSE;
        }
        
        ExternalD3DCompile = (OriginalD3DCompile) GetProcAddress(original_lib, "D3DCompile");
        ExternalD3DReflect = (OriginalD3DReflect) GetProcAddress(original_lib, "D3DReflect");

        if (!ExternalD3DReflect) {
            fputs("Could not locate D3DReflect in d3dcompiler_47original.dll\r\n", fp);
        } else {
            fputs("Located D3DReflect in d3dcompiler_47original.dll\r\n", fp);
        }

        if (!ExternalD3DCompile) {
            fputs("Could not locate D3DCompile in d3dcompiler_47original.dll\r\n", fp);
        } else {
            fputs("Located D3DCompile in d3dcompiler_47original.dll\r\n", fp);
        }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        fputs("Closing...\r\n", fp);
        fclose(fp);
        fp = NULL;
        break;
    }
    return TRUE;
}

