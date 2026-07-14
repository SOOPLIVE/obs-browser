#include "browser-panel-internal.hpp"
#include "browser-osr-panel-client.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"
#include "browser-app.hpp"

#include <QWindow>
#include <QApplication>
#include <QPainter>
#include <QMouseEvent>

#ifdef ENABLE_BROWSER_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

#ifdef __APPLE__
#include <objc/objc.h>
#endif

#include <obs-module.h>
#include <util/threading.h>
#include <util/base.h>
#include <thread>
#include <cmath>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <windowsx.h> 
#pragma comment(lib, "Imm32.lib")
#endif

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

std::mutex popup_whitelist_mutex;
std::vector<PopupWhitelistInfo> popup_whitelist;
std::vector<PopupWhitelistInfo> forced_popups;

static int zoomLvls[] = {25, 33, 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400};

#ifdef _WIN32
/* ------------------------------------------------------------------------- */

int IsKeyDown2(int key)
{
	return ((GetAsyncKeyState(key) & 0x8000) != 0);
}

int GetCefKeyboardModifiers2(WPARAM wparam, LPARAM lparam)
{

	int modifiers = 0;
	if (IsKeyDown2(VK_SHIFT))
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (IsKeyDown2(VK_CONTROL))
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (IsKeyDown2(VK_MENU))
		modifiers |= EVENTFLAG_ALT_DOWN;

	// Low bit set from GetKeyState indicates "toggled".
	if (::GetKeyState(VK_NUMLOCK) & 1)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (::GetKeyState(VK_CAPITAL) & 1)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;

	switch (wparam)
	{
	case VK_RETURN:
		if ((lparam >> 16) & KF_EXTENDED)
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_INSERT:
	case VK_DELETE:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
		if (!((lparam >> 16) & KF_EXTENDED))
			modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_NUMLOCK:
	case VK_NUMPAD0:
	case VK_NUMPAD1:
	case VK_NUMPAD2:
	case VK_NUMPAD3:
	case VK_NUMPAD4:
	case VK_NUMPAD5:
	case VK_NUMPAD6:
	case VK_NUMPAD7:
	case VK_NUMPAD8:
	case VK_NUMPAD9:
	case VK_DIVIDE:
	case VK_MULTIPLY:
	case VK_SUBTRACT:
	case VK_ADD:
	case VK_DECIMAL:
	case VK_CLEAR:
		modifiers |= EVENTFLAG_IS_KEY_PAD;
		break;
	case VK_SHIFT:
		if (IsKeyDown2(VK_LSHIFT))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown2(VK_RSHIFT))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_CONTROL:
		if (IsKeyDown2(VK_LCONTROL))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown2(VK_RCONTROL))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_MENU:
		if (IsKeyDown2(VK_LMENU))
			modifiers |= EVENTFLAG_IS_LEFT;
		else if (IsKeyDown2(VK_RMENU))
			modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	case VK_LWIN:
		modifiers |= EVENTFLAG_IS_LEFT;
		break;
	case VK_RWIN:
		modifiers |= EVENTFLAG_IS_RIGHT;
		break;
	}
	return modifiers;
}
#endif

class CookieCheck : public CefCookieVisitor {
public:
	QCefCookieManager::cookie_exists_cb callback;
	std::string target;
	bool cookie_found = false;

	inline CookieCheck(QCefCookieManager::cookie_exists_cb callback_, const std::string target_)
		: callback(callback_),
		  target(target_)
	{
	}

	virtual ~CookieCheck() { callback(cookie_found); }

	virtual bool Visit(const CefCookie &cookie, int, int, bool &) override
	{
		CefString cef_name = cookie.name.str;
		std::string name = cef_name;

		if (name == target) {
			cookie_found = true;
			return false;
		}
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieCheck);
};

struct QCefCookieManagerInternal : QCefCookieManager {
	CefRefPtr<CefCookieManager> cm;
	CefRefPtr<CefRequestContext> rc;

	QCefCookieManagerInternal(const std::string &storage_path, bool persist_session_cookies)
	{
		if (os_event_try(cef_started_event) != 0)
			throw "Browser thread not initialized";

		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		if (os_mkdirs(rpath.Get()) == MKDIR_ERROR)
			throw "Failed to create cookie directory";

		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());
		/*
		CefRequestContextSettings settings;
#if CHROME_VERSION_BUILD <= 6533
		settings.persist_user_preferences = 1;
#endif
		CefString(&settings.cache_path) = path.Get();
		rc = CefRequestContext::CreateContext(settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);
		*/

