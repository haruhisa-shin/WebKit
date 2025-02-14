/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AutomationClientWin.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "APIPageConfiguration.h"
#include "WKAPICast.h"
#include "WebAutomationSession.h"
#include "WebPageProxy.h"
#include <WebKit/WKAuthenticationChallenge.h>
#include <WebKit/WKAuthenticationDecisionListener.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKString.h>
#include <wtf/RunLoop.h>
#endif

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)

// AutomationSessionClient
AutomationSessionClient::AutomationSessionClient(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
    : m_sessionIdentifier(sessionIdentifier)
    , m_capabilities(capabilities)
    , m_dialog(WTF::makeUnique<AutomationDialog>())
{
}

void AutomationSessionClient::close(WKPageRef pageRef, const void* clientInfo)
{
    auto page = WebKit::toImpl(pageRef);
    page->setControlledByAutomation(false);

    auto sessionClient = static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo));
    sessionClient->releaseWebView(page);
}

void AutomationSessionClient::runJavaScriptAlert(WKPageRef page, WKStringRef string, WKFrameRef frame, WKSecurityOriginRef origin, WKPageRunJavaScriptAlertResultListenerRef listener, const void* clientInfo)
{
    static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo))->runJavaScriptAlert(page, string, frame, origin, listener);
}

void AutomationSessionClient::runJavaScriptAlert(WKPageRef, WKStringRef string, WKFrameRef, WKSecurityOriginRef, WKPageRunJavaScriptAlertResultListenerRef listener)
{
    m_dialog->runJavaScriptAlert(string, WTFMove(listener));
}

void AutomationSessionClient::runJavaScriptConfirm(WKPageRef page, WKStringRef string, WKFrameRef frame, WKSecurityOriginRef origin, WKPageRunJavaScriptConfirmResultListenerRef listener, const void* clientInfo)
{
    static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo))->runJavaScriptConfirm(page, string, frame, origin, listener);
}

void AutomationSessionClient::runJavaScriptConfirm(WKPageRef, WKStringRef string, WKFrameRef, WKSecurityOriginRef, WKPageRunJavaScriptConfirmResultListenerRef listener)
{
    m_dialog->runJavaScriptConfirm(string, WTFMove(listener));
}

void AutomationSessionClient::runJavaScriptPrompt(WKPageRef page, WKStringRef message, WKStringRef defaultValue, WKFrameRef frame, WKSecurityOriginRef origin, WKPageRunJavaScriptPromptResultListenerRef listener, const void* clientInfo)
{
    static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo))->runJavaScriptPrompt(page, message, defaultValue, frame, origin, listener);
}

void AutomationSessionClient::runJavaScriptPrompt(WKPageRef, WKStringRef message, WKStringRef defaultValue, WKFrameRef, WKSecurityOriginRef, WKPageRunJavaScriptPromptResultListenerRef listener)
{
    m_dialog->runJavaScriptPrompt(message, defaultValue, WTFMove(listener));
}

void AutomationSessionClient::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge, const void *clientInfo)
{
    static_cast<AutomationSessionClient*>(const_cast<void*>(clientInfo))->didReceiveAuthenticationChallenge(page, authenticationChallenge);
}

void AutomationSessionClient::didReceiveAuthenticationChallenge(WKPageRef page, WKAuthenticationChallengeRef authenticationChallenge)
{
    auto decisionListener = WKAuthenticationChallengeGetDecisionListener(authenticationChallenge);
    if (m_capabilities.acceptInsecureCertificates) {
        auto username = adoptWK(WKStringCreateWithUTF8CString("accept server trust"));
        auto password = adoptWK(WKStringCreateWithUTF8CString(""));
        auto credential = adoptWK(WKCredentialCreate(username.get(), password.get(), kWKCredentialPersistenceNone));
        WKAuthenticationDecisionListenerUseCredential(decisionListener, credential.get());
    } else
        WKAuthenticationDecisionListenerRejectProtectionSpaceAndContinue(decisionListener);
}

