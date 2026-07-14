/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#pragma once

#include <obs-module.h>

#include "cef-headers.hpp"
#include "browser-app.hpp"
#include <atomic>
#include <functional>
#include <string>
#include <mutex>

#if CHROME_VERSION_BUILD < 4103
#include <obs.hpp>
#include <unordered_map>
#include <vector>

struct AudioStream {
	OBSSourceAutoRelease source;
	speaker_layout speakers;
	int channels;
	int sample_rate;
};
#endif // CHROME_VERSION_BUILD < 4103

enum class ControlLevel : int {
	None,
	ReadObs,
	ReadUser,
	Basic,
	Advanced,
	All,
};
inline constexpr ControlLevel DEFAULT_CONTROL_LEVEL = ControlLevel::ReadObs;

extern bool hwaccel;
extern std::string browser_cookie;

extern void SetBrowserCookie(std::string cookie);
extern std::string GetBrowserCookie();

// aqua style
struct StyleItem {
	std::string name;
	std::string key;
};
typedef std::vector<StyleItem> StyleList;
struct ScoreInfo {
	std::string style_name;
	int styleIdx = 0;
	int transparency = 0;
	int thema = 1;
	std::string left_name;
	std::string right_name;
	int left_score = 0;
	int right_score = 0;
	bool use_set = true;
	int left_set = 0;
	int right_set = 0;
	bool use_timer = true;
};

struct MissionInfo {
	std::string theme = "default";
	std::string sort_type = "regDate";
	bool only_progress_flag = true;
	int transparency = 100;
	int volume = 100;
	bool mute = false;
};

struct AnimeSubtitleInfo {
	std::string style_name;
	int style_idx = 0;
	std::string lang_type;

	int transparency = 0;
	bool display_quickmenu = false;
	bool display_background = false;
	std::string background_color;
	int background_transparency = 100;

	std::string font_background_color;
	std::string font_color;
	std::string font_face;
	int	    font_size = 54;
	bool	    font_bold;
	int	    font_transparency;
	bool	    font_outline_display;
	std::string font_outline_color;
	int	    font_outline_size;
	int	    font_align_type;

	bool	    use_animation;
	int	    animation_speed;

	std::string subtitle_text;
	int	    subtitle_theme;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class IMessageHandler {
public:
	explicit IMessageHandler() {}
	virtual ~IMessageHandler() {}
	//
	virtual int CefQuery(std::string utf8data,
                std::string &resultdata) = 0;
};
//
struct BrowserSource : public IMessageHandler {
	BrowserSource **p_prev_next = nullptr;
	BrowserSource *next = nullptr;

	obs_source_t *source = nullptr;

	bool tex_sharing_avail = false;
	bool create_browser = false;
	std::recursive_mutex lockBrowser;
	CefRefPtr<CefBrowser> cefBrowser;

	std::string id;
	std::string type;
	std::string kbo_type;
	std::string football_type;

	std::string url;
	std::string style;
	std::string css;
	gs_texture_t *texture = nullptr;
	gs_texture_t *extra_texture = nullptr;
	uint32_t last_cx = 0;
	uint32_t last_cy = 0;
	gs_color_format last_format = GS_UNKNOWN;

	gs_texture_t* popup_texture = nullptr;
	gs_texture_t* popup_extra_texture = nullptr;
	uint32_t popup_last_cx = 0;
	uint32_t popup_last_cy = 0;
	gs_color_format popup_last_format = GS_UNKNOWN;

	// aqua style
	StyleList style_list;
	ScoreInfo score_info;
	MissionInfo mission_info;
	AnimeSubtitleInfo anime_subtitle_info;

	int style_index;	// use animation subtitle

	std::string script;

#ifdef ENABLE_BROWSER_SHARED_TEXTURE
#ifdef _WIN32
	void *last_handle = INVALID_HANDLE_VALUE;
	void *popup_last_handle = INVALID_HANDLE_VALUE;
#elif defined(__APPLE__)
	void *last_handle = nullptr;
	void *popup_last_handle = nullptr;
#endif
#endif