		cm = CefCookieManager::GetGlobalManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
	}

	void StringReplace(std::string& strText, const char* pszBefore, const char* pszAfter)
	{
		std::string::size_type pos = strText.find(pszBefore);
		size_t nBeforeLen = strlen(pszBefore);
		size_t nAfterLen = strlen(pszAfter);

		while (pos != std::string::npos) {
			strText.replace(pos, nBeforeLen, pszAfter);
			pos = strText.find(pszBefore, pos + nAfterLen);
		}
	}

	void Tokenize(const char* pszStr, std::vector<std::string>& tokens, const char chDelimiters)
	{
		std::string strTmp(pszStr);
		char szTmp1[3] = { chDelimiters, chDelimiters, '\0' };
		char szTmp2[4] = { chDelimiters, '0', chDelimiters, '\0' };
		StringReplace(strTmp, szTmp1, szTmp2);

		std::string::size_type lastPos = strTmp.find_first_not_of(chDelimiters, 0);

		std::string::size_type pos = strTmp.find_first_of(chDelimiters, lastPos);

		std::string strFound;
		while (std::string::npos != pos ||
			std::string::npos != lastPos)
		{
			strFound = strTmp.substr(lastPos, pos - lastPos);
			tokens.push_back(strFound);
			lastPos = strTmp.find_first_not_of(chDelimiters, pos);
			pos = strTmp.find_first_of(chDelimiters, lastPos);
		}
	}

	void setCookie(std::string url, std::string name, std::string value)
	{
		CefCookie cookie;

		CefString(&cookie.name) = name;
		CefString(&cookie.value) = value;
		CefString(&cookie.path).FromASCII("/");

		size_t pos = url.find("://");
		std::string domain = (pos != std::string::npos) ? url.substr(pos + 3) : url;
		size_t slashPos = domain.find('/');
		if (slashPos != std::string::npos) {
			domain = domain.substr(0, slashPos);
		}
		size_t lastDot = domain.rfind('.');
		if (lastDot != std::string::npos) {
			size_t secondLastDot = domain.rfind('.', lastDot - 1);
			if (secondLastDot != std::string::npos) {
				size_t thirdLastDot = domain.rfind('.', secondLastDot - 1);
				if (thirdLastDot != std::string::npos) {
					domain = domain.substr(thirdLastDot + 1);
				} else {
					domain = domain.substr(secondLastDot + 1);
				}
			}
		}
		CefString(&cookie.domain).FromASCII(domain.c_str());

		CefRefPtr<CefSetCookieCallback> back = nullptr;

		if (!!cm) {
			cm->SetCookie(url, cookie, back);
		}
	}

	virtual void SetCookies(const std::string& url,
				const std::string& cookies) override
	{
		std::vector<std::string> cookieTokens;
		Tokenize(cookies.c_str(), cookieTokens, ';');

		for (size_t i = 0; i < cookieTokens.size(); i++) {
			std::vector<std::string> cookieTokens2;
			Tokenize(cookieTokens[i].c_str(), cookieTokens2, '=');
			const char* pszName = cookieTokens2[0].c_str();
			if (cookieTokens2.size() < 2) {
				cookieTokens2.clear();
				continue;
			}
			const char* pszValue = cookieTokens2[1].c_str();
			setCookie(url, pszName, pszValue);
		}
	}

	virtual bool DeleteCookies(const std::string &url,
				   const std::string &name) override
	{
		return !!cm ? cm->DeleteCookies(url, name, nullptr) : false;
	}

	virtual bool SetStoragePath(const std::string &storage_path, bool persist_session_cookies) override
	{
		BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
		BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

		CefRequestContextSettings settings;
#if CHROME_VERSION_BUILD <= 6533
		settings.persist_user_preferences = 1;
#endif
		CefString(&settings.cache_path) = storage_path;
		rc = CefRequestContext::CreateContext(settings, CefRefPtr<CefRequestContextHandler>());
		if (rc)
			cm = rc->GetCookieManager(nullptr);

		UNUSED_PARAMETER(persist_session_cookies);
		return true;
	}

	virtual bool FlushStore() override { return !!cm ? cm->FlushStore(nullptr) : false; }

	virtual void CheckForCookie(const std::string &site, const std::string &cookie,
				    cookie_exists_cb callback) override
	{
		if (!cm)
			return;

		CefRefPtr<CookieCheck> c = new CookieCheck(callback, cookie);
		cm->VisitUrlCookies(site, false, c);
	}

	virtual bool SetCookie(const std::string &url, const std::string &name,
			       const std::string &value) override
	{
		CefCookie cefCookie;
		CefString(&cefCookie.name).FromString(name);
		CefString(&cefCookie.value).FromString(value);
		CefString(&cefCookie.path).FromASCII("/");
		size_t pos = url.find("://");
		std::string domain =
			(pos != std::string::npos) ? url.substr(pos + 3) : url;
		size_t slashPos = domain.find('/');
		if (slashPos != std::string::npos) {
			domain = domain.substr(0, slashPos);
		}
		size_t lastDot = domain.rfind('.');
		if (lastDot != std::string::npos) {
			size_t secondLastDot = domain.rfind('.', lastDot - 1);
			if (secondLastDot != std::string::npos) {
				size_t thirdLastDot =
					domain.rfind('.', secondLastDot - 1);
				if (thirdLastDot != std::string::npos) {
					domain =
						domain.substr(thirdLastDot + 1);
				} else {
					domain = domain.substr(secondLastDot +
							       1);
				}
			}
		}
		CefString(&cefCookie.domain).FromASCII(domain.c_str());
		return !!cm ? cm->SetCookie(url.c_str(), cefCookie, nullptr)
			    : false;
	}
};

/* ------------------------------------------------------------------------- */
QCefWidgetInternal::QCefWidgetInternal(QWidget *parent, const std::string &url_,
				       CefRefPtr<CefRequestContext> rqc_,
				       const std::string &headers_)
	: QCefWidget(parent), url(url_), rqc(rqc_), headers(headers_)
{

	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
#ifdef __APPLE__
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_PaintOnScreen);
#endif
	setAttribute(Qt::WA_NativeWindow);

	setFocusPolicy(Qt::ClickFocus);

#ifndef __APPLE__
	window = new QWindow();
	window->setFlags(Qt::FramelessWindowHint);
#endif
}

QCefWidgetInternal::~QCefWidgetInternal()
{
	closeBrowser();
}

void QCefWidgetInternal::closeBrowser()
{
	CefRefPtr<CefBrowser> browser = cefBrowser;
	if (!!browser) {
		auto destroyBrowser = [=](CefRefPtr<CefBrowser> cefBrowser) {
			CefRefPtr<CefClient> client = cefBrowser->GetHost()->GetClient();
			QCefOsrBrowserClient *bc = reinterpret_cast<QCefOsrBrowserClient*>(client.get());

			cefBrowser->GetHost()->CloseBrowser(true);

#if CHROME_VERSION_BUILD >= 6533
			QEventLoop loop;

			connect(this, &QCefWidgetInternal::readyToClose, &loop, &QEventLoop::quit);

			QTimer::singleShot(1000, &loop, &QEventLoop::quit);

			loop.exec();
#endif
			if (bc) {
				bc->widget = nullptr;
			}
		};

		/* So you're probably wondering what's going on here.  If you
		 * call CefBrowserHost::CloseBrowser, and it fails to unload
		 * the web page *before* WM_NCDESTROY is called on the browser
		 * HWND, it will call an internal CEF function
		 * CefBrowserPlatformDelegateNativeWin::CloseHostWindow, which
		 * will attempt to close the browser's main window itself.
		 * Problem is, this closes the root window containing the
		 * browser's HWND rather than the browser's specific HWND for
		 * whatever mysterious reason.  If the browser is in a dock
		 * widget, then the window it closes is, unfortunately, the
		 * main program's window, causing the entire program to shut
		 * down.
		 *
		 * So, instead, before closing the browser, we need to decouple
		 * the browser from the widget.  To do this, we hide it, then
		 * remove its parent. */
#ifdef _WIN32
		HWND hwnd = (HWND)cefBrowser->GetHost()->GetWindowHandle();
		if (hwnd) {
			ShowWindow(hwnd, SW_HIDE);
			SetParent(hwnd, nullptr);
		}
#elif __APPLE__
		// felt hacky, might delete later
		void *view = (id)cefBrowser->GetHost()->GetWindowHandle();
		if (*((bool *)view))
			((void (*)(id, SEL))objc_msgSend)((id)view, sel_getUid("removeFromSuperview"));
#endif

		destroyBrowser(browser);
		browser = nullptr;
		cefBrowser = nullptr;
	}
}

