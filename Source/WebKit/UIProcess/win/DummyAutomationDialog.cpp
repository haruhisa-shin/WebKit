/*
 * Copyright (C) 2025 Sony Interactive Entertainment Inc.
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
#include "AutomationDialog.h"

#if ENABLE(REMOTE_INSPECTOR)
#include "WKAPICast.h"
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKString.h>
#endif

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)

void AutomationDialog::runJavaScriptAlert(WKStringRef message, WKPageRunJavaScriptAlertResultListenerRef listener)
{
    if (isShowing())
        return;
    m_dialog = API::AutomationSessionClient::JavaScriptDialogType::Alert;
    m_message = toWTFString(message);
    WKRetain(listener);
    m_pendingAlertListener = listener;
}

void AutomationDialog::runJavaScriptConfirm(WKStringRef message, WKPageRunJavaScriptConfirmResultListenerRef listener)
{
    if (isShowing())
        return;
    m_dialog = API::AutomationSessionClient::JavaScriptDialogType::Confirm;
    m_message = toWTFString(message);
    WKRetain(listener);
    m_pendingConfirmListener = listener;
}

void AutomationDialog::runJavaScriptPrompt(WKStringRef message, WKStringRef defaultValue, WKPageRunJavaScriptPromptResultListenerRef listener)
{
    if (isShowing())
        return;
    m_dialog = API::AutomationSessionClient::JavaScriptDialogType::Prompt;
    m_message = toWTFString(message);
    m_promptValue = toWTFString(defaultValue);
    WKRetain(listener);
    m_pendingPromptListener = listener;
}

std::optional<API::AutomationSessionClient::JavaScriptDialogType> AutomationDialog::type()
{
    return m_dialog;
}

bool AutomationDialog::isShowing()
{
    return m_dialog.has_value();
}

String AutomationDialog::message() const
{
    return m_message;
}

void AutomationDialog::setUserInputFotPrompt(const String& string)
{
    m_promptValue = string;
}

String AutomationDialog::prompt() const
{
    return m_promptValue;
}

void AutomationDialog::close(bool result)
{
    if (!m_dialog.has_value())
        return;

    switch (*m_dialog) {
    case API::AutomationSessionClient::JavaScriptDialogType::Alert:
        WKPageRunJavaScriptAlertResultListenerCall(m_pendingAlertListener);
        WKRelease(m_pendingAlertListener);
        m_pendingAlertListener = nullptr;
        break;
    case API::AutomationSessionClient::JavaScriptDialogType::Confirm:
        WKPageRunJavaScriptConfirmResultListenerCall(m_pendingConfirmListener, result);
        WKRelease(m_pendingConfirmListener);
        m_pendingConfirmListener = nullptr;
        break;
    case API::AutomationSessionClient::JavaScriptDialogType::Prompt:
        if (result) {
            auto promptValue = adoptWK(WKStringCreateWithUTF8CString(m_promptValue.utf8().data()));
            WKPageRunJavaScriptPromptResultListenerCall(m_pendingPromptListener, promptValue.get());
        } else
            WKPageRunJavaScriptPromptResultListenerCall(m_pendingPromptListener, nullptr);
        WKRelease(m_pendingPromptListener);
        m_pendingPromptListener = nullptr;
        m_promptValue = { };
        break;
    default:
        break;
    }

    m_dialog = std::nullopt;
    m_message = { };
}

#endif

} // namespace WebKit
