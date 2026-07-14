#include "browser-osr-panel-client.hpp"
#include "QCefQueryHandler.hpp"
#include <util/dstr.h>

#include <QUrl>
#include <QDesktopServices>
#include <QApplication>
#include <QMenu>
#include <QThread>
#include <QMessageBox>
#include <QInputDialog>
#include <QRegularExpression>
#include <QLabel>
#include <QClipboard>
#include <QScreen>

// Danggu Added
#include <QLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QStyle>
//

#include <obs-module.h>
#ifdef _WIN32
#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")
#endif
#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#endif

#define MENU_ITEM_DEVTOOLS MENU_ID_CUSTOM_FIRST
#define MENU_ITEM_MUTE MENU_ID_CUSTOM_FIRST + 1
#define MENU_ITEM_ZOOM_IN MENU_ID_CUSTOM_FIRST + 2
#define MENU_ITEM_ZOOM_RESET MENU_ID_CUSTOM_FIRST + 3
#define MENU_ITEM_ZOOM_OUT MENU_ID_CUSTOM_FIRST + 4
#define MENU_ITEM_COPY_URL MENU_ID_CUSTOM_FIRST + 5


/* CefClient */
CefRefPtr<CefResourceRequestHandler>
QCefOsrBrowserClient::GetResourceRequestHandler(CefRefPtr<CefBrowser>,
						CefRefPtr<CefFrame>,
						CefRefPtr<CefRequest>, bool, bool,
						const CefString &, bool &)
{
	return this;
}
CefRefPtr<CefLoadHandler> QCefOsrBrowserClient::GetLoadHandler()
{
	return this;
}

CefRefPtr<CefDisplayHandler> QCefOsrBrowserClient::GetDisplayHandler()
{
	return this;
}

CefRefPtr<CefRequestHandler> QCefOsrBrowserClient::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> QCefOsrBrowserClient::GetLifeSpanHandler()
{
	return this;
}

CefRefPtr<CefFocusHandler> QCefOsrBrowserClient::GetFocusHandler()
{
	return this;
}

CefRefPtr<CefContextMenuHandler> QCefOsrBrowserClient::GetContextMenuHandler()
{
	return this;
}

CefRefPtr<CefKeyboardHandler> QCefOsrBrowserClient::GetKeyboardHandler()
{
	return this;
}

CefRefPtr<CefJSDialogHandler> QCefOsrBrowserClient::GetJSDialogHandler()
{
	return this;
}

CefRefPtr<CefRenderHandler> QCefOsrBrowserClient::GetRenderHandler()
{
	return this;
}

/* CefDisplayHandler */
void QCefOsrBrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
				      const CefString &title)
{
	if (widget && widget->cefBrowser->IsSame(browser)) {
		std::string str_title = title;
		QString qt_title = QString::fromUtf8(str_title.c_str());
		QMetaObject::invokeMethod(widget, "titleChanged",
					  Q_ARG(QString, qt_title));
	} else { /* handle popup title */
		if (title.compare("DevTools") == 0)
			return;

#if defined(_WIN32)
		CefWindowHandle handl = browser->GetHost()->GetWindowHandle();
		std::wstring str_title = title;
		SetWindowTextW((HWND)handl, str_title.c_str());
#elif defined(__linux__)
		CefWindowHandle handl = browser->GetHost()->GetWindowHandle();
		XStoreName(cef_get_xdisplay(), handl, title.ToString().c_str());
#endif
	}
}

bool QCefOsrBrowserClient::OnCursorChange(CefRefPtr<CefBrowser> browser,
	CefCursorHandle cursor,
	cef_cursor_type_t type,
	const CefCursorInfo& custom_cursor_info)
{
	//UNUSED_PARAMETER
	(void) browser;
	(void) cursor;
	(void) type;
	(void) custom_cursor_info;
	//UNUSED_PARAMETER
	
	
	
	if (!widget)
		return false;

#if defined(_WIN32)
	QPointer<QCefOsrWidgetInternal> safeWidget = widget;

	QMetaObject::invokeMethod(widget, [=]() {
		if (!safeWidget)
			return;
		safeWidget->ApplyCursorChange(type, custom_cursor_info);
		}, Qt::QueuedConnection);
	return true;

#else
	return false;
#endif
}