	int width = 0;
	int height = 0;
	bool fps_custom = false;
	int fps = 0;
	double canvas_fps = 0;
	bool restart = false;
	bool shutdown_on_invisible = false;
	bool is_local = false;
	bool first_update = true;
	bool reroute_audio = false;
	std::atomic<bool> destroying = false;
	ControlLevel webpage_control_level = DEFAULT_CONTROL_LEVEL;
#if defined(BROWSER_EXTERNAL_BEGIN_FRAME_ENABLED) && defined(ENABLE_BROWSER_SHARED_TEXTURE)
	bool reset_frame = false;
#endif
	bool is_showing = false;

	CefRect popup_rect;
	int popup_width = 0;
	int popup_height = 0;
	bool is_showing_popup = false;

	inline void DestroyTextures()
	{
		obs_enter_graphics();
		if (extra_texture) {
			gs_texture_destroy(extra_texture);
			extra_texture = nullptr;
			last_cx = 0;
			last_cy = 0;
			last_format = GS_UNKNOWN;
		}
		if (texture) {
			gs_texture_destroy(texture);
			texture = nullptr;
		}
		obs_leave_graphics();
	}

	inline void DestroyPopupTextures()
	{
		obs_enter_graphics();
		if (popup_extra_texture) {
			gs_texture_destroy(popup_extra_texture);
			popup_extra_texture = nullptr;
			popup_last_cx = 0;
			popup_last_cy = 0;
			popup_last_format = GS_UNKNOWN;
		}
		if (popup_texture) {
			gs_texture_destroy(popup_texture);
			popup_texture = nullptr;
		}
		obs_leave_graphics();
	}

	/* ---------------------------- */

	bool CreateBrowser();
	void DestroyBrowser();
	void ExecuteOnBrowser(BrowserFunc func, bool async = false);

	/* ---------------------------- */

	BrowserSource(obs_data_t *settings, obs_source_t *source);
	~BrowserSource();

	void Destroy();

	void Update(obs_data_t *settings = nullptr);
	void Tick();
	void Render();
#if CHROME_VERSION_BUILD < 4103
	void ClearAudioStreams();
	void EnumAudioStreams(obs_source_enum_proc_t cb, void *param);
	bool AudioMix(uint64_t *ts_out, struct audio_output_data *audio_output, size_t channels, size_t sample_rate);
	std::mutex audio_sources_mutex;
	std::vector<obs_source_t *> audio_sources;
	std::unordered_map<int, AudioStream> audio_streams;
#endif
	void SendMouseClick(const struct obs_mouse_event *event, int32_t type, bool mouse_up, uint32_t click_count);
	void SendMouseMove(const struct obs_mouse_event *event, bool mouse_leave);
	void SendMouseWheel(const struct obs_mouse_event *event, int x_delta, int y_delta);
	void SendFocus(bool focus);
	void SendKeyClick(const struct obs_key_event *event, bool key_up);
	void SetShowing(bool showing);
	void SetActive(bool active);
	void Refresh();

	void ExcuteJavaScript(const std::string& script);
	
#if defined(BROWSER_EXTERNAL_BEGIN_FRAME_ENABLED) && defined(ENABLE_BROWSER_SHARED_TEXTURE)
	inline void SignalBeginFrame();
#endif

	void SetBrowser(CefRefPtr<CefBrowser> b);
	CefRefPtr<CefBrowser> GetBrowser();

	void SetCookies(std::string _cookies) {
        UNUSED_PARAMETER(_cookies);
	}

	// cefQuery
	virtual int CefQuery(std::string utf8data, std::string &resultdata);

	// aqua-source
	void UpdateChatSource(obs_data_t *settings = nullptr);
	void UpdateChatScoreSource(obs_data_t *settings = nullptr);
	void UpdateAnimeSubtitleSource(obs_data_t* settings = nullptr);
	// video balloon
	void UpdateVideoBalloonSource(obs_data_t *settings = nullptr);
	// kbo graphic
	void UpdateKBOGraphicSource(obs_data_t *settings = nullptr);
	// Football graphic
	void UpdateFootballGraphicSource(obs_data_t *settings = nullptr);
	// commerce
	void UpdateCommerceSource(obs_data_t *settings = nullptr);
	// mission
	void UpdateMissionSource(obs_data_t *settings = nullptr);
	// chat mood check
	void UpdateChatMoodCheckSource(obs_data_t *settings = nullptr);

	void UpdateAIManagerSource(obs_data_t *settings = nullptr);

	void UpdatePainterSource(obs_data_t* settings = nullptr);
};
