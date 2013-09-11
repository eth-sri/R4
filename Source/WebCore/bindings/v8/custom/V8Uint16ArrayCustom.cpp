/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/ArrayBuffer.h>
#include <wtf/Uint16Array.h>

#include "V8Binding.h"
#include "V8ArrayBuffer.h"
#include "V8ArrayBufferViewCustom.h"
#include "V8Uint16Array.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8Uint16Array::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Uint16Array.Contructor");

    return constructWebGLArray<Uint16Array, unsigned short>(args, &info, v8::kExternalUnsignedShortArray);
}

v8::Handle<v8::Value> V8Uint16Array::setCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Uint16Array.set()");
    return setWebGLArrayHelper<Uint16Array, V8Uint16Array>(args);
}

v8::Handle<v8::Value> toV8(Uint16Array* impl, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = V8Uint16Array::wrap(impl, isolate);
    if (!wrapper.IsEmpty())
        wrapper->SetIndexedPropertiesToExternalArrayData(impl->baseAddress(), v8::kExternalUnsignedShortArray, impl->length());
    return wrapper;
}

} // namespace WebCore
