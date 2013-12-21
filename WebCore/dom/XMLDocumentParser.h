/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef XMLDocumentParser_h
#define XMLDocumentParser_h

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "FragmentScriptingPermission.h"
#include "ScriptableDocumentParser.h"
#include "SegmentedString.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>

#if USE(QXMLSTREAM)
#include <xml/qxmlstream.h>
#elif USE(_LIBXML)
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#else
#include <expat/expat.h>
#endif

namespace WebCore {

    class Node;
    class CachedScript;
    class CachedResourceLoader;
    class DocumentFragment;
    class Document;
    class Element;
    class FrameView;
    class PendingCallbacks;
    class ScriptElement;

// #if !USE(QXMLSTREAM)
//     class XMLParserContext : public RefCounted<XMLParserContext> {
//     public:
//         static PassRefPtr<XMLParserContext> createMemoryParser(xmlSAXHandlerPtr, void* userData, const CString& chunk);
//         static PassRefPtr<XMLParserContext> createStringParser(xmlSAXHandlerPtr, void* userData);
//         ~XMLParserContext();
//         xmlParserCtxtPtr context() const { return m_context; }
// 
//     private:
//         XMLParserContext(xmlParserCtxtPtr context)
//             : m_context(context)
//         {
//         }
//         xmlParserCtxtPtr m_context;
//     };
// #endif

    typedef HashMap<AtomicString, AtomicString> URIToPrefixMap;
    class XMLParserContext : public RefCounted<XMLParserContext> {
    public:
        static PassRefPtr<XMLParserContext> createMemoryParser(void* userData, const CString& chunk);
        static PassRefPtr<XMLParserContext> createStringParser(void* userData);
        ~XMLParserContext();
        XML_Parser getParser() const { return m_parser; }

        URIToPrefixMap m_URIToPrefixMap;
        URIToPrefixMap m_temURIToPrefixMap; // 临时的，因为expat的ns回调和startElementNs回调不在一块


    private:
        XMLParserContext(XML_Parser parser);
        XML_Parser m_parser;
    };

    class XMLDocumentParser : public ScriptableDocumentParser, public CachedResourceClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static PassRefPtr<XMLDocumentParser> create(Document* document, FrameView* view)
        {
            return adoptRef(new XMLDocumentParser(document, view));
        }
        static PassRefPtr<XMLDocumentParser> create(DocumentFragment* fragment, Element* element, FragmentScriptingPermission permission)
        {
            return adoptRef(new XMLDocumentParser(fragment, element, permission));
        }

        ~XMLDocumentParser();

        // Exposed for callbacks:
        enum ErrorType { warning, nonFatal, fatal };
        void handleError(ErrorType, const char* message, int lineNumber, int columnNumber);
        void handleError(ErrorType, const char* message, TextPosition1);

        void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
        bool isXHTMLDocument() const { return m_isXHTMLDocument; }
#if ENABLE(XHTMLMP)
        void setIsXHTMLMPDocument(bool isXHTML) { m_isXHTMLMPDocument = isXHTML; }
        bool isXHTMLMPDocument() const { return m_isXHTMLMPDocument; }
#endif

        static bool parseDocumentFragment(const String&, DocumentFragment*, Element* parent = 0, FragmentScriptingPermission = FragmentScriptingAllowed);

        // FIXME: This function used to be used by WML. Can we remove it?
        virtual bool wellFormed() const { return !m_sawError; }

        TextPosition0 textPosition() const;
        TextPosition1 textPositionOneBased() const;

        static bool supportsXMLVersion(const String&);

    private:
        XMLDocumentParser(Document*, FrameView* = 0);
        XMLDocumentParser(DocumentFragment*, Element*, FragmentScriptingPermission);

        // From DocumentParser
        virtual void insert(const SegmentedString&);
        virtual void append(const SegmentedString&);
        virtual void finish();
        virtual bool finishWasCalled();
        virtual bool isWaitingForScripts() const;
        virtual void stopParsing();
        virtual void detach();
        virtual int lineNumber() const;
        int columnNumber() const;

        // from CachedResourceClient
        virtual void notifyFinished(CachedResource*);

        void end();

        void pauseParsing();
        void resumeParsing();