#ifdef __linux__
static bool XWindowHasAtom(Display *display, Window w, Atom a)
{
	Atom type;
	int format;
	unsigned long nItems;
	unsigned long bytesAfter;
	unsigned char *data = NULL;

	if (XGetWindowProperty(display, w, a, 0, LONG_MAX, False, AnyPropertyType, &type, &format, &nItems, &bytesAfter,
			       &data) != Success)
		return false;

	if (data)
		XFree(data);

	return type != None;
}

/* On Linux / X11, CEF sets the XdndProxy of the toplevel window
 * it's attached to, so that it can read drag events. When this
 * toplevel happens to be OBS Studio's main window (e.g. when a
 * browser panel is docked into to the main window), setting the
 * XdndProxy atom ends up breaking DnD of sources and scenes. Thus,
 * we have to manually unset this atom.
 */
void QCefWidgetInternal::unsetToplevelXdndProxy()
{
	if (!cefBrowser)
		return;

	CefWindowHandle browserHandle = cefBrowser->GetHost()->GetWindowHandle();
	Display *xDisplay = cef_get_xdisplay();
	Window toplevel, root, parent, *children;
	unsigned int nChildren;
	bool found = false;

	toplevel = browserHandle;

	// Find the toplevel
	Atom netWmPidAtom = XInternAtom(xDisplay, "_NET_WM_PID", False);
	do {
		if (XQueryTree(xDisplay, toplevel, &root, &parent, &children, &nChildren) == 0)
			return;

		if (children)
			XFree(children);

		if (root == parent || !XWindowHasAtom(xDisplay, parent, netWmPidAtom)) {
			found = true;
			break;
		}
		toplevel = parent;
	} while (true);

	if (!found)
		return;

	// Check if the XdndProxy property is set
	Atom xDndProxyAtom = XInternAtom(xDisplay, "XdndProxy", False);
	if (needsDeleteXdndProxy && !XWindowHasAtom(xDisplay, toplevel, xDndProxyAtom)) {
		QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
		return;
	}

	XDeleteProperty(xDisplay, toplevel, xDndProxyAtom);
	needsDeleteXdndProxy = false;
}
#endif

void QCefWidgetInternal::Init(unsigned background_color_alpha)
{
#ifndef __APPLE__
	WId handle = window->winId();
	QSize size = this->size();
	size *= devicePixelRatioF();
	bool success = QueueCEFTask(
		[this, handle, size]()
#else
	WId handle = winId();
	bool success = QueueCEFTask(
		[this, handle]()
#endif
		{
			CefWindowInfo windowInfo;

			/* Make sure Init isn't called more than once. */
			if (cefBrowser)
				return;

#ifdef __APPLE__
			QSize size = this->size();
#endif

#if CHROME_VERSION_BUILD >= 6533
			windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
#endif

#if CHROME_VERSION_BUILD < 4430
#ifdef __APPLE__
			windowInfo.SetAsChild((CefWindowHandle)handle, 0, 0, size.width(), size.height());
#else
#ifdef _WIN32
			RECT rc = {0, 0, size.width(), size.height()};
#else
			CefRect rc = {0, 0, size.width(), size.height()};
#endif
			windowInfo.SetAsChild((CefWindowHandle)handle, rc);
#endif
#else
			windowInfo.SetAsChild((CefWindowHandle)handle, CefRect(0, 0, size.width(), size.height()));
#endif

			CefRefPtr<QCefBrowserClient> browserClient =
				new QCefBrowserClient(this, headers, script, allowAllPopups_);

			CefBrowserSettings cefBrowserSettings;
			cefBrowser = CefBrowserHost::CreateBrowserSync(windowInfo, browserClient, url,
								       cefBrowserSettings,
								       CefRefPtr<CefDictionaryValue>(), rqc);

#ifdef __linux__
			QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
#endif
		});

	if (success) {
		timer.stop();
#ifndef __APPLE__
		if (!container) {
			container = QWidget::createWindowContainer(window, this);
			container->show();
		}

		Resize();
#endif
	}
}

void QCefWidgetInternal::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
#ifndef __APPLE__
	Resize();
}

void QCefWidgetInternal::Resize()
{
	QSize size = this->size() * devicePixelRatioF();

	bool success = QueueCEFTask([this, size]() {
		if (!cefBrowser)
			return;

		CefWindowHandle handle = cefBrowser->GetHost()->GetWindowHandle();

		if (!handle)
			return;

#ifdef _WIN32
		SetWindowPos((HWND)handle, nullptr, 0, 0, size.width(), size.height(),
			     SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		SendMessage((HWND)handle, WM_SIZE, 0, MAKELPARAM(size.width(), size.height()));
#else
		Display *xDisplay = cef_get_xdisplay();

		if (!xDisplay)
			return;

		XWindowChanges changes = {0};
		changes.x = 0;
		changes.y = 0;
		changes.width = size.width();
		changes.height = size.height();
		XConfigureWindow(xDisplay, (Window)handle, CWX | CWY | CWHeight | CWWidth, &changes);
#if CHROME_VERSION_BUILD >= 4638
		XSync(xDisplay, false);
#endif
#endif
	});

	if (success && container)
		container->resize(size.width(), size.height());
#endif
}

void QCefWidgetInternal::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	if (!cefBrowser) {
		obs_browser_initialize();
		unsigned background_color_alpha = 255;
		connect(&timer, &QTimer::timeout, this,
			[this, background_color_alpha]() {
				this->Init(background_color_alpha);
			});
		timer.start(500);
		Init();
	}
}

QPaintEngine *QCefWidgetInternal::paintEngine() const
{
	return nullptr;
}

void QCefWidgetInternal::setURL(const std::string &url_, unsigned background_color_alpha)
{
	url = url_;
	if (cefBrowser) {
		cefBrowser->GetMainFrame()->LoadURL(url);
	}
}

void QCefWidgetInternal::reloadPage()
{
	if (cefBrowser)
		cefBrowser->ReloadIgnoreCache();
}

void QCefWidgetInternal::setStartupScript(const std::string &script_)
{
	script = script_;
}

void QCefWidgetInternal::executeJavaScript(const std::string &script_)
{
	if (!cefBrowser)
		return;

	CefRefPtr<CefFrame> frame = cefBrowser->GetMainFrame();
	std::string url = frame->GetURL();
	frame->ExecuteJavaScript(script_, url, 0);
}

