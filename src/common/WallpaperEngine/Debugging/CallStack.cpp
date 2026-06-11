/*
* https://github.com/ZhangJunQCC/CallStack-for-Windows-and-Linux
BSD 3-Clause License

Copyright (c) 2017, Jun Zhang
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

//
// Author: Jun Zhang
// mailto: zhangjunqcc@gmail.com
//

#include "CallStack.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <imagehlp.h>
#include <windows.h>
#if defined(__MINGW32__)
#define PACKAGE 1 // Supress cmake error.
#define PACKAGE_VERSION 1 // Supress cmake error.
#include <bfd.h>
#include <cxxabi.h>
#endif
#else
#include <cxxabi.h>
#include <execinfo.h>
#endif

using namespace WallpaperEngine::Debugging;
using namespace std;

const char* CallStack::Unknown_Function = "[unknown function]";
const char* CallStack::Unknown_Module = "[unknown module]";

CallStack::CallStack (void) { /* Nothing to do */ }

CallStack::~CallStack (void) { /* Nothing to do */ }

string CallStack::Demangle (const char* name) {
#if defined(_MSC_VER)
    return string (name);
#else
    string dname;
    int status = 0;
    char* pdname = abi::__cxa_demangle (name, NULL, 0, &status);
    if (status == 0) {
	dname.assign (pdname);
	free (pdname);
    } else {
	dname.assign (CallStack::Unknown_Function);
    }
    return dname;
#endif
}

#if defined(_WIN32)
class noncopyable // Like the one in boost.
{
protected:
    noncopyable (void) { /* Nothing to do */ }

private:
    noncopyable (const noncopyable&);
    noncopyable& operator= (const noncopyable&);
};
// A global thread mutex class.
class mutex : noncopyable {
public:
    mutex (void) { InitializeCriticalSection (&cs); }
    ~mutex (void) { DeleteCriticalSection (&cs); }
    void lock (void) { EnterCriticalSection (&cs); }
    void unlock (void) { LeaveCriticalSection (&cs); }

private:
    CRITICAL_SECTION cs;
} cs;
#if defined(__MINGW32__)
class bfd_usage : noncopyable {
public:
    bfd_usage (void) : abfd (NULL), sec (NULL), symbol_table (NULL) {
	char procname[MAX_PATH];
	GetModuleFileName (NULL, procname, sizeof (procname));

	bfd_init ();
	abfd = bfd_openr (procname, NULL); // Obtain the bfd pointer.
	if (abfd == NULL) {
	    fprintf (stderr, "Error: Failed to parse the executable format.\n");
	    return;
	}

	char** matching = NULL;
	if (!(bfd_check_format (abfd, bfd_object) && // Whether the executable format is supported.
	      bfd_check_format_matches (abfd, bfd_object, &matching) && // Whether the executable format is supported.
	      (bfd_get_file_flags (abfd) & HAS_SYMS))) // Whether BFD has symbols
	{
	    bfd_close (abfd);
	    free (matching);
	    fprintf (stderr, "Error: Failed to parse the executable format.\n");
	    return;
	}
	free (matching);

	// Obtain the symbol table.
	unsigned int dummy = 0;
	if (bfd_read_minisymbols (abfd, FALSE, reinterpret_cast<void**> (&symbol_table), &dummy) == 0
	    && bfd_read_minisymbols (abfd, TRUE, reinterpret_cast<void**> (&symbol_table), &dummy) < 0) {
	    free (symbol_table);
	    bfd_close (abfd);
	    fprintf (stderr, "Error: Failed to obtain the symbol table from the executable.\n");
	    return;
	}
    }

    ~bfd_usage (void) {
	free (symbol_table);
	bfd_close (abfd);
    }

    string get_function_name (DWORD offset) {
	fnis_para para;
	para.symbol_table = symbol_table;
	para.counter = offset;
	bfd_map_over_sections (abfd, &find_function_name_in_section, &para); // Call the corresponding functions.
	return para.function;
    }

private:
    bfd* abfd;
    asection* sec;
    asymbol** symbol_table;

    class fnis_para {
    public:
	string function;
	asymbol** symbol_table;
	bfd_vma counter;
    };
    static void find_function_name_in_section (bfd* tabfd, asection* tsec, void* tpara) {
	fnis_para& para = *static_cast<fnis_para*> (tpara);
	if (!para.function.empty ()) {
	    return; // We have got the name.
	}
	if (!(bfd_get_section_flags (tabfd, tsec) & SEC_ALLOC)) {
	    return; // Allocate space for this section.
	}
	bfd_vma vma = bfd_get_section_vma (tabfd, tsec);
	if (para.counter < vma || vma + bfd_get_section_size (tsec) <= para.counter) {
	    return;
	}
	const char* function = NULL;
	const char* file = NULL;
	unsigned int line = 0;
	// Get corresponding file and function name, and line number.
	if (bfd_find_nearest_line (tabfd, tsec, para.symbol_table, para.counter - vma, &file, &function, &line)
	    && function) {
	    para.function = CallStack::Demangle (function);
	}
    }
};
#endif // __MINGW32__
#endif // _WIN32

