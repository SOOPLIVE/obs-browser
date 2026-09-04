
#include "QCefQueryHandler.hpp"

#include "QCefQuery.hpp"
#include "browser-panel.hpp"

#include <qstring.h>


int QCefQuery::typeid_ = qRegisterMetaType<QCefQuery>("QCefQuery");
//
bool QCefQueryHandler::OnQuery(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
			       int64_t query_id,
			       const CefString &request,
			       bool,
			       CefRefPtr<Callback> callback)
{
	return false;
}

void QCefQueryHandler::OnQueryCanceled(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
				       int64_t query_id)
{
}

bool QCefQueryHandler::Response(int64_t query,
				bool success,
				const CefString &response,
				int error)
{
	CefRefPtr<Callback> cb = nullptr;
	callbackLock.lock();
	auto itr = callbackMap.find(query);
	if(itr != callbackMap.end()) {
		cb = itr->second;
		callbackMap.erase(query);
	}
	callbackLock.unlock();

	if(!cb)
		return false;

	if(success)
		cb->Success(response);
	else
		cb->Failure(error, response);
	return true;
}