void QCefWidgetInternal::searchText(std::string& text, bool matchCase, bool forward, bool findNext)
{
	searchText_ = text;

	if(cefBrowser)
		cefBrowser->GetHost()->Find(searchText_, forward, matchCase, findNext);
}

void QCefWidgetInternal::stopSearchtext(bool clearSelection)
{
	if (cefBrowser)
		cefBrowser->GetHost()->StopFinding(clearSelection);

}

void QCefWidgetInternal::allowAllPopups(bool allow)
{
	allowAllPopups_ = allow;
}

bool QCefWidgetInternal::zoomPage(int direction)
{
	if (!cefBrowser || direction < -1 || direction > 1)
		return false;

	CefRefPtr<CefBrowserHost> host = cefBrowser->GetHost();
	if (direction == 0) {
		// Reset zoom
		host->SetZoomLevel(0);
		return true;
	}

	int currentZoomPercent = round(pow(1.2, host->GetZoomLevel()) * 100.0);
	int zoomCount = sizeof(zoomLvls) / sizeof(zoomLvls[0]);
	int zoomIdx = 0;

	while (zoomIdx < zoomCount) {
		if (zoomLvls[zoomIdx] == currentZoomPercent) {
			break;
		}
		zoomIdx++;
	}
	if (zoomIdx == zoomCount)
		return false;

	int newZoomIdx = zoomIdx;
	if (direction == -1 && zoomIdx > 0) {
		// Zoom out
		newZoomIdx -= 1;
	} else if (direction == 1 && zoomIdx >= 0 && zoomIdx < zoomCount - 1) {
		// Zoom in
		newZoomIdx += 1;
	}

	if (newZoomIdx != zoomIdx) {
		int newZoomLvl = zoomLvls[newZoomIdx];
		// SetZoomLevel only accepts a zoomLevel, not a percentage
		host->SetZoomLevel(log(newZoomLvl / 100.0) / log(1.2));
		return true;
	}
	return false;
}

/* ------------------------------------------------------------------------- */

struct QCefInternal : QCef {
	virtual bool init_browser(void) override;
	virtual bool initialized(void) override;
	virtual bool wait_for_browser_init(void) override;

	virtual QCefWidget * create_widget(QWidget *parent, const std::string &url,
					  QCefCookieManager *cookie_manager,
					  const std::string &headers,
					  bool dummy = true) override;

	virtual QCefCookieManager *create_cookie_manager(const std::string &storage_path,
							 bool persist_session_cookies) override;

	virtual BPtr<char> get_cookie_path(const std::string &storage_path) override;

	virtual void add_popup_whitelist_url(const std::string &url, QObject *obj) override;
	virtual void add_force_popup_url(const std::string &url, QObject *obj) override;
};

bool QCefInternal::init_browser(void)
{
	if (os_event_try(cef_started_event) == 0)
		return true;

	obs_browser_initialize();
	return false;
}

bool QCefInternal::initialized(void)
{
	return os_event_try(cef_started_event) == 0;
}

bool QCefInternal::wait_for_browser_init(void)
{
	return os_event_wait(cef_started_event) == 0;
}

QCefWidget *QCefInternal::create_widget(QWidget *parent, const std::string &url,
										QCefCookieManager *cm,
										const std::string &headers, bool dummy)
{
	QCefCookieManagerInternal *cmi = reinterpret_cast<QCefCookieManagerInternal *>(cm);

#ifdef _WIN32
	//return new QCefWidgetInternal(parent, url, cmi ? cmi->rc : nullptr);
	return new QCefOsrWidgetInternal(parent, url, cmi ? cmi->rc : nullptr,
					 headers, dummy);
#else
	return new QCefWidgetInternal(parent, url, cmi ? cmi->rc : nullptr,
				      headers);
#endif
}

QCefCookieManager *QCefInternal::create_cookie_manager(const std::string &storage_path, bool persist_session_cookies)
{
	try {
		return new QCefCookieManagerInternal(storage_path, persist_session_cookies);
	} catch (const char *error) {
		blog(LOG_ERROR, "Failed to create cookie manager: %s", error);
		return nullptr;
	}
}

BPtr<char> QCefInternal::get_cookie_path(const std::string &storage_path)
{
	BPtr<char> rpath = obs_module_config_path(storage_path.c_str());
	return os_get_abs_path_ptr(rpath.Get());
}

void QCefInternal::add_popup_whitelist_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	popup_whitelist.emplace_back(url, obj);
}

void QCefInternal::add_force_popup_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	forced_popups.emplace_back(url, obj);
}

extern "C" EXPORT QCef *obs_browser_create_qcef(void)
{
	return new QCefInternal();
}

#define BROWSER_PANEL_VERSION 3

extern "C" EXPORT int obs_browser_qcef_version_export(void)
{
	return BROWSER_PANEL_VERSION;
}



QCefOsrWidgetInternal::QCefOsrWidgetInternal(QWidget* parent, const std::string& url_,
					     CefRefPtr<CefRequestContext> rqc_,
					     const std::string &headers_,
					     bool dummy)
	: QCefWidget(parent), url(url_), rqc(rqc_), headers(headers_)
{

	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);

#ifdef __APPLE__
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_OpaquePaintEvent);
#endif

	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_InputMethodEnabled, true);

	setFocusPolicy(Qt::ClickFocus);

	setMouseTracking(true);

#ifdef _WIN32
	m_eventFilter.reset(BrowserBuildEventFilter());
	installEventFilter(m_eventFilter.get());

	if (dummy == true)
		m_pDummyInteraction = new CDummyInteraction(this);
#endif
	if (window()->windowHandle()) {
		connect(window()->windowHandle(), &QWindow::screenChanged,
			this, &QCefOsrWidgetInternal::ScreenChanged);
	}

	connect(this, &QCefOsrWidgetInternal::OnMainViewBufferReady,
		this, &QCefOsrWidgetInternal::OnMainViewBufferReceive, Qt::QueuedConnection);


	connect(this, &QCefOsrWidgetInternal::OnPopupBufferReady,
		this, &QCefOsrWidgetInternal::OnPopupBufferReceive, Qt::QueuedConnection);
}

QCefOsrWidgetInternal::~QCefOsrWidgetInternal()
{
	DisconnectDummyInteraction();

	closeBrowser();
}

