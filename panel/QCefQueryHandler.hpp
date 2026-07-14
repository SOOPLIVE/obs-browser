#pragma once

#include "cef-headers.hpp"
#include "include/wrapper/cef_message_router.h"

#include <string>
#include <mutex>

////////////////////////////////////////////////////////////////////////////////////////////////////
class QCefWidget;
//
class QCefQueryHandler : public CefBaseRefCounted,
			 public CefMessageRouterBrowserSide::Handler {
public:
	 QCefQueryHandler(QCefWidget* widget)
		: cefWidget(widget)
	{}
	~QCefQueryHandler() {}

	// Called due to cefQuery execution in html.
	virtual bool OnQuery(CefRefPtr<CefBrowser> browser,
			     CefRefPtr<CefFrame> frame,
			     int64_t query_id,
			     const CefString &request,
			     bool persistent,
			     CefRefPtr<Callback> callback) override;

	virtual void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
				     CefRefPtr<CefFrame> frame,
				     int64_t query_id) override;

	bool Response(int64_t query, bool success, const CefString &response, int error);

private:
	QCefWidget* cefWidget = nullptr;
	std::map<int64_t, CefRefPtr<Callback>> callbackMap;
	std::mutex callbackLock;

private:
	IMPLEMENT_REFCOUNTING(QCefQueryHandler);
};
