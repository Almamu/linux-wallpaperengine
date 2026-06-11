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

#ifndef __CALLSTACK_H__
#define __CALLSTACK_H__

#include <string>
#include <vector>

namespace WallpaperEngine::Debugging {

using namespace std;

class CallStack {
public:
    CallStack (void);
    ~CallStack (void);

    static const char* Unknown_Function;
    static const char* Unknown_Module;

    class CallInfo {
    public:
	CallInfo (void) { /* Nothing to do */ }
	CallInfo (long long unsigned int toffset, const string& tfunction, const string& tmodule) :
	    offset (toffset), function (tfunction), module (tmodule) { /* Nothing to do */ }
	~CallInfo (void) { /* Nothing to do */ }
	long unsigned int offset;
	string function;
	string module;
    };

    static void GetCalls (vector<CallStack::CallInfo>& calls); // Get the call information.
    static string Demangle (const char* name); // Demangle the C++ function name.
};

}

#endif //  __CALLSTACK_H__