cef_return_value_t QCefOsrBrowserClient::OnBeforeResourceLoad(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback)
{
	if (!setHeaders &&
		!headers.empty()) {
		CefRequest::HeaderMap headerMap;
		request->GetHeaderMap(headerMap);
		//
		nlohmann::json headerJson;
		try {
			headerJson = nlohmann::json::parse(headers);
		} catch (const nlohmann::json::parse_error &e) {
			// parse error
			blog(LOG_ERROR, "cef headers parse error... : %s",
			     e.what());
			//return;
		}

		for (auto &[key, value] : headerJson.items()) {
			if (value.is_string()) {
				headerMap.insert(std::make_pair(
					key, value.get<std::string>()));
			} else if (value.is_number_integer()) {
				headerMap.insert(std::make_pair(
					key, std::to_string(value.get<int>())));
			} else if (value.is_boolean()) {
				headerMap.insert(std::make_pair(
					key,
					std::to_string(value.get<bool>())));
			} else if (value.is_object()) {
				headerMap.insert(std::make_pair(key, value.dump()));
			} else if (value.is_null()) {
				blog(LOG_INFO, "header value is null");
			}
		}
		//
		request->SetHeaderMap(headerMap);
		setHeaders = true;
	}

	// 요청을 계속 진행하도록 RV_CONTINUE를 반환합니다.
	return RV_CONTINUE;
}
/* CefRequestHandler */
bool QCefOsrBrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
				       CefRefPtr<CefFrame> frame,
				       CefRefPtr<CefRequest> request, bool,
				       bool)
{
	if (message_router_) {
		message_router_->OnBeforeBrowse(browser, frame);
	}
	//
	std::string str_url = request->GetURL();

	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	for (size_t i = forced_popups.size(); i > 0; i--) {
		PopupWhitelistInfo &info = forced_popups[i - 1];

		if (!info.obj) {
			forced_popups.erase(forced_popups.begin() + (i - 1));
			continue;
		}

		if (astrcmpi(info.url.c_str(), str_url.c_str()) == 0) {
			/* Open tab popup URLs in user's actual browser */
			QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
			QDesktopServices::openUrl(url);
			browser->GoBack();
			return true;
		}
	}

	if (widget) {
		QString qt_url = QString::fromUtf8(str_url.c_str());
		if (qt_url.contains("app/find_security.php")) { // GL SOOP Account Link
			QDesktopServices::openUrl(qt_url);
			browser->GoBack();
		} else {
			QMetaObject::invokeMethod(widget, "urlChanged",
						  Q_ARG(QString, qt_url));
		}
	}
	return false;
}
bool QCefOsrBrowserClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefDownloadItem> download_item,
	const CefString& suggested_name,
	CefRefPtr<CefBeforeDownloadCallback> callback)
{
	callback->Continue(suggested_name, true);

	return false;
}

void QCefOsrBrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	//CefCurrentlyOn(TID_UI);
	if (!message_router_) {
		// Create the browser-side router for query handling.
		CefMessageRouterConfig config;
		message_router_ = CefMessageRouterBrowserSide::Create(config);

		// Register handlers with the router.
		message_handler_.reset(new QCefQueryHandler(widget));
		message_router_->AddHandler(message_handler_.get(), false);
	}
	++browser_count;
	//
	if (browser && browser->GetHost()) {
//		CefWindowInfo wndDevTool;
//#ifdef _WIN32
//		wndDevTool.SetAsPopup(browser->GetHost()->GetWindowHandle(),
//				      "DevTools");
//#endif
//		wndDevTool.bounds.width = 300;
//		wndDevTool.bounds.height = 500;
//		CefBrowserSettings settings2;
//		browser->GetHost()->ShowDevTools(wndDevTool, nullptr, settings2, CefPoint(0, 0));

		emit widget->cefCreateAfter();
	}
}
void QCefOsrBrowserClient::OnBeforeClose(CefRefPtr<CefBrowser>)
{
	--browser_count;
	if (message_router_ && browser_count == 0) {

		QPointer<QWidget> qwiget = widget;
		QMetaObject::invokeMethod(
			qwiget, [qwiget]() {
				if (!qwiget) return;
				static_cast<QCefOsrWidgetInternal*>(qwiget.data())->DisconnectDummyInteraction();
			},
			Qt::QueuedConnection);

		// Free the router when the last browser is closed.
		message_router_->RemoveHandler(message_handler_.get());
		message_handler_.reset();
		message_router_ = nullptr;
	}

	if (widget) {
		widget->finishCloseBrowser();
	}

}
void QCefOsrBrowserClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
						     TerminationStatus
#if CHROME_VERSION_BUILD >= 6367
						     ,
						     int, const CefString &
#endif
)
{   
	if (message_router_) {
		message_router_->OnRenderProcessTerminated(browser);
	}
}