cef_rect_t QCefOsrWidgetInternal::GetViewSize()
{
	int nWidth = browserSize.width();
	int nHeight = browserSize.height();

	if (nWidth <= 0) nWidth = 1;
	if (nHeight <= 0) nHeight = 1;
	cef_rect_t rect = { 0,0,nWidth,nHeight };
	return rect;
}


QRect QCefOsrWidgetInternal::GetPopupRect()
{
	return popupRect;
}

void QCefOsrWidgetInternal::SetPopupRect(CefRect rect)
{
	popupRect.setX(rect.x);
	popupRect.setY(rect.y);
	popupRect.setWidth(rect.width);
	popupRect.setHeight(rect.height);
}

void QCefOsrWidgetInternal::ExecuteOnBrowser(BrowserFunc func, bool async)
{
	if (!async) {
#ifdef ENABLE_BROWSER_QT_LOOP
		if (QThread::currentThread() == qApp->thread()) {
			if (!!cefBrowser)
				func(cefBrowser);
			return;
		}
#endif
		os_event_t* finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&]() {
			if (!!cefBrowser)
				func(cefBrowser);
			os_event_signal(finishedEvent);
			});
		if (success) {
			os_event_wait(finishedEvent);
		}
		os_event_destroy(finishedEvent);
	}
	else {
		CefRefPtr<CefBrowser> browser = GetBrowser();
		if (browser && browser->IsValid()) {
#ifdef ENABLE_BROWSER_QT_LOOP
			QueueBrowserTask(browser, func);
#else
			QueueCEFTask([browser, func]() {
				if (browser && browser->IsValid() && browser->GetHost())
					func(browser);
				});
#endif
		}
	}
}

bool QCefOsrWidgetInternal::HandleMouseClickEvent(QMouseEvent* event)
{
	uint32_t modifiers =  0;
	int32_t x = event->pos().x();
	int32_t y = event->pos().y();

	bool mouseUp = event->type() == QEvent::MouseButtonRelease;
	int clickCount = 1;
	if (event->type() == QEvent::MouseButtonDblClick) {
		clickCount = 2;
	}

	CefBrowserHost::MouseButtonType buttonType = MBT_LEFT;
	switch (event->button()) {
	case Qt::LeftButton:
		buttonType = MBT_LEFT;
		break;
	case Qt::MiddleButton:
		buttonType = MBT_MIDDLE;
		break;
	case Qt::RightButton:
		buttonType = MBT_RIGHT;
		break;
	default:
		blog(LOG_WARNING, "unknown button type %d", event->button());
		return false;
	}

	Qt::MouseButtons buttons = event->buttons();
	if (buttons & Qt::LeftButton)
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (buttons & Qt::MiddleButton)
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if (buttons & Qt::RightButton)
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> browser) {

			if (!browser || !browser->IsValid() || !browser->GetHost())
				return;

			CefMouseEvent e;
			e.modifiers = modifiers;
			e.x = x;
			e.y = y; 
			browser->GetHost()->SendMouseClickEvent(
				e, buttonType, mouseUp, clickCount);
		},
		true);

#ifdef _WIN32
	if (m_pDummyInteraction)
		SetFocus(m_pDummyInteraction->GetHwnd());
#endif

	return true;
}

bool QCefOsrWidgetInternal::HandleMouseMoveEvent(QMouseEvent* event)
{
	int modifiers = 0;
	Qt::MouseButtons buttons = event->buttons();
	if (buttons & Qt::LeftButton)
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (buttons & Qt::MiddleButton)
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	if (buttons & Qt::RightButton)
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

	int32_t x = event->pos().x();
	int32_t y = event->pos().y();

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> browser) {

			if (!browser || !browser->IsValid() || !browser->GetHost())
				return;

			CefMouseEvent e;
			e.modifiers = modifiers;
			e.x = x;
			e.y = y;
			browser->GetHost()->SendMouseMoveEvent(e, false);
		},
		true);

	return true;
}

bool QCefOsrWidgetInternal::HandleMouseLeaveEvent(QEvent* event)
{
	UNUSED_PARAMETER(event);
		
	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> browser) {

			if (!browser || !browser->IsValid() || !browser->GetHost())
				return;

			CefMouseEvent e;
			browser->GetHost()->SendMouseMoveEvent(e, true);
		},
		true);

	return true;
}


bool QCefOsrWidgetInternal::HandleMouseWheelEvent(QWheelEvent* event)
{
	int xDelta = 0;
	int yDelta = 0;

	const QPoint angleDelta = event->angleDelta();
	if (!event->pixelDelta().isNull()) {
		if (angleDelta.x())
			xDelta = event->pixelDelta().x();
		else
			yDelta = event->pixelDelta().y();
	}
	else {
		if (angleDelta.x())
			xDelta = angleDelta.x();
		else
			yDelta = angleDelta.y();
	}


#ifdef __APPLE__
	const bool zoomModifier =
		(event->modifiers() & Qt::MetaModifier);
#else
	const bool zoomModifier =
		(event->modifiers() & Qt::ControlModifier);
#endif

	if (zoomModifier) {

		CefRefPtr<CefBrowser> browser = GetBrowser();
		if (!browser) return false;

		QueueCEFTask([=]() {
			if (!angleDelta.isNull()) {
				if (angleDelta.y() > 0) {
					// Zoom in
					zoomPage(1);
				}
				else if (angleDelta.y() < 0) {
					// Zoom out
					zoomPage(-1);
				}
			}
			});
		event->accept();
		return true;
	}


	const QPointF position = event->position();
	const int x = position.x();
	const int y = position.y();

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> browser) {

			if (!browser || !browser->IsValid() || !browser->GetHost())
				return;

			CefMouseEvent e;
			e.x = x;
			e.y = y;
			browser->GetHost()->SendMouseWheelEvent(e, xDelta, yDelta);
		},
		true);

	return true;
}

bool QCefOsrWidgetInternal::HandleFocusEvent(QFocusEvent* event)
{
	bool focus = (event->type() == QEvent::FocusIn);

	ExecuteOnBrowser(
		[=](CefRefPtr<CefBrowser> browser) {

			if (!browser || !browser->IsValid() || !browser->GetHost())
				return;

#if CHROME_VERSION_BUILD < 4430
			browser->GetHost()->SendFocusEvent(focus);
#else
			UNUSED_PARAMETER(focus);
			browser->GetHost()->SetFocus(true);
#endif
		},
		true);

	return true;
}

bool QCefOsrWidgetInternal::HandleKeyEvent(QKeyEvent* event)
{
	UNUSED_PARAMETER(event);
	return true;
}