void CallStack::GetCalls (vector<CallStack::CallInfo>& calls) {
#if defined(_WIN32)

    // Prepare context.
    cs.lock ();
    if (!SymInitialize (GetCurrentProcess (), 0, TRUE)) {
	fprintf (stderr, "Error: Failed to initialize symbol context.\n");
	return;
    }
#ifdef __MINGW32__
    bfd_usage bfdu;
#endif

    // Prepare variables.
#if defined(_M_AMD64)
    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
    const DWORD machine = IMAGE_FILE_MACHINE_I386;
#endif
    const HANDLE process = GetCurrentProcess ();
    const HANDLE thread = GetCurrentThread ();
    STACKFRAME frame = { 0 };
    CONTEXT context = { 0 };
    context.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext (&context);
#if defined(_M_AMD64)
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
#else
    frame.AddrPC.Offset = context.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
#endif
    int limit = 0;
    const bool has_limit = (limit != 0);
    char symbol_buffer[sizeof (IMAGEHLP_SYMBOL) + 255];
    char module_name_raw[MAX_PATH];

    // Go over stack.
    calls.clear ();
    while (StackWalk (machine, process, thread, &frame, &context, 0, SymFunctionTableAccess, SymGetModuleBase, 0)) {
	if (has_limit && limit == 0) {
	    break;
	}
	--limit;

	IMAGEHLP_SYMBOL* symbol = reinterpret_cast<IMAGEHLP_SYMBOL*> (symbol_buffer);
	symbol->SizeOfStruct = (sizeof (*symbol)) + 255;
	symbol->MaxNameLength = 254;

#if defined(_WIN64)
	const DWORD64 module_base = SymGetModuleBase (process, frame.AddrPC.Offset);
#else
	const DWORD module_base = SymGetModuleBase (process, frame.AddrPC.Offset);
#endif
	string module_name = Unknown_Module;
	if (module_base && GetModuleFileNameA (reinterpret_cast<HINSTANCE> (module_base), module_name_raw, MAX_PATH)) {
	    module_name = module_name_raw;
	}

#if defined(__MINGW32__)
	string function = bfdu.get_function_name (frame.AddrPC.Offset);
	if (function.empty ()) {
	    DWORD64 dummy = 0;
	    const bool got_symbol = SymGetSymFromAddr (process, frame.AddrPC.Offset, &dummy, symbol);
	    function = (got_symbol) ? (symbol->Name) : Unknown_Function;
	}
#else
#if defined(_WIN64)
	DWORD64 dummy = 0;
#else
	DWORD dummy = 0;
#endif
	const bool got_symbol = SymGetSymFromAddr (process, frame.AddrPC.Offset, &dummy, symbol);
	const string function = (got_symbol) ? symbol->Name : Unknown_Function;

#endif

	calls.push_back (CallInfo (frame.AddrPC.Offset, function, module_name));
    }

    SymCleanup (GetCurrentProcess ());
    cs.unlock ();

#else // For Linux and Unix.

    const int BufferSize = 256;
    void* buffer[BufferSize];
    const int ncalls = backtrace (buffer, sizeof (buffer));
    char** callstrings = backtrace_symbols (buffer, ncalls);

    if (callstrings == NULL) {
	fprintf (stderr, "Error: Failed to obtain the symbol table from the executable.\n");
	return;
    }

    calls.clear ();
    for (int i = 0; i < ncalls; ++i) {
	const string funinfo (callstrings[i]);
	const int posleftparenthesis = funinfo.rfind ('(');
	const int posplus = funinfo.rfind ('+');
	const int posleftbracket = funinfo.rfind ('[');
	const int posrightbracket = funinfo.rfind (']');

	long long unsigned int offset
	    = strtoull (funinfo.substr (posleftbracket + 1, posrightbracket - posleftbracket - 1).c_str (), NULL, 0);
	const string module_name = funinfo.substr (0, posleftparenthesis);
	const string function
	    = Demangle (funinfo.substr (posleftparenthesis + 1, posplus - posleftparenthesis - 1).c_str ());

	calls.push_back (CallInfo (offset, function, module_name));
    }
    free (callstrings);

#endif // _WIN32
}