void AutomationSessionClient::requestNewPageWithOptions(WebKit::WebAutomationSession& session, API::AutomationSessionBrowsingContextOptions options, CompletionHandler<void(WebKit::WebPageProxy*)>&& completionHandler)
{
    auto pageConfiguration = API::PageConfiguration::create();
    pageConfiguration->setProcessPool(session.protectedProcessPool());

    RECT r { };
    Ref newWindow = WebView::create(r, pageConfiguration, 0);

    auto newPage = newWindow->page();
    newPage->setControlledByAutomation(true);

    WKPageUIClientV6 uiClient = { };
    uiClient.base.version = 6;
    uiClient.base.clientInfo = this;
    uiClient.close = close;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    uiClient.runJavaScriptConfirm = runJavaScriptConfirm;
    uiClient.runJavaScriptPrompt = runJavaScriptPrompt;
    WKPageSetPageUIClient(toAPI(newPage), &uiClient.base);

    WKPageNavigationClientV0 navigationClient = { };
    navigationClient.base.version = 0;
    navigationClient.base.clientInfo = this;
    navigationClient.didReceiveAuthenticationChallenge = didReceiveAuthenticationChallenge;
    WKPageSetPageNavigationClient(toAPI(newPage), &navigationClient.base);

    retainWebView(WTFMove(newWindow));

    completionHandler(newPage);
}

void AutomationSessionClient::didDisconnectFromRemote(WebKit::WebAutomationSession& session)
{
    session.setClient(nullptr);

    RunLoop::protectedMain()->dispatch([&session] {
        auto processPool = session.protectedProcessPool();
        if (processPool) {
            processPool->setAutomationSession(nullptr);
            processPool->setPagesControlledByAutomation(false);
        }
    });
}

bool AutomationSessionClient::isShowingJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&)
{
    return m_dialog->isShowing();
}

void AutomationSessionClient::dismissCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&)
{
    m_dialog->dismiss();
}

void AutomationSessionClient::acceptCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&)
{
    m_dialog->accept();
}

WTF::String AutomationSessionClient::messageOfCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&)
{
    return m_dialog->message();
}

void AutomationSessionClient::setUserInputForCurrentJavaScriptPromptOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&, const WTF::String& string)
{
    m_dialog->setUserInputFotPrompt(string);
}

std::optional<AutomationSessionClient::JavaScriptDialogType> AutomationSessionClient::typeOfCurrentJavaScriptDialogOnPage(WebKit::WebAutomationSession&, WebKit::WebPageProxy&)
{
    return m_dialog->type();
}

void AutomationSessionClient::retainWebView(Ref<WebView>&& webView)
{
    m_webViews.add(WTFMove(webView));
}

void AutomationSessionClient::releaseWebView(WebPageProxy* page)
{
    m_webViews.removeIf([&](auto& view) {
        if (view->page() == page) {
            view->close();
            return true;
        }
        return false;
    });
}

// AutomationClient
AutomationClient::AutomationClient(WebProcessPool& processPool)
    : m_processPool(processPool)
{
    Inspector::RemoteInspector::singleton().setClient(this);
}

AutomationClient::~AutomationClient()
{
    Inspector::RemoteInspector::singleton().setClient(nullptr);
}

RefPtr<WebProcessPool> AutomationClient::protectedProcessPool() const
{
    if (RefPtr processPool = m_processPool.get())
        return processPool;

    return nullptr;
}

void AutomationClient::requestAutomationSession(const String& sessionIdentifier, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
{
    ASSERT(isMainRunLoop());

    auto session = adoptRef(new WebAutomationSession());
    session->setSessionIdentifier(sessionIdentifier);
    session->setClient(WTF::makeUnique<AutomationSessionClient>(sessionIdentifier, capabilities));
    m_processPool->setAutomationSession(WTFMove(session));
}

void AutomationClient::closeAutomationSession()
{
    RunLoop::protectedMain()->dispatch([this] {
        auto processPool = protectedProcessPool();
        if (!processPool || !processPool->automationSession())
            return;

        processPool->automationSession()->setClient(nullptr);
        processPool->setAutomationSession(nullptr);
    });
}

#endif

} // namespace WebKit
