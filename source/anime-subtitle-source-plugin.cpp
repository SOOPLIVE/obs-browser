
#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/dstr.hpp>
#include <obs-module.h>
#include <obs.hpp>
#include <functional>
#include <sstream>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <qstring.h>
#include <qdesktopservices.h>
#include <qurl.h>

#include "obs-browser-source.hpp"
#include "browser-scheme.hpp"
#include "browser-app.hpp"
#include "browser-version.h"

#include "cef-headers.hpp"

// request api
#include <util/curl/curl-helper.h>
#include <qjsonobject.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
//
#include "soop-source-plugin.h"
#include "CommonUtils.h"

//
static bool GetStyleList(StyleList& list)
{
	return false;
}
static bool GetAnimationSubtitleInfo(const char *key, AnimeSubtitleInfo& info)
{
	return false;
}
static void SetAnimeSubtitleInfo(obs_data_t *settings, const AnimeSubtitleInfo &info)
{
}

static void update_anime_subtitle_info(BrowserSource *source)
{
}

static void UpdateAnimeSubtitleInfoQuery(void* data)
{
	if (!data) return;

	BrowserSource* bs = static_cast<BrowserSource*>(data);
	update_anime_subtitle_info(bs);
}

static bool change_anime_subtitle_info(void *data, obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	return false;
}
//
obs_properties_t* anime_subtitle_source_get_properties(void* data)
{
	return nullptr;
}

extern void soop_browser_source_get_defaults(obs_data_t* settings);
static const char *default_css = "body { \
					background-color: rgba(0, 0, 0, 0); \
					margin: 0px auto; \
					overflow: hidden; \
					}";

void anime_subtitle_source_get_defaults(obs_data_t *settings)
{
	soop_browser_source_get_defaults(settings);

	obs_data_set_default_string(settings, "url", "");

	obs_data_set_default_string(settings, "url", "");
	obs_data_set_default_string(settings, "css", default_css);

	obs_data_set_default_string(settings, "skin_key", "");
	obs_data_set_default_string(settings, "subtitle_text", "");
	obs_data_set_default_int(settings, "subtitle_theme", 0);
}

extern "C" EXPORT void obs_browser_initialize(void);
//
void RegisterAnimeSubtitleSource()
{
	struct obs_source_info info = {};
	//
	info.id = "soop_chat_source_anmSubtitle";
	info.get_name = [](void *) {
		return obs_module_text("Dowoomi.GetName.AnimeSubtitle");
	};
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
		OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
		OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB |
		OBS_SOURCE_NOT_DRAW_PREVIEW;

	info.get_properties = anime_subtitle_source_get_properties;
	info.get_defaults = anime_subtitle_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;
	//
	info.create = [](obs_data_t *settings,
			 obs_source_t *source) -> void * {
		obs_browser_initialize();
		BrowserSource* bs = new BrowserSource(settings, source);

		//StyleList style_list;
		bool success = GetStyleList(bs->style_list);
		if (success) {
			std::string key_ = bs->style_list[0].key;
			std::string url_ = SOOP_AQUA_GET_COMPONENT + key_;
			obs_data_set_string(settings, "url", url_.c_str());
			obs_data_set_string(settings, "skin_key", key_.c_str());
			//
			AnimeSubtitleInfo& info = bs->anime_subtitle_info;
			success = GetAnimationSubtitleInfo(key_.c_str(), info);
			if (success)
				SetAnimeSubtitleInfo(settings, info);
		}

		return bs;
	};
	info.destroy = [](void *data) {
		static_cast<BrowserSource *>(data)->Destroy();
	};
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->UpdateAnimeSubtitleSource(
			settings);
	};
	info.get_width = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)
			->width;
	};
	info.get_height = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)
			->height;
	};
	info.video_tick = [](void *data, float) {
		static_cast<BrowserSource *>(data)->Tick();
	};
	info.video_render = [](void *data, gs_effect_t *) {
		static_cast<BrowserSource *>(data)->Render();
	};