BrowserEventFilter* QCefOsrWidgetInternal::BrowserBuildEventFilter()
{
	return new BrowserEventFilter([this](QObject*, QEvent* event) {
		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
			return this->HandleMouseClickEvent(
				static_cast<QMouseEvent*>(event));
		case QEvent::MouseMove:
			return this->HandleMouseMoveEvent(
				static_cast<QMouseEvent*>(event));
		//case QEvent::Enter:
		case QEvent::Leave:
			return this->HandleMouseLeaveEvent(event);
		case QEvent::Wheel:
			return this->HandleMouseWheelEvent(
				static_cast<QWheelEvent*>(event));
		case QEvent::FocusIn:
		case QEvent::FocusOut:
			return this->HandleFocusEvent(
				static_cast<QFocusEvent*>(event));
#if _WIN32
			// WIN32 Use m_pDummyInteraction Window KeyEvent Message
#else
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return this->HandleKeyEvent(
				static_cast<QKeyEvent*>(event));
#endif
		default:
			return false;
		}
		});
}

CefRefPtr<CefBrowser> QCefOsrWidgetInternal::GetBrowser()
{
	return cefBrowser;
}

void QCefOsrWidgetInternal::resizeEvent(QResizeEvent* event)
{
	browserSize = event->size();

	CefRefPtr<CefBrowser> browser = GetBrowser();
	if(browser && browser->GetHost())
		browser->GetHost()->WasResized();
}

void QCefOsrWidgetInternal::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);

	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser) {
		obs_browser_initialize();
		unsigned background_color_alpha = 255;
		connect(&timer, &QTimer::timeout, this,
			[this, background_color_alpha]() {
				this->Init(background_color_alpha);
			});
		timer.start(500);
		Init();
	}
}

void QCefOsrWidgetInternal::paintEvent(QPaintEvent* event)
{
	UNUSED_PARAMETER(event);	

	QImage paintImage;
	{
		std::lock_guard<std::mutex> lock(imageMutex);
		paintImage = offscreenImage;
	}

	{
		QPainter painter(this);
		if (!paintImage.isNull())
		{
			painter.drawImage(0, 0, paintImage);
		}
	}
}

void QCefOsrWidgetInternal::setURL(const std::string& url_, unsigned background_color_alpha)
{
	url = url_;

	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (browser && browser->GetMainFrame()) {
		browser->GetMainFrame()->LoadURL(url);
	}
	else {
		obs_browser_initialize();
		connect(&timer, &QTimer::timeout, this,
			[this, background_color_alpha]() {
				this->Init(background_color_alpha);
			});
		timer.start(500);
		Init(background_color_alpha);
	}
}

void QCefOsrWidgetInternal::setStartupScript(const std::string& script_)
{
	script = script_;
}

void QCefOsrWidgetInternal::allowAllPopups(bool allow)
{
	allowAllPopups_ = allow;
}

void QCefOsrWidgetInternal::closeBrowser()
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (browser && browser->IsValid()) {
		auto destroyBrowser = [](CefRefPtr<CefBrowser> browser) {

			if (!browser || !browser->IsValid() || !browser->GetHost())
				return;

			CefRefPtr<CefClient> client =
				browser->GetHost()->GetClient();
			QCefOsrBrowserClient* bc =
				reinterpret_cast<QCefOsrBrowserClient*>(
					client.get());

			if (bc) {
				bc->widget = nullptr;
			}

			browser->GetHost()->CloseBrowser(true);
		};

		/* So you're probably wondering what's going on here.  If you
		 * call CefBrowserHost::CloseBrowser, and it fails to unload
		 * the web page *before* WM_NCDESTROY is called on the browser
		 * HWND, it will call an internal CEF function
		 * CefBrowserPlatformDelegateNativeWin::CloseHostWindow, which
		 * will attempt to close the browser's main window itself.
		 * Problem is, this closes the root window containing the
		 * browser's HWND rather than the browser's specific HWND for
		 * whatever mysterious reason.  If the browser is in a dock
		 * widget, then the window it closes is, unfortunately, the
		 * main program's window, causing the entire program to shut
		 * down.
		 *
		 * So, instead, before closing the browser, we need to decouple
		 * the browser from the widget.  To do this, we hide it, then
		 * remove its parent. */
#ifdef _WIN32
		if (browser->GetHost()) {
			HWND hwnd = (HWND)browser->GetHost()->GetWindowHandle();
			if (hwnd) {
				ShowWindow(hwnd, SW_HIDE);
				SetParent(hwnd, nullptr);
			}
		}
#elif __APPLE__
		 // felt hacky, might delete later
		void* view = (id)browser->GetHost()->GetWindowHandle();
		if (*((bool*)view))
			((void (*)(id, SEL))objc_msgSend)(
				(id)view, sel_getUid("removeFromSuperview"));
#endif
		destroyBrowser(browser);
		cefBrowser = nullptr;
	}
}

void QCefOsrWidgetInternal::reloadPage()
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (browser && browser->IsValid() && browser->GetHost())
		browser->ReloadIgnoreCache();
}

bool QCefOsrWidgetInternal::zoomPage(int direction)
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser || !browser->IsValid() || !browser->GetHost())
		return false;

	if (direction < -1 || direction > 1)
		return false;

	CefRefPtr<CefBrowserHost> host = browser->GetHost();
	if (direction == 0) {
		// Reset zoom
		host->SetZoomLevel(0);
		return true;
	}

	int currentZoomPercent = round(pow(1.2, host->GetZoomLevel()) * 100.0);
	int zoomCount = sizeof(zoomLvls) / sizeof(zoomLvls[0]);
	int zoomIdx = 0;

	while (zoomIdx < zoomCount) {
		if (zoomLvls[zoomIdx] == currentZoomPercent) {
			break;
		}
		zoomIdx++;
	}
	if (zoomIdx == zoomCount)
		return false;

	int newZoomIdx = zoomIdx;
	if (direction == -1 && zoomIdx > 0) {
		// Zoom out
		newZoomIdx -= 1;
	}
	else if (direction == 1 && zoomIdx >= 0 && zoomIdx < zoomCount - 1) {
		// Zoom in
		newZoomIdx += 1;
	}

	if (newZoomIdx != zoomIdx) {
		int newZoomLvl = zoomLvls[newZoomIdx];
		// SetZoomLevel only accepts a zoomLevel, not a percentage
		host->SetZoomLevel(log(newZoomLvl / 100.0) / log(1.2));
		return true;
	}
	return false;
}