bool QCefOsrBrowserClient::OnOpenURLFromTab(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	CefRequestHandler::WindowOpenDisposition, bool)
{
	std::string str_url = target_url;

	/* Open tab popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

void QCefOsrBrowserClient::OnLoadError(CefRefPtr<CefBrowser>,
				    CefRefPtr<CefFrame> frame,
				    CefLoadHandler::ErrorCode errorCode,
				    const CefString &,
				    const CefString &failedUrl)
{
	if (errorCode == ERR_ABORTED)
		return;

	struct dstr html;
	char *path = obs_module_file("error.html");
	char *errorPage = os_quick_read_utf8_file(path);

	dstr_init_copy(&html, errorPage);

	dstr_replace(&html, "%%ERROR_URL%%", failedUrl.ToString().c_str());

	dstr_replace(&html, "Error.Title", obs_module_text("Error.Title"));
	//dstr_replace(&html, "Error.Description",
	//	     obs_module_text("Error.Description"));
	dstr_replace(&html, "Error.Retry", obs_module_text("Error.Retry"));
	//const char *translError;
	//std::string errorKey = "ErrorCode." + errorText.ToString();
	//if (obs_module_get_string(errorKey.c_str(),
	//			  (const char **)&translError)) {
	//	dstr_replace(&html, "%%ERROR_CODE%%", translError);
	//} else {
	//	dstr_replace(&html, "%%ERROR_CODE%%",
	//		     errorText.ToString().c_str());
	//}

	frame->LoadURL(
		"data:text/html;base64," +
		CefURIEncode(CefBase64Encode(html.array, html.len), false)
			.ToString());

	dstr_free(&html);
	bfree(path);
	bfree(errorPage);
}

/* CefLifeSpanHandler */
bool QCefOsrBrowserClient::OnBeforePopup(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, const CefString &target_url,
	const CefString &, CefLifeSpanHandler::WindowOpenDisposition, bool,
	const CefPopupFeatures &, CefWindowInfo &windowInfo,
	CefRefPtr<CefClient> &, CefBrowserSettings &,
	CefRefPtr<CefDictionaryValue> &, bool *)
{
    std::string checkUrl = target_url;
    
    size_t index_check = checkUrl.find("about:blank");
    if (index_check != std::string::npos) {
	    //login
		if (widget->m_SoopLogin) {
			return true;
		}
		//balloon vod
	    return false;
    }

    if (widget) {
	    if (widget->m_SoopLogin) {
			emit widget->cefBeforePopup(
				QString::fromUtf8(target_url.ToString()));
			return true;
	    }

	    //Soop Login -> query, AccountLink -> Url Change
	    if (target_url.ToString().find(
			"app/find_id_pw.php?nExternalType=2") !=
			std::string::npos ||
		target_url.ToString().find("app/join.php?nExternalType=2") !=
			std::string::npos ||
		target_url.ToString().find("app/campaign_second_pw.php") !=
			std::string::npos ||
		target_url.ToString().find("app/campaign_pw.php") !=
			std::string::npos ||
		target_url.ToString().find("app/pop_login_block.php") !=
			std::string::npos ||
		target_url.ToString().find("app/pop_verify_self_minor_none_login.php") !=
			std::string::npos ||
		target_url.ToString().find("app/join.php") !=
			std::string::npos ||
		target_url.ToString().find("app/pop_black_info.php") !=
			std::string::npos) {
			emit widget->cefBeforePopup(
				QString::fromUtf8(target_url.ToString()));
			return true;
	    }
	    //Soop Login -> query, AccountLink -> Url Change
    }
	

	if (allowAllPopups) {
#ifdef _WIN32
		HWND hwnd = (HWND)widget->effectiveWinId();
		windowInfo.parent_window = hwnd;
#else
		UNUSED_PARAMETER(windowInfo);
#endif
		return false;
	}

	std::string str_url = target_url;

	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	for (size_t i = popup_whitelist.size(); i > 0; i--) {
		PopupWhitelistInfo &info = popup_whitelist[i - 1];

		if (!info.obj) {
			popup_whitelist.erase(popup_whitelist.begin() +
					      (i - 1));
			continue;
		}

		if (astrcmpi(info.url.c_str(), str_url.c_str()) == 0) {
#ifdef _WIN32
			HWND hwnd = (HWND)widget->effectiveWinId();
			windowInfo.parent_window = hwnd;
#endif
			return false;
		}
	}

	/* Open popup URLs in user's actual browser */
	QUrl url = QUrl(str_url.c_str(), QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
	return true;
}

bool QCefOsrBrowserClient::OnSetFocus(CefRefPtr<CefBrowser>,
				   CefFocusHandler::FocusSource source)
{
	/* Don't steal focus when the webpage navigates. This is especially
	   obvious on startup when the user has many browser docks defined,
	   as each one will steal focus one by one, resulting in poor UX.
	 */
	switch (source) {
	case FOCUS_SOURCE_NAVIGATION:
		return true;
	default:
		return false;
	}
}

void QCefOsrBrowserClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
					    CefRefPtr<CefFrame>,
					    CefRefPtr<CefContextMenuParams>,
					    CefRefPtr<CefMenuModel> model)
{
	UNUSED_PARAMETER(browser);

#ifndef _DEBUG
	model->Clear();
#else
	if (model->IsVisible(MENU_ID_BACK) &&
	    (!model->IsVisible(MENU_ID_RELOAD) &&
	     !model->IsVisible(MENU_ID_RELOAD_NOCACHE))) {
		model->InsertItemAt(
			2, MENU_ID_RELOAD_NOCACHE,
			QObject::tr("RefreshBrowser").toUtf8().constData());
	}
	if (model->IsVisible(MENU_ID_PRINT)) {
		model->Remove(MENU_ID_PRINT);
	}
	if (model->IsVisible(MENU_ID_VIEW_SOURCE)) {
		model->Remove(MENU_ID_VIEW_SOURCE);
	}
	model->AddItem(MENU_ITEM_ZOOM_IN, obs_module_text("Zoom.In"));
	if (browser->GetHost()->GetZoomLevel() != 0) {
		model->AddItem(MENU_ITEM_ZOOM_RESET,
			       obs_module_text("Zoom.Reset"));
	}
	model->AddItem(MENU_ITEM_ZOOM_OUT, obs_module_text("Zoom.Out"));
	model->AddSeparator();
	model->InsertItemAt(model->GetCount(), MENU_ITEM_COPY_URL,
			    obs_module_text("CopyUrl"));
	model->InsertItemAt(model->GetCount(), MENU_ITEM_DEVTOOLS,
			    obs_module_text("Inspect"));
	model->InsertCheckItemAt(model->GetCount(), MENU_ITEM_MUTE,
				 QObject::tr("Mute").toUtf8().constData());
	model->SetChecked(MENU_ITEM_MUTE, browser->GetHost()->IsAudioMuted());
#endif // _DEBUG

}

void QCefOsrBrowserClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
					   CefRefPtr<CefFrame> frame,
					   const CefString &url)
{
	if (widget && !widget->m_LinkSns) {
		return;
	}
	size_t nTwitch = url.ToString().find("twitch.tv");
	size_t nNaver = url.ToString().find("naver.com");
	size_t nApple = url.ToString().find("apple.com");
	size_t nKakao = url.ToString().find("kakao.com");
	size_t nX = url.ToString().find("api.twitter.com/oauth");
	size_t nFacebook = url.ToString().find("facebook.com/login.php");
	size_t nGoogle = url.ToString().find("accounts.google.com/v3/signin");
	size_t nSignUp = url.ToString().find("/app/join.php");

	HWND hWnd = browser->GetHost()->GetWindowHandle();

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int width = 0;
	int height = 0;

	if (nTwitch != std::string::npos) {
		width = 490;
		height = 435;
	} else if (nNaver != std::string::npos) {
		width = 560;
		height = 760;
	} else if (nKakao != std::string::npos) {
		width = 560;
		height = 840;
	} else if (nX != std::string::npos) {
		width = 885;
		height = 900;
	} else if (nGoogle != std::string::npos) {
		width = 615;
		height = 700;
	} else if (nApple != std::string::npos) {
		width = 450;
		height = 715;
	} else if (nFacebook != std::string::npos) {
		width = 560;
		height = 400;
	} else if (nSignUp != std::string::npos) {
		std::string str_url = url;
		QUrl urltoNavigate = QUrl(str_url.c_str(),
				QUrl::TolerantMode);
		QDesktopServices::openUrl(urltoNavigate);
		emit widget->cefBeforePopup(
			QString::fromUtf8("close_window"));
		browser->GoBack();
	} else {
		return;
	}

	HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	UINT dpiX = 96, dpiY = 96;
	GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
	double scale = dpiX / 96.0;

	width = static_cast<int>(width * scale);
	height = static_cast<int>(height * scale);
	int x = (screenWidth - width) / 2;
	int y = (screenHeight - height) / 2;

	MoveWindow(hWnd, x, y, width, height, true);

	return;
}

