#pragma once

#include "cef-headers.hpp"
#include "browser-panel-internal.hpp"

#include "include/wrapper/cef_message_router.h"
#include "include/cef_request_handler.h"

#include <string>


class QCefOsrBrowserClient : public CefClient,
			     public CefDisplayHandler,
			     public CefRequestHandler,
		             public CefResourceRequestHandler,
			     public CefLifeSpanHandler,
			     public CefContextMenuHandler,
			     public CefLoadHandler,
			     public CefKeyboardHandler,
			     public CefFocusHandler,
			     public CefJSDialogHandler,
			     public CefDownloadHandler,
			     public CefRenderHandler {

public:
	inline QCefOsrBrowserClient(QCefOsrWidgetInternal *widget_,
				 const std::string &headers_,
				 const std::string &script_,
				 bool allowAllPopups_)
		: widget(widget_),
		  headers(headers_),
		  script(script_),
		  allowAllPopups(allowAllPopups_)
	{
	}

	/* CefClient */
	virtual CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request, bool is_navigation,
		bool is_download, const CefString &request_initiator,
		bool &disable_default_handling) override;
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override;
	virtual CefRefPtr<CefFocusHandler> GetFocusHandler() override;
	virtual CefRefPtr<CefContextMenuHandler>
	GetContextMenuHandler() override;
	virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override;
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override;

	/* CefDisplayHandler */
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
				   const CefString &title) override;

	virtual bool OnCursorChange(CefRefPtr<CefBrowser> browser,
				    CefCursorHandle cursor,
				    cef_cursor_type_t type,
				    const CefCursorInfo& custom_cursor_info) override;

	// CefResourceRequestHandler
	virtual ReturnValue
	OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
			     CefRefPtr<CefFrame> frame,
			     CefRefPtr<CefRequest> request,
			     CefRefPtr<CefCallback> callback) override;
	/* CefRequestHandler */
	virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				    CefRefPtr<CefFrame> frame,
				    CefRefPtr<CefRequest> request,
				    bool user_gesture,
				    bool is_redirect) override;

	virtual CefRefPtr<CefDownloadHandler> GetDownloadHandler() override
	{
		return this;
	}

	virtual bool OnBeforeDownload(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefDownloadItem> download_item,
		const CefString& suggested_name,
		CefRefPtr<CefBeforeDownloadCallback> callback) override;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
	virtual void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus
#if CHROME_VERSION_BUILD >= 6367
					       ,
					       int, const CefString &
#endif
					       ) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefLoadHandler::ErrorCode errorCode,
				 const CefString &errorText,
				 const CefString &failedUrl) override;

	virtual bool OnOpenURLFromTab(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString &target_url,
		CefRequestHandler::WindowOpenDisposition target_disposition,
		bool user_gesture) override;

	/* CefLifeSpanHandler */
	virtual bool OnBeforePopup(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		const CefString &target_url, const CefString &target_frame_name,
		CefLifeSpanHandler::WindowOpenDisposition target_disposition,
		bool user_gesture, const CefPopupFeatures &popupFeatures,
		CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client,
		CefBrowserSettings &settings,
		CefRefPtr<CefDictionaryValue> &extra_info,
		bool *no_javascript_access) override;

	/* CefFocusHandler */
	virtual bool OnSetFocus(CefRefPtr<CefBrowser> browser,
				CefFocusHandler::FocusSource source) override;

	/* CefContextMenuHandler */
	virtual void
	OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
			    CefRefPtr<CefFrame> frame,
			    CefRefPtr<CefContextMenuParams> params,
			    CefRefPtr<CefMenuModel> model) override;

	virtual void
	OnAddressChange(CefRefPtr<CefBrowser> browser,
					 CefRefPtr<CefFrame> frame,
					 const CefString &url) override;
#if defined(_WIN32)
	virtual bool
	RunContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		       CefRefPtr<CefContextMenuParams> params,
		       CefRefPtr<CefMenuModel> model,
		       CefRefPtr<CefRunContextMenuCallback> callback) override;
#endif

	virtual bool OnContextMenuCommand(
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
		CefRefPtr<CefContextMenuParams> params, int command_id,
		CefContextMenuHandler::EventFlags event_flags) override;

	/* CefLoadHandler */
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
			       CefRefPtr<CefFrame> frame,
			       int httpStatusCode) override;

	/* CefKeyboardHandler */
	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				   const CefKeyEvent &event,
				   CefEventHandle os_event,
				   bool *is_keyboard_shortcut) override;

	/* CefJSDialogHandler */
	virtual bool OnJSDialog(CefRefPtr<CefBrowser> browser,
				const CefString &origin_url,
				CefJSDialogHandler::JSDialogType dialog_type,
				const CefString &message_text,
				const CefString &default_prompt_text,
				CefRefPtr<CefJSDialogCallback> callback,
				bool &suppress_message) override;

	virtual bool
	OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
				 CefRefPtr<CefFrame> frame,
				 CefProcessId source_process,
				 CefRefPtr<CefProcessMessage> message) override;

	/* CefRenderHandler */

	virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;

	virtual void OnPaint(CefRefPtr<CefBrowser> browser,
			     PaintElementType type,
			     const RectList& dirtyRects,
			     const void* buffer,
			     int width,
			     int height) override;

	virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser,
				   CefScreenInfo& screen_info) override;

	virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;
	virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;

	QCefOsrWidgetInternal* widget = nullptr;
	bool setHeaders = false;
	std::string headers;
	std::string script;
	bool allowAllPopups;

	IMPLEMENT_REFCOUNTING(QCefOsrBrowserClient);

private:
	// Handle the browser side of query routing
	CefRefPtr<CefMessageRouterBrowserSide> message_router_;
	std::unique_ptr<CefMessageRouterBrowserSide::Handler> message_handler_;
	int browser_count = 0;
};