        bool appendFragmentSource(const String&);

#if USE(QXMLSTREAM)
private:
        void parse();
        void startDocument();
        void parseStartElement();
        void parseEndElement();
        void parseCharacters();
        void parseProcessingInstruction();
        void parseCdata();
        void parseComment();
        void endDocument();
        void parseDtd();
        bool hasError() const;
#elif USE(_LIBXML)
public:
        // callbacks from parser SAX
        void error(ErrorType, const char* message, va_list args) WTF_ATTRIBUTE_PRINTF(3, 0);
        void startElementNs(const xmlChar* xmlLocalName, const xmlChar* xmlPrefix, const xmlChar* xmlURI, int nb_namespaces,
                            const xmlChar** namespaces, int nb_attributes, int nb_defaulted, const xmlChar** libxmlAttributes);
        void endElementNs();
        void characters(const xmlChar* s, int len);
        void processingInstruction(const xmlChar* target, const xmlChar* data);
        void cdataBlock(const xmlChar* s, int len);
        void comment(const xmlChar* s);
        void startDocument(const xmlChar* version, const xmlChar* encoding, int standalone);
        void internalSubset(const xmlChar* name, const xmlChar* externalID, const xmlChar* systemID);
        void endDocument();
#else
public:
        void error(ErrorType, const char* message);
        void preStartElementNs(const XML_Char *name, const XML_Char **atts);
        void startElementNs(URIToPrefixMap& uriToPrefixMap, const XML_Char *name, const XML_Char **atts);
        void endElementNs();
        void StartNSDecl(AtomicString prefix, AtomicString uri);
        void characters(const XML_Char* s, int len);
        void processingInstruction(const XML_Char* target, const XML_Char* data);
        void cdataBlock(const XML_Char* s, int len);
        void comment(const XML_Char* s);
        void internalSubset(const XML_Char* name, const XML_Char* externalID, const XML_Char* systemID);
        void startDocument(const XML_Char* version, const XML_Char* encoding, int standalone);
        void endDocument();
#endif
    private:
        void initializeParserContext(const CString& chunk = CString());

        void pushCurrentNode(Node*);
        void popCurrentNode();
        void clearCurrentNodeStack();

        void insertErrorMessageBlock();

        void enterText();
        void exitText();

        void doWrite(const String&);
        void doEnd();

        FrameView* m_view;

        String m_originalSourceForTransform;

#if USE(QXMLSTREAM)
        QXmlStreamReader m_stream;
        bool m_wroteText;
#elif USE(_LIBXML)
        xmlParserCtxtPtr context() const { return m_context ? m_context->context() : 0; };
        RefPtr<XMLParserContext> m_context;
        OwnPtr<PendingCallbacks> m_pendingCallbacks;
        Vector<xmlChar> m_bufferedText;
#else 
        RefPtr<XMLParserContext> m_context;
        OwnPtr<PendingCallbacks> m_pendingCallbacks;
        String m_bufferedText;
public:
        URIToPrefixMap& getURIToPrefixMap() {return m_context->m_URIToPrefixMap;}
private:

#endif
        Node* m_currentNode;
        Vector<Node*> m_currentNodeStack;

        bool m_sawError;
        bool m_sawCSS;
        bool m_sawXSLTransform;
        bool m_sawFirstElement;
        bool m_isXHTMLDocument;
#if ENABLE(XHTMLMP)
        bool m_isXHTMLMPDocument;
        bool m_hasDocTypeDeclaration;
#endif

        bool m_parserPaused;
        bool m_requestingScript;
        bool m_finishCalled;

        int m_errorCount;
        TextPosition1 m_lastErrorPosition;
        String m_errorMessages;

        CachedResourceHandle<CachedScript> m_pendingScript;
        RefPtr<Element> m_scriptElement;
        TextPosition1 m_scriptStartPosition;

        bool m_parsingFragment;
        AtomicString m_defaultNamespaceURI;

        typedef HashMap<AtomicString, AtomicString> PrefixForNamespaceMap;
        PrefixForNamespaceMap m_prefixToNamespaceMap;
        SegmentedString m_pendingSrc;
        FragmentScriptingPermission m_scriptingPermission;
    };

#if ENABLE(XSLT)
void* xmlDocPtrForString(CachedResourceLoader*, const String& source, const String& url);
#endif

HashMap<String, String> parseAttributes(const String&, bool& attrsOK);

} // namespace WebCore

#endif // XMLDocumentParser_h
