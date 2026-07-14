#pragma once

#include <QTimer>
#include <QPointer>
#include "browser-panel.hpp"
#include "cef-headers.hpp"
#include "browser-app.hpp"
#include "osr_ime_handler_win.h"
#include <nlohmann/json.hpp>

#include <vector>
#include <mutex>

struct PopupWhitelistInfo {
	std::string url;
	QPointer<QObject> obj;

	inline PopupWhitelistInfo(const std::string &url_, QObject *obj_) : url(url_), obj(obj_) {}
};

extern std::mutex popup_whitelist_mutex;
extern std::vector<PopupWhitelistInfo> popup_whitelist;
extern std::vector<PopupWhitelistInfo> forced_popups;

/* ------------------------------------------------------------------------- */
class QCefWidgetInternal : public QCefWidget {
	Q_OBJECT

public:
	QCefWidgetInternal(QWidget *parent, const std::string &url,
			   		   CefRefPtr<CefRequestContext> rqc,
			   		   const std::string &headers_);
	~QCefWidgetInternal();

	CefRefPtr<CefBrowser> cefBrowser;
	std::string url;
	std::string headers;
	std::string script;
	std::string searchText_;
	CefRefPtr<CefRequestContext> rqc;
	QTimer timer;
#ifndef __APPLE__
	QPointer<QWindow> window;
	QPointer<QWidget> container;
#endif
	bool allowAllPopups_ = false;

	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void showEvent(QShowEvent *event) override;
	virtual QPaintEngine *paintEngine() const override;

	virtual void setURL(const std::string &url, unsigned background_color_alpha = 255) override;
	virtual void setStartupScript(const std::string &script) override;
	virtual void allowAllPopups(bool allow) override;
	virtual void closeBrowser() override;
	virtual void reloadPage() override;
	virtual bool zoomPage(int direction) override;
	virtual void executeJavaScript(const std::string &script) override;
	virtual void searchText(std::string& text, bool matchCase, bool forward, bool findNext) override;
	virtual void stopSearchtext(bool clearSelection) override;
	
#ifdef  _WIN32
	virtual void SetIME(bool show) {};
#endif

	void Resize();

#ifdef __linux__
private:
	bool needsDeleteXdndProxy = true;
	void unsetToplevelXdndProxy();
#endif

public slots:
	void Init(unsigned background_color_alpha = 255);

signals:
	void readyToClose();
};


class CDummyInteraction;

typedef std::function<bool(QObject*, QEvent*)> EventFilterFunc;
class BrowserEventFilter : public QObject {
	Q_OBJECT
public:
	BrowserEventFilter(EventFilterFunc filter_) : filter(filter_) {}

protected:
	bool eventFilter(QObject* obj, QEvent* event)
	{
		return filter(obj, event);
	}

public:
	EventFilterFunc filter;
};

class QCefOsrWidgetInternal : public QCefWidget {
	Q_OBJECT
public:
	QCefOsrWidgetInternal(QWidget* parent, const std::string& url,
			      CefRefPtr<CefRequestContext> rqc,
			      const std::string &headers, bool dummy = true);
	~QCefOsrWidgetInternal();

	CefRefPtr<CefBrowser> cefBrowser;
	QTimer timer;
	std::string url;
	std::string headers;
	std::string script;
	std::string searchText_;
	CefRefPtr<CefRequestContext> rqc;
	bool allowAllPopups_ = false;

	QSize   browserSize;
	QRect   popupRect;

	std::mutex imageMutex;
	std::atomic<bool> updatePending{false};
	QImage offscreenImage;
	std::unique_ptr<BrowserEventFilter> m_eventFilter;

#ifdef _WIN32
	CDummyInteraction* m_pDummyInteraction = nullptr;
#endif

	CefRefPtr<CefBrowser> GetBrowser();

	virtual void resizeEvent(QResizeEvent* event) override;
	virtual void showEvent(QShowEvent* event) override;
	virtual void paintEvent(QPaintEvent* event) override;

	virtual void setURL(const std::string& url, unsigned background_color_alpha = 255) override;
	virtual void setStartupScript(const std::string& script) override;
	virtual void allowAllPopups(bool allow) override;
	virtual void closeBrowser() override;
	virtual void reloadPage() override;
	virtual bool zoomPage(int direction) override;
	virtual void executeJavaScript(const std::string& script) override;
	virtual void searchText(std::string& text, bool matchCase, bool forward, bool findNext) override;
	virtual void stopSearchtext(bool clearSelection) override;
#ifdef _WIN32
	virtual void SetIME(bool show) override;
#endif

public:
	void finishCloseBrowser();

#ifdef _WIN32
	void DisconnectDummyInteraction();
#endif

	void UpdateBuffer(int type, const void* buffer, int width, int height);
	void UpdateMainViewBuffer(const void* buffer, int width, int height);
	void UpdatePopupBuffer(const void* buffer, int width, int height);
	void ApplyCursorChange(cef_cursor_type_t type, const CefCursorInfo& info);

	bool HandleMouseClickEvent(QMouseEvent* event);
	bool HandleMouseMoveEvent(QMouseEvent* event);
	bool HandleMouseLeaveEvent(QEvent* event);
	bool HandleMouseWheelEvent(QWheelEvent* event);
	bool HandleFocusEvent(QFocusEvent* event);
	bool HandleKeyEvent(QKeyEvent* event);

	BrowserEventFilter* BrowserBuildEventFilter();

	void ExecuteOnBrowser(BrowserFunc func, bool async);

	cef_rect_t GetViewSize();

	QRect	GetPopupRect();
	void	SetPopupRect(CefRect rect);

signals:
	void OnMainViewBufferReady(QImage mainView);
	void OnPopupBufferReady(QImage popup);
	void readyToClose();

public slots:
	void Init(unsigned background_color_alpha = 255);
	void ScreenChanged();
	void SetFocusDummyInteraction();

	void OnMainViewBufferReceive(QImage mainView);
	void OnPopupBufferReceive(QImage popup);
};


#ifdef _WIN32
class CDummyInteraction {
public:
	CDummyInteraction(QWidget* parent);
	~CDummyInteraction();

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT  BrowserInteractionProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	HWND	 GetHwnd() { return hwnd; }
	void	 SetCefBrowser(CefRefPtr<CefBrowser> browser);
	CefRefPtr<CefBrowser> GetBrowser();
	void	 SetIME(bool show);

private:
	void _CreateInterationWindow();

private:
	HWND hwnd = NULL;
	CefRefPtr<CefBrowser> cefBrowser = nullptr;
	std::unique_ptr<client::OsrImeHandlerWin> imeHandler;
	bool showIME = false;

	QWidget* pParent = nullptr;
};
#endif