#if defined(_WIN32)
bool QCefOsrBrowserClient::RunContextMenu(
	CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
	CefRefPtr<CefContextMenuParams>, CefRefPtr<CefMenuModel> model,
	CefRefPtr<CefRunContextMenuCallback> callback)
{
	std::vector<std::tuple<std::string, int, bool, int, bool>> menu_items;
	menu_items.reserve(model->GetCount());
	for (int i = 0; i < model->GetCount(); i++) {
		menu_items.push_back(
			{model->GetLabelAt(i), model->GetCommandIdAt(i),
			 model->IsEnabledAt(i), model->GetTypeAt(i),
			 model->IsCheckedAt(i)});
	}

	QMetaObject::invokeMethod(
		QCoreApplication::instance()->thread(),
		[menu_items, callback]() {
			QMenu contextMenu;
			std::string name;
			int command_id;
			bool enabled;
			int type_id;
			bool check;

			for (const std::tuple<std::string, int, bool, int, bool>
				     &menu_item : menu_items) {
				std::tie(name, command_id, enabled, type_id,
					 check) = menu_item;
				switch (type_id) {
				case MENUITEMTYPE_CHECK:
				case MENUITEMTYPE_COMMAND: {
					QAction *item =
						new QAction(name.c_str());
					item->setEnabled(enabled);
					if (type_id == MENUITEMTYPE_CHECK) {
						item->setCheckable(true);
						item->setChecked(check);
					}
					item->setProperty("cmd_id", command_id);
					contextMenu.addAction(item);
				} break;
				case MENUITEMTYPE_SEPARATOR:
					contextMenu.addSeparator();
					break;
				}
			}

			QAction *action = contextMenu.exec(QCursor::pos());
			if (action) {
				QVariant cmdId = action->property("cmd_id");
				callback.get()->Continue(cmdId.toInt(),
							 EVENTFLAG_NONE);
			} else {
				callback.get()->Cancel();
			}
		});
	return true;
}
#endif