#if CHROME_VERSION_BUILD < 4103
	info.audio_mix = [](void *data, uint64_t *ts_out,
				struct audio_output_data *audio_output,
				size_t channels, size_t sample_rate) {
		return static_cast<BrowserSource *>(data)->AudioMix(
			ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void *data,
					obs_source_enum_proc_t cb,
					void *param) {
		static_cast<BrowserSource *>(data)->EnumAudioStreams(
			cb, param);
	};
#endif
	info.mouse_click = [](void *data,
				const struct obs_mouse_event *event,
				int32_t type, bool mouse_up,
				uint32_t click_count) {
		static_cast<BrowserSource *>(data)->SendMouseClick(
			event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void *data,
				const struct obs_mouse_event *event,
				bool mouse_leave) {
		static_cast<BrowserSource *>(data)->SendMouseMove(
			event, mouse_leave);
	};
	info.mouse_wheel = [](void *data,
				const struct obs_mouse_event *event,
				int x_delta, int y_delta) {
		static_cast<BrowserSource *>(data)->SendMouseWheel(
			event, x_delta, y_delta);
	};
	info.focus = [](void *data, bool focus) {
		static_cast<BrowserSource *>(data)->SendFocus(focus);
	};
	info.key_click = [](void *data,
				const struct obs_key_event *event,
				bool key_up) {
		static_cast<BrowserSource *>(data)->SendKeyClick(
			event, key_up);
	};
	info.show = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(true);
	};
	info.hide = [](void *data) {
		static_cast<BrowserSource *>(data)->SetShowing(false);
	};
	info.activate = [](void *data) {
		BrowserSource *bs = static_cast<BrowserSource *>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
		//
		/*obs_source_set_name(bs->source,
				    obs_module_text("Dowoomi.GetName.Score"));*/
	};
	info.deactivate = [](void *data) {
		static_cast<BrowserSource *>(data)->SetActive(false);
	};

	obs_register_source(&info);
}

void UnRegisterAnimeSubtitleSource()
{
}

// BrowserSource
void BrowserSource::UpdateAnimeSubtitleSource(obs_data_t *settings)
{
	if (settings) {
		std::string url_;
		std::string key_;
		int style_index_;
		//
		int width_ = 0;
		int height_ = 0;

		bool shutdown_ = false;

		url_ = obs_data_get_string(settings, "url");
		key_ = obs_data_get_string(settings, "skin_key");
		style_index_ = obs_data_get_int(settings, "style_index");

		if (style_list.empty())
		{
			bool success = GetStyleList(style_list);
			if (success) {
				key_ = style_list[0].key;
				url_ = SOOP_AQUA_GET_COMPONENT + key_;
				obs_data_set_string(settings, "url", url_.c_str());
				obs_data_set_string(settings, "skin_key", key_.c_str());
				//
				success = GetAnimationSubtitleInfo(key_.c_str(), anime_subtitle_info);
				if(success)
					SetAnimeSubtitleInfo(settings, anime_subtitle_info);
			}
		}

		if (style_index_ != style_index) {
			if (!style_list.empty()) {
				key_ = style_list[style_index_].key;
				url_ = SOOP_AQUA_GET_COMPONENT + key_;
				obs_data_set_string(settings, "url", url_.c_str());
				obs_data_set_string(settings, "skin_key", key_.c_str());
				//
				bool success = GetAnimationSubtitleInfo(key_.c_str(), anime_subtitle_info);
				if (success)
					SetAnimeSubtitleInfo(settings, anime_subtitle_info);

				style_index = style_index_;
			}
			else {
				url_ = "about:blank";
			}
		}

		//
		width_ = (int)obs_data_get_int(settings, "width");
		height_ = (int)obs_data_get_int(settings, "height");

		shutdown_ = obs_data_get_bool(settings, "shutdown");

		if (url_ == url && shutdown_ == shutdown_on_invisible) {
			if (width_ == width && height_ == height)
				return;

			width = width_;
			height = height_;

			ExecuteOnBrowser(
				[=](CefRefPtr<CefBrowser> cefBrowser) {
					const CefSize cefSize(width, height);
					cefBrowser->GetHost()
						->GetClient()
						->GetDisplayHandler()
						->OnAutoResize(cefBrowser,
							       cefSize);
					cefBrowser->GetHost()->WasResized();
					cefBrowser->GetHost()->Invalidate(
						PET_VIEW);
				},
				true);
			return;
		}

		url = url_;
		//
		width = width_;
		height = height_;

		shutdown_on_invisible = shutdown_;

		obs_source_set_audio_active(source, reroute_audio);
	}

	DestroyBrowser();
	DestroyTextures();
#if CHROME_VERSION_BUILD < 4103
	ClearAudioStreams();
#endif
	if (!shutdown_on_invisible || obs_source_showing(source))
		create_browser = true;

	first_update = false;
};
