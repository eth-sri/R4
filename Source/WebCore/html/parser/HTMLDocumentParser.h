/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLDocumentParser_h
#define HTMLDocumentParser_h

#include "CachedResourceClient.h"
#include "FragmentScriptingPermission.h"
#include "HTMLInputStream.h"
#include "HTMLScriptRunnerHost.h"
#include "HTMLSourceTracker.h"
#include "HTMLToken.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include "Timer.h"
#include "XSSAuditor.h"
#include <wtf/OwnPtr.h>

#include <string>

#include <wtf/EventActionDescriptor.h>

namespace WebCore {

class Document;
class DocumentFragment;
class HTMLDocument;
class HTMLParserScheduler;
class HTMLTokenizer;
class HTMLScriptRunner;
class HTMLTreeBuilder;
class HTMLPreloadScanner;
class ScriptController;
class ScriptSourceCode;

class PumpSession;

/**
 * WebERA:
 *
 * The HTMLDocumentParser is modified in WebERA to control when the HTML parser yields and allows other events
 * (XMLHttpRequests, user-interactions, timers ect.) to be scheduled.
 *
 * The parser yields after each token (see comment in pumpTokenizer()
 *
 * When the parser yields it will:
 *   - Add a timer to ThreadTimers
 *   - The timer will be named according to the next token to be parsed
 *   - Yielding will be deterministic. Regardless of application state the same sequence of events will be yielded between executions.
 *
 * Naming:
 *
 * The timer will be renamed to HTMLDocumentParser(<URL>[<TOKEN>]) each time the parser yields.
 *
 * Note that with this naming we don't support handling multiple frames showing the same URL at the same time.
 *
 *
 * TODO(WebERA-HB):
 *
 * This document contains a number of calls to ActionLog, creating happens before relations between current network
 * events and the next fragment to be parsed.
 *
 * The current setup is a bit too restrictive.
 *
 * The happens before relation is associated with a parsing event action too early in the parsing event action chain. E.g. if a network
 * event occurs that adds a new chunk of the web page, then the first parsing event action using that chunk of the page should have a
 * happens before relation, and not earlier.
 *
 * Furthermore, the "page loaded" network event action should also have a happens before much later in the chain - notice that this event
 * action has an effect on the number of parse events needed (it can subsume the last parse event action if executed late).
 *
 */

class HTMLDocumentParser :  public ScriptableDocumentParser, HTMLScriptRunnerHost, CachedResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<HTMLDocumentParser> create(HTMLDocument* document, bool reportErrors)
    {
        return adoptRef(new HTMLDocumentParser(document, reportErrors));
    }
    static PassRefPtr<HTMLDocumentParser> create(DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission permission)
    {
        return adoptRef(new HTMLDocumentParser(fragment, contextElement, permission));
    }

    virtual ~HTMLDocumentParser();

    // Exposed for HTMLParserScheduler
    void resumeParsingAfterYield();

    static void parseDocumentFragment(const String&, DocumentFragment*, Element* contextElement, FragmentScriptingPermission = FragmentScriptingAllowed);
    
    static bool usePreHTML5ParserQuirks(Document*);
    static unsigned maximumDOMTreeDepth(Document*);

    HTMLTokenizer* tokenizer() const { return m_tokenizer.get(); }
    String sourceForToken(const HTMLToken&);

    virtual TextPosition textPosition() const;
    virtual OrdinalNumber lineNumber() const;

    virtual void suspendScheduledTasks();
    virtual void resumeScheduledTasks();

    // WebERA
    std::string getDocumentUrl() const;
    unsigned long getTokensSeen() const { return m_tokensSeen; }

protected:
    virtual void insert(const SegmentedString&);
    virtual void append(const SegmentedString&);
    virtual void finish();

    HTMLDocumentParser(HTMLDocument*, bool reportErrors);
    HTMLDocumentParser(DocumentFragment*, Element* contextElement, FragmentScriptingPermission);

    HTMLTreeBuilder* treeBuilder() const { return m_treeBuilder.get(); }

private:
    // DocumentParser
    virtual void detach();
    virtual bool hasInsertionPoint();
    virtual bool finishWasCalled();
    virtual bool processingData() const;
    virtual void prepareToStopParsing();
    virtual void stopParsing();
    virtual bool isWaitingForScripts() const;
    virtual bool isExecutingScript() const;
    virtual void executeScriptsWaitingForStylesheets();

    // HTMLScriptRunnerHost
    virtual void watchForLoad(CachedResource*);
    virtual void stopWatchingForLoad(CachedResource*);
    virtual HTMLInputStream& inputStream() { return m_input; }
    virtual bool hasPreloadScanner() const { return m_preloadScanner.get(); }
    virtual void appendCurrentInputStreamToPreloadScannerAndScan();

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);

    enum SynchronousMode {
        AllowYield,
        ForceSynchronous,
    };
    bool canTakeNextToken(SynchronousMode, PumpSession&);
    void pumpTokenizer(SynchronousMode);
    void pumpTokenizerIfPossible(SynchronousMode);

    bool runScriptsForPausedTreeBuilder();
    void resumeParsingAfterScriptExecution();

    void begin();
    void attemptToEnd();
    void endIfDelayed();
    void attemptToRunDeferredScriptsAndEnd();
    void end();

    bool isParsingFragment() const;
    bool isScheduledForResume() const;
    bool inPumpSession() const { return m_pumpSessionNestingLevel > 0; }
    bool shouldDelayEnd() const { return inPumpSession() || isWaitingForScripts() || isScheduledForResume() || isExecutingScript(); }

    ScriptController* script() const;

    HTMLInputStream m_input;

    // We hold m_token here because it might be partially complete.
    HTMLToken m_token;

    OwnPtr<HTMLTokenizer> m_tokenizer;
    OwnPtr<HTMLScriptRunner> m_scriptRunner;
    OwnPtr<HTMLTreeBuilder> m_treeBuilder;
    OwnPtr<HTMLPreloadScanner> m_preloadScanner;
    OwnPtr<HTMLPreloadScanner> m_insertionPreloadScanner;
    OwnPtr<HTMLParserScheduler> m_parserScheduler;
    HTMLSourceTracker m_sourceTracker;
    XSSAuditor m_xssAuditor;

    bool m_endWasDelayed;
    unsigned m_pumpSessionNestingLevel;

    // WebERA:
    EventActionId m_lastParseEventAction;
    EventActionId m_lastNonInsertedParseEventAction;
    MultiJoinHappensBefore m_modifiedInputJoin;

    // WebERA: We could also use textPosition, however this has a bit lower overhead
    unsigned long m_tokensSeen;
};

}

#endif