bool QCefOsrBrowserClient::OnContextMenuCommand(
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>,
	CefRefPtr<CefContextMenuParams> params, int command_id,
	CefContextMenuHandler::EventFlags)
{
	if (command_id < MENU_ID_CUSTOM_FIRST)
		return false;
	CefRefPtr<CefBrowserHost> host = browser->GetHost();
	CefWindowInfo windowInfo;
	QPoint pos;
	QString title;
	switch (command_id) {
	case MENU_ITEM_DEVTOOLS:
#if defined(_WIN32)
		title = QString(obs_module_text("DevTools"))
				.arg(widget->parentWidget()->windowTitle());
		windowInfo.SetAsPopup(host->GetWindowHandle(),
				      title.toUtf8().constData());
#endif
		pos = widget->mapToGlobal(QPoint(0, 0));
		windowInfo.bounds.x = pos.x();
		windowInfo.bounds.y = pos.y() + 30;
		windowInfo.bounds.width = 900;
		windowInfo.bounds.height = 700;
		host->ShowDevTools(
			windowInfo, host->GetClient(), CefBrowserSettings(),
			{params.get()->GetXCoord(), params.get()->GetYCoord()});
		return true;
	case MENU_ITEM_MUTE:
		host->SetAudioMuted(!host->IsAudioMuted());
		return true;
	case MENU_ITEM_ZOOM_IN:
		widget->zoomPage(1);
		return true;
	case MENU_ITEM_ZOOM_RESET:
		widget->zoomPage(0);
		return true;
	case MENU_ITEM_ZOOM_OUT:
		widget->zoomPage(-1);
		return true;
	case MENU_ITEM_COPY_URL:
		std::string url = browser->GetMainFrame()->GetURL().ToString();
		auto saveClipboard = [url]() {
			QClipboard *clipboard = QApplication::clipboard();

			clipboard->setText(url.c_str(), QClipboard::Clipboard);

			if (clipboard->supportsSelection()) {
				clipboard->setText(url.c_str(),
						   QClipboard::Selection);
			}
		};
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(), saveClipboard);
		return true;
		break;
	}
	return false;
}

void QCefOsrBrowserClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
				  CefRefPtr<CefFrame> frame, int)
{
	if (!frame->IsMain())
		return;

	browser->GetHost()->SetFocus(true);

	if (widget && !widget->script.empty())
		frame->ExecuteJavaScript(widget->script, CefString(), 0);
	else if (!script.empty())
		frame->ExecuteJavaScript(script, CefString(), 0);

	std::string script2 = "window.close = () => ";
	script2 += "console.log(";
	script2 += "'OBS browser docks cannot be closed using JavaScript.'";
	script2 += ");";
	frame->ExecuteJavaScript(script2, "", 0);

	if (widget) {
		QMetaObject::invokeMethod(widget,
			"SetFocusDummyInteraction", Qt::QueuedConnection);

		emit widget->cefLoadEnd();
	}

}

