/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SymbolTable_h
#define SymbolTable_h

#include "JSObject.h"
#include "UString.h"
#include <wtf/AlwaysInline.h>
#include <wtf/HashTraits.h>
#include <wtf/text/CString.h>

#include <vector>

namespace JSC {

    static ALWAYS_INLINE int missingSymbolMarker() { return std::numeric_limits<int>::max(); }

    // The bit twiddling in this class assumes that every register index is a
    // reasonably small positive or negative number, and therefore has its high
    // four bits all set or all unset.

    struct SymbolTableEntry {
        SymbolTableEntry()
            : m_bits(0)
        {
        }

        SymbolTableEntry(int index)
        {
            ASSERT(isValidIndex(index));
            pack(index, false, false);
        }

        SymbolTableEntry(int index, unsigned attributes)
        {
            ASSERT(isValidIndex(index));
            pack(index, attributes & ReadOnly, attributes & DontEnum);
        }
        
        bool isNull() const
        {
            return !m_bits;
        }

        int getIndex() const
        {
            return m_bits >> FlagBits;
        }

        unsigned getAttributes() const
        {
            unsigned attributes = 0;
            if (m_bits & ReadOnlyFlag)
                attributes |= ReadOnly;
            if (m_bits & DontEnumFlag)
                attributes |= DontEnum;
            return attributes;
        }

        void setAttributes(unsigned attributes)
        {
            pack(getIndex(), attributes & ReadOnly, attributes & DontEnum);
        }

        bool isReadOnly() const
        {
            return m_bits & ReadOnlyFlag;
        }

    private:
        static const unsigned ReadOnlyFlag = 0x1;
        static const unsigned DontEnumFlag = 0x2;
        static const unsigned NotNullFlag = 0x4;
        static const unsigned FlagBits = 3;

        void pack(int index, bool readOnly, bool dontEnum)
        {
            m_bits = (index << FlagBits) | NotNullFlag;
            if (readOnly)
                m_bits |= ReadOnlyFlag;
            if (dontEnum)
                m_bits |= DontEnumFlag;
        }
        
        bool isValidIndex(int index)
        {
            return ((index << FlagBits) >> FlagBits) == index;
        }

        int m_bits;
    };

    struct SymbolTableIndexHashTraits : HashTraits<SymbolTableEntry> {
        static const bool emptyValueIsZero = true;
        static const bool needsDestruction = false;
    };

    // SRL: To keep the field name as string for the log, we replace the defintion of SymbolTable.
    // We WebKit interpreter replaces string identifier names with integer values and the SymbolTable keeps
    // this mapping. Normally, no reverse lookup is needed, however our log includes the actual field names.
    // That's why we also keep a reverse index in SymbolTable in addition to the HashMap.
    //
    // For this to work, any users inserting in the map need to call addReverseEntry() too.
    class SymbolTable : public HashMap<RefPtr<StringImpl>, SymbolTableEntry, IdentifierRepHash, HashTraits<RefPtr<StringImpl> >, SymbolTableIndexHashTraits> {
    	WTF_MAKE_FAST_ALLOCATED;
    public:
    	void addReverseEntry(int index, const CString& entry) {
    		while (index >= static_cast<int>(m_reverseData.size())) {
    			m_reverseData.push_back(CString("?"));
    		}
    		m_reverseData[index] = entry;
    	}

    	const char* resolveReverseSymbolName(int index) {
    		if (index < 0 || index >= static_cast<int>(m_reverseData.size())) return "?";
    		return m_reverseData[index].data();
    	}
    private:
    	std::vector<CString> m_reverseData;
    };

    //typedef HashMap<RefPtr<StringImpl>, SymbolTableEntry, IdentifierRepHash, HashTraits<RefPtr<StringImpl> >, SymbolTableIndexHashTraits> SymbolTable;

    class SharedSymbolTable : public SymbolTable, public RefCounted<SharedSymbolTable> {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static PassRefPtr<SharedSymbolTable> create() { return adoptRef(new SharedSymbolTable); }
    private:
        SharedSymbolTable() { turnOffVerifier(); }
    };

} // namespace JSC

#endif // SymbolTable_h