void QCefOsrWidgetInternal::executeJavaScript(const std::string& script_)
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser) return;
	if (script_.empty()) return;

	QueueCEFTask([browser, code = std::string(script_)]() {

		if (!browser || !browser->IsValid() || !browser->GetHost())
			return;

		CefRefPtr<CefFrame> frame = browser->GetMainFrame();
		if (!frame || !frame->IsValid()) return;

		CefString url = frame->GetURL();
		frame->ExecuteJavaScript(code, url, 0);
		});
}

void QCefOsrWidgetInternal::searchText(std::string& text, bool matchCase, bool forward, bool findNext)
{
	searchText_ = text;

	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser) return;

	QueueCEFTask([browser,
		s = std::string(searchText_),
		forward, matchCase, findNext] {
			auto host = browser->GetHost();
			if (!host) return;
			browser->GetHost()->Find(s, forward, matchCase, findNext);
		});

}

void QCefOsrWidgetInternal::stopSearchtext(bool clearSelection)
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser) return;

	QueueCEFTask([browser, clearSelection] {
		auto host = browser->GetHost();
		if (!host) return;
		host->StopFinding(clearSelection);
		});
}

void QCefOsrWidgetInternal::finishCloseBrowser()
{
	emit readyToClose();
}

#ifdef _WIN32
void QCefOsrWidgetInternal::SetIME(bool show)
{
	if(m_pDummyInteraction)
		m_pDummyInteraction->SetIME(show);
}

void QCefOsrWidgetInternal::DisconnectDummyInteraction()
{
	removeEventFilter(m_eventFilter.get());

	if (m_pDummyInteraction != nullptr) {
		delete m_pDummyInteraction;
		m_pDummyInteraction = nullptr;
	}
}

#endif

void QCefOsrWidgetInternal::UpdateBuffer(int type, const void* buffer, int width, int height)
{
	if (0 == width || 0 == height || nullptr == buffer) {
		return;
	}

	// 1 : PET_POPUP, 0 : PET_VIEW
	if (type == 0) {
		UpdateMainViewBuffer(buffer, width, height);
	}
	else if (type == 1) {
		UpdatePopupBuffer(buffer, width, height);
	}
}


void QCefOsrWidgetInternal::UpdateMainViewBuffer(const void* buffer, int width, int height)
{
	// Sanity check: Ensure buffer is valid and image dimensions are within typical GPU limits.
	if (!buffer || width <= 0 || height <= 0 || width > 16384 || height > 16384)
		return;

	int bytesPerLine = width * 4;
	int dataSize = bytesPerLine * height;

	//
	QByteArray safeCopy;
	safeCopy.resize(dataSize);
	memcpy(safeCopy.data(), buffer, dataSize);

	// QByteArray -> QImage (shallow wrap)
	QImage tmpImg(reinterpret_cast<const uchar*>(safeCopy.constData()),
		width,
		height,
		bytesPerLine,
		QImage::Format_ARGB32_Premultiplied);

	if (tmpImg.isNull())
		return;

	// 
	QImage copiedImage = tmpImg.copy();
	emit OnMainViewBufferReady(std::move(copiedImage));
}

void QCefOsrWidgetInternal::UpdatePopupBuffer(const void* buffer, int width, int height)
{
	// Sanity check: Ensure buffer is valid and image dimensions are within typical GPU limits.
	if (!buffer || width <= 0 || height <= 0 || width > 16384 || height > 16384)
		return;

	const int bytesPerLine = width * 4;
	const int dataSize = bytesPerLine * height;

	QByteArray safeCopy;
	safeCopy.resize(dataSize);
	memcpy(safeCopy.data(), buffer, dataSize);

	QImage popupImage(reinterpret_cast<const uchar*>(safeCopy.constData()),
		width, height, bytesPerLine, QImage::Format_ARGB32_Premultiplied);

	if (popupImage.isNull()) return;

	emit OnPopupBufferReady(popupImage.copy());
}

void QCefOsrWidgetInternal::OnMainViewBufferReceive(QImage mainView)
{
	if (mainView.isNull())
		return;

	{
		std::lock_guard<std::mutex> lock(imageMutex);
		mainView.setDevicePixelRatio(devicePixelRatioF());
		offscreenImage = std::move(mainView);
	}

	bool expected = false;
	if (updatePending.compare_exchange_strong(expected, true)) {
		QMetaObject::invokeMethod(this, [this] {
			update();
			updatePending.store(false);
			}, Qt::QueuedConnection);
	}
}

void QCefOsrWidgetInternal::OnPopupBufferReceive(QImage popup)
{
	if (popup.isNull())
		return;

	std::lock_guard<std::mutex> lock(imageMutex);

	if (offscreenImage.isNull())
		return;

	popup.setDevicePixelRatio(devicePixelRatioF());

	QPainter painter(&offscreenImage);
	if (!painter.isActive())
		return;

	QRect targetRect = popupRect.intersected(offscreenImage.rect());
	QRect sourceRect = targetRect.translated(-popupRect.topLeft());

	painter.drawImage(targetRect.topLeft(), popup, sourceRect);
	update();
}

void QCefOsrWidgetInternal::ApplyCursorChange(cef_cursor_type_t type, const CefCursorInfo& info)
{
	UNUSED_PARAMETER(info);
	
	switch (type) {
	case CT_POINTER:
		unsetCursor();
		break;
	case CT_IBEAM:
		setCursor(Qt::IBeamCursor);
		break;
	case CT_HAND:
		setCursor(Qt::PointingHandCursor);
		break;
	case CT_WAIT:
		setCursor(Qt::WaitCursor);
		break;
	//case CT_CUSTOM:
	//	break;
	default:
		unsetCursor();
		break;
	}
}