bool QCefOsrBrowserClient::OnJSDialog(CefRefPtr<CefBrowser>, const CefString &,
				   CefJSDialogHandler::JSDialogType dialog_type,
				   const CefString &message_text,
				   const CefString &default_prompt_text,
				   CefRefPtr<CefJSDialogCallback> callback,
				   bool &)
{
	//QString parentTitle = widget->parentWidget()->windowTitle();
	std::string default_value = default_prompt_text;
	QString msg_raw(message_text.ToString().c_str());
	// Replace <br> with standard newline as we will render in plaintext
	msg_raw.replace(QRegularExpression("<br\\s{0,1}\\/{0,1}>"), "\n");
	/*QString submsg =
		QString(obs_module_text("Dialog.ReceivedFrom")).arg(parentTitle);*/
	//QString msg = QString("%1\n\n\n%2").arg(msg_raw).arg(submsg);
	QString msg = msg_raw;

	//somsool Soop Login Alert
	if (widget->m_SoopLogin && dialog_type != JSDIALOGTYPE_CONFIRM)
	{
		emit widget->cefMessageBoxMessage(msg_raw);
		return true;
	}

	if (dialog_type == JSDIALOGTYPE_PROMPT) {
		auto msgbox = [this, msg, default_value,  callback]() {
			QInputDialog *dlg = new QInputDialog(widget);
			dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
			dlg->setWindowFlag(Qt::WindowContextHelpButtonHint,
					   false);
			std::stringstream title;
			title << obs_module_text("Dialog.Prompt") << ": "
			      << obs_module_text("Dialog.BrowserDock");
			dlg->setWindowTitle(title.str().c_str());
			if (!default_value.empty())
				dlg->setTextValue(default_value.c_str());

			auto finished = [callback, dlg](int result) {
				callback.get()->Continue(
					result == QDialog::Accepted,
					dlg->textValue().toUtf8().constData());
			};

			QWidget::connect(dlg, &QInputDialog::finished,
					 finished);
			dlg->open();
			if (QLabel *lbl = dlg->findChild<QLabel *>()) {
				// Force plaintext manually
				lbl->setTextFormat(Qt::PlainText);
			}
			dlg->setLabelText(msg);
		};
		QMetaObject::invokeMethod(
			QCoreApplication::instance()->thread(), msgbox);
		return true;
	}
	auto msgbox = [this, msg, dialog_type, callback]() {
		QMessageBox *dlg = new QMessageBox(widget);
		dlg->setStandardButtons(QMessageBox::Ok);
		dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
		dlg->setTextFormat(Qt::PlainText);
		dlg->setText(msg);

		std::stringstream title;
		switch (dialog_type) {
		case JSDIALOGTYPE_CONFIRM:
			title << obs_module_text("Dialog.Confirm");
			//dlg->setIcon(QMessageBox::Question);
			dlg->addButton(QMessageBox::Cancel);
			break;
		case JSDIALOGTYPE_ALERT:
		default:
			title << obs_module_text("Dialog.Alert");
			//dlg->setIcon(QMessageBox::Information);
			break;
		}
		//title << ": " << obs_module_text("Dialog.BrowserDock");
		dlg->setWindowTitle(title.str().c_str());

		auto finished = [callback](int result) {
			callback.get()->Continue(result == QMessageBox::Ok, "");
		};

		QWidget::connect(dlg, &QMessageBox::finished, finished);

		// Danggu Added
		dlg->setWindowFlag(Qt::FramelessWindowHint, true);

		QLayout *msgLayout = dlg->layout();
		if (msgLayout)
			msgLayout->setContentsMargins(QMargins(20, 20, 20, 20));
		
		QDialogButtonBox *msgButtonBox = dlg->findChild<QDialogButtonBox *>();
		if (msgButtonBox) {
			msgButtonBox->setCenterButtons(true);

			// Set Button Style
			QPushButton * okButton = msgButtonBox->button(QDialogButtonBox::Ok);
			if (okButton)
			{
				okButton->setFixedSize(QSize(100, 40));
				okButton->setProperty("pushButtonTheme", "type2");
				okButton->style()->unpolish(okButton);
				okButton->style()->polish(okButton);
			}

			QPushButton * cancelButton = msgButtonBox->button(QDialogButtonBox::Cancel);
			if (cancelButton)
			{
				cancelButton->setFixedSize(QSize(100, 40));
				cancelButton->setProperty("pushButtonTheme", "type3");
				cancelButton->style()->unpolish(cancelButton);
				cancelButton->style()->polish(cancelButton);
			}
			//
		}

		dlg->exec();
		//dlg->open();
		//
	};
	QMetaObject::invokeMethod(QCoreApplication::instance()->thread(),
				  msgbox);
	return true;
}

