/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 */

#include <iostream>

#include "config.h"
#include "CSSImageValue.h"

#include "CSSCursorImageValue.h"
#include "CSSParser.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "MemoryCache.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "StyleCachedImage.h"
#include "StylePendingImage.h"

#include "WebCore/platform/EventActionHappensBeforeReport.h"
#include "WebCore/dom/Element.h"

namespace WebCore {

CSSImageValue::CSSImageValue(ClassType classType, const String& url)
    : CSSValue(classType)
    , m_url(url)
    , m_accessedImage(false)
    , m_source(HBCurrentEventAction())
{
}

CSSImageValue::CSSImageValue(const String& url)
    : CSSValue(ImageClass)
    , m_url(url)
    , m_accessedImage(false)
    , m_source(HBCurrentEventAction())
{
}

CSSImageValue::CSSImageValue(const String& url, StyleImage* image)
    : CSSValue(ImageClass)
    , m_url(url)
    , m_image(image)
    , m_accessedImage(true)
    , m_source(HBCurrentEventAction())
{
}

CSSImageValue::~CSSImageValue()
{
}

StyleImage* CSSImageValue::cachedOrPendingImage()
{
    if (!m_image)
        m_image = StylePendingImage::create(this);

    return m_image.get();
}

StyleCachedImage* CSSImageValue::cachedImage(Element* element, CachedResourceLoader* loader)
{
    if (isCursorImageValue())
        return static_cast<CSSCursorImageValue*>(this)->cachedImage(element, loader);
    return cachedImage(element, loader, m_url);
}

StyleCachedImage* CSSImageValue::cachedImage(Element* element, CachedResourceLoader* loader, const String& url)
{
    ASSERT(loader);

    if (!m_accessedImage) {
        m_accessedImage = true;

        ResourceRequest request(loader->document()->completeURL(url));

        // WebERA
        request.setCalleeEventAction(m_source);

        if (element != NULL) {
            request.setSenderEventAction(element->getCreatingEventAction());
        } else {
            std::cerr << "WebERA(Warning): CSSImageValue used by unknown DOM Node. An image is being fetched because of a CSS rule matching a DOM element. Because of missing instrumentation, we can't insert a HB relation between the event action inserting the DOM element and the image load." << std::endl;
        }

        if (CachedImage* cachedImage = loader->requestImage(request))
            m_image = StyleCachedImage::create(cachedImage);
    }

    return (m_image && m_image->isCachedImage()) ? static_cast<StyleCachedImage*>(m_image.get()) : 0;
}

String CSSImageValue::cachedImageURL()
{
    if (!m_image || !m_image->isCachedImage())
        return String();
    return static_cast<StyleCachedImage*>(m_image.get())->cachedImage()->url();
}

void CSSImageValue::clearCachedImage()
{
    m_image = 0;
    m_accessedImage = false;
}

String CSSImageValue::customCssText() const
{
    return "url(" + quoteCSSURLIfNeeded(m_url) + ")";
}

PassRefPtr<CSSValue> CSSImageValue::cloneForCSSOM() const
{
    // NOTE: We expose CSSImageValues as URI primitive values in CSSOM to maintain old behavior.
    RefPtr<CSSPrimitiveValue> uriValue = CSSPrimitiveValue::create(m_url, CSSPrimitiveValue::CSS_URI);
    uriValue->setCSSOMSafe();
    return uriValue.release();
}

} // namespace WebCore