void QCefOsrWidgetInternal::Init(unsigned background_color_alpha)
{

#ifndef __APPLE__
	WId handle = winId();
	QSize size = this->size();
	size *= devicePixelRatioF();
	bool success = QueueCEFTask(
		[this, handle, size, background_color_alpha]()
#else
	WId handle = winId();
	bool success = QueueCEFTask(
		[this, handle]()
#endif
	{
		CefWindowInfo windowInfo;

		if (cefBrowser)
			return;


#if defined(_WIN32)
		windowInfo.bounds.width = size.width();
		windowInfo.bounds.height = size.height();
		windowInfo.windowless_rendering_enabled = true;
		
		windowInfo.parent_window = (CefWindowHandle)handle;
#else
		windowInfo.SetAsWindowless(0);
#endif

		CefRefPtr<QCefOsrBrowserClient> browserClient =
			new QCefOsrBrowserClient(this, headers, script, allowAllPopups_);

		CefBrowserSettings cefBrowserSettings;
		cefBrowserSettings.windowless_frame_rate = 30;
		cefBrowserSettings.background_color = CefColorSetARGB(background_color_alpha, 255, 255, 255);

		cefBrowser = CefBrowserHost::CreateBrowserSync(
			windowInfo, browserClient, url,
			cefBrowserSettings,
			CefRefPtr<CefDictionaryValue>(), rqc);

#if defined(_WIN32)
		if(m_pDummyInteraction)
			m_pDummyInteraction->SetCefBrowser(cefBrowser);
#endif

#ifdef __linux__
		QueueCEFTask([this]() { unsetToplevelXdndProxy(); });
#endif
		});

	if (success) {
		timer.stop();
	}
}

void QCefOsrWidgetInternal::ScreenChanged()
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if(browser && browser->GetHost())
		browser->GetHost()->NotifyScreenInfoChanged();
}

void QCefOsrWidgetInternal::SetFocusDummyInteraction()
{
#ifdef _WIN32
	if (m_pDummyInteraction)
		SetFocus(m_pDummyInteraction->GetHwnd());
#endif
}

////////////////////////////////////////////////////////////////////////////
//
// For Dummy Browser Interaction 
#ifdef _WIN32
CDummyInteraction::CDummyInteraction(QWidget* parent) :
	pParent(parent)
{
	// Using the WINAPI to create a window.
	_CreateInterationWindow();

	// Convert HWND to QWidget
	if (hwnd) {
		imeHandler.reset(new client::OsrImeHandlerWin(hwnd));
	}
}

CDummyInteraction::~CDummyInteraction()
{
	imeHandler.reset();

	DestroyWindow(hwnd);
	hwnd = NULL;

	pParent = nullptr;
	cefBrowser = nullptr;
}

void CDummyInteraction::_CreateInterationWindow()
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);

	WNDCLASS wc = {};
	wc.lpfnWndProc = CDummyInteraction::WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"CDummyInteraction";
	RegisterClass(&wc);

	hwnd = CreateWindowEx(
		0, L"CDummyInteraction", L"DummyInteractionWindow",
		WS_OVERLAPPEDWINDOW, 0, 0, 1, 1,
		nullptr, nullptr, hInstance, nullptr
	);

	::SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
	::SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);
}

LRESULT CDummyInteraction::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CDummyInteraction* pThis = nullptr;
	if (uMsg == WM_CREATE) {
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		pThis = static_cast<CDummyInteraction*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
	}
	else {
		pThis = reinterpret_cast<CDummyInteraction*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (pThis) {
		return pThis->BrowserInteractionProc(hwnd, uMsg, wParam, lParam);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CDummyInteraction::BrowserInteractionProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CefRefPtr<CefBrowser> browser = GetBrowser();
	if (!browser || !browser->IsValid() || !browser->GetHost())
		return 0;

	if (uMsg == WM_CREATE) {
		return 0;
	}
	else if (uMsg == WM_CLOSE) {
		PostQuitMessage(0);
		return 0;
	}
	else if (uMsg == WM_DESTROY) {
		return 0;
	}
	else if (WM_KEYFIRST <= uMsg && WM_KEYLAST >= uMsg) {

		CefKeyEvent event;
		event.windows_key_code = wParam;
		event.native_key_code = lParam;
		event.is_system_key = uMsg == WM_SYSCHAR ||
				      uMsg == WM_SYSKEYDOWN ||
				      uMsg == WM_SYSKEYUP;

		if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
			event.type = KEYEVENT_RAWKEYDOWN;
		else if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP)
			event.type = KEYEVENT_KEYUP;
		else
			event.type = KEYEVENT_CHAR;
		event.modifiers = GetCefKeyboardModifiers2(wParam, lParam);

		browser->GetHost()->SendKeyEvent(event);

		return 0;
	}
	else if (WM_IME_SETCONTEXT == uMsg) {

		if (showIME)
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

		lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
		::DefWindowProc(hwnd, uMsg, wParam, lParam);

		if (imeHandler) {
			imeHandler->CreateImeWindow();
			imeHandler->MoveImeWindow();
		}

		return 0;
	}
	else if (WM_IME_COMPOSITION == uMsg) {

		if (showIME)
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

		if (browser && browser->IsValid() && browser->GetHost() && imeHandler) {
			CefString compositionText;
			if (imeHandler->GetResult(lParam, compositionText)) {
				browser->GetHost()->ImeCommitText(compositionText,
					CefRange(UINT32_MAX, UINT32_MAX), 0);
				imeHandler->ResetComposition();
			}

			std::vector<CefCompositionUnderline> underlines;
			int compositionStart = 0;

			if (imeHandler->GetComposition(lParam, compositionText, underlines,
				compositionStart)) {
				browser->GetHost()->ImeSetComposition(
					compositionText, underlines, CefRange(UINT32_MAX, UINT32_MAX),
					CefRange(compositionStart,
						static_cast<int>(compositionStart + compositionText.length())));
				imeHandler->UpdateCaretPosition(compositionStart - 1);
			}
			else {
				if (browser && imeHandler) {
					browser->GetHost()->ImeCancelComposition();
					imeHandler->ResetComposition();
					imeHandler->DestroyImeWindow();
				}
			}
		}

		return 0;
	}
	else if (WM_IME_STARTCOMPOSITION == uMsg) {

		if (showIME)
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

		if (imeHandler) {
			imeHandler->CreateImeWindow();
			imeHandler->MoveImeWindow();
			imeHandler->ResetComposition();
		}
		return 0;

	}
	else if (WM_IME_ENDCOMPOSITION == uMsg) {

		if (showIME)
			return DefWindowProc(hwnd, uMsg, wParam, lParam);

		if (browser && imeHandler) {
			browser->GetHost()->ImeCancelComposition();
			imeHandler->ResetComposition();
			imeHandler->DestroyImeWindow();
		}

		return 0;

	}
	else
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CDummyInteraction::SetCefBrowser(CefRefPtr<CefBrowser> browser)
{
	cefBrowser = browser;
}

CefRefPtr<CefBrowser> CDummyInteraction::GetBrowser()
{
	return cefBrowser;
}

void CDummyInteraction::SetIME(bool show)
{
	showIME = show;
}

#endif