bool QCefOsrBrowserClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
						 CefRefPtr<CefFrame> frame,
						 CefProcessId source_process,
						 CefRefPtr<CefProcessMessage> message)
{

	if (!widget) {
		return false;
	}
	//
	const std::string &name = message->GetName();
	if ("cefQueryMsg" == name) {
		//CefCurrentlyOn(TID_UI);
		if (!message_router_)
			return false;
		return message_router_->OnProcessMessageReceived(browser, frame, source_process, message);
	}

	return true;
}

void QCefOsrBrowserClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	//UNUSED_PARAMETER
	(void) browser;
	//UNUSED_PARAMETER
	
	if (!widget) {
		rect.width = rect.height = 1;
		return;
	}

	cef_rect_t rect_ = widget->GetViewSize();
	rect.x = rect.y = 0;

	rect.width = rect_.width;
	if (rect.width <= 0)
		rect.width = 1;

	rect.height = rect_.height;
	if (rect.height <= 0)
		rect.height = 1;
}

void QCefOsrBrowserClient::OnPaint(CefRefPtr<CefBrowser> browser,
	PaintElementType type,
	const RectList& dirtyRects,
	const void* buffer,
	int width,
	int height)
{
	//UNUSED_PARAMETER
	(void) browser;
	(void) dirtyRects;
	//UNUSED_PARAMETER
	
	
	
	
	if (!widget || !buffer || width == 0 || height == 0)
		return;

	if (widget) {
		widget->UpdateBuffer(type, buffer, width, height);
	}

}

void QCefOsrBrowserClient::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect)
{
	//UNUSED_PARAMETER
	(void) browser;
	//UNUSED_PARAMETER
	
	
	
	if (widget) {
		widget->SetPopupRect(rect);
	}
}

bool QCefOsrBrowserClient::GetScreenInfo(CefRefPtr<CefBrowser> browser,
					 CefScreenInfo& screen_info)
{
	//UNUSED_PARAMETER
	(void) browser;
	//UNUSED_PARAMETER
	
	
	
	if (!widget)
		return false;

	const qreal scale = widget->devicePixelRatioF();

	QScreen* screen = widget->screen();

	screen_info.device_scale_factor = scale;
	screen_info.depth = screen ? screen->depth() : 24;
	screen_info.is_monochrome = false;
	CefRect rect;
	rect.x = 0;
	rect.y = 0;
	rect.width = static_cast<int>(widget->width() * scale);
	rect.height = static_cast<int>(widget->height() * scale);
	screen_info.available_rect = rect;

	return true;
}

void QCefOsrBrowserClient::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show)
{
	if (widget) {
		if (!show) {
			CefRect rc;
			rc.Set(0, 0, 0, 0);
			widget->SetPopupRect(rc);
			browser->GetHost()->Invalidate(PET_VIEW);
		}
	}
}

bool QCefOsrBrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
				      const CefKeyEvent &event, CefEventHandle,
				      bool *)
{
	if (event.type != KEYEVENT_RAWKEYDOWN)
		return false;

	if (event.windows_key_code == 'R' &&
#ifdef __APPLE__
	    (event.modifiers & EVENTFLAG_COMMAND_DOWN) != 0) {
#else
	    (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
#endif
		browser->ReloadIgnoreCache();
		return true;
	} else if ((event.windows_key_code == 189 ||
		    event.windows_key_code == 109) &&
		   (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
		// Zoom out
		return widget->zoomPage(-1);
	} else if ((event.windows_key_code == 187 ||
		    event.windows_key_code == 107) &&
		   (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
		// Zoom in
		return widget->zoomPage(1);
	} else if ((event.windows_key_code == 48 ||
		    event.windows_key_code == 96) &&
		   (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0) {
		// Reset zoom
		return widget->zoomPage(0);
	}
	return false;
}
