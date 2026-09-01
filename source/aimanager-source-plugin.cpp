
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
obs_properties_t* aimanager_source_get_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	BrowserSource* bs = static_cast<BrowserSource*>(data);
	UNUSED_PARAMETER(bs);

	obs_properties_add_button(
		props, "refreshnocache", obs_module_text("RefreshNoCache"),
		[](obs_properties_t *, obs_property_t *, void *data) {
			static_cast<BrowserSource *>(data)->Refresh();
			return false;
		});

	return props;
}

extern void soop_browser_source_get_defaults(obs_data_t* settings);
static const char *default_css = "body { \
					background-color: rgba(0, 0, 0, 0); \
					margin: 0px auto; \
					overflow: hidden; \
					}";
void aimanager_source_get_defaults(obs_data_t *settings)
{
	soop_browser_source_get_defaults(settings);

	obs_data_set_default_string(settings, "css", default_css);
}

extern "C" EXPORT void obs_browser_initialize(void);
//
void RegisterAIManager()
{
	struct obs_source_info info = {};
	info.id = "soop_aimanager_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
		OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
		OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;
	info.get_properties = aimanager_source_get_properties;
	info.get_defaults = aimanager_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void *) { return obs_module_text("Dowoomi.GetName.AIManager"); };
	info.create = [](obs_data_t *settings,
			 obs_source_t *source) -> void * {
		obs_browser_initialize();
		return new BrowserSource(settings, source);
	};
	info.destroy = [](void *data) {
		static_cast<BrowserSource *>(data)->Destroy();
	};
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->UpdateAIManagerSource(
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
	};
	info.deactivate = [](void *data) {
		static_cast<BrowserSource *>(data)->SetActive(false);
	};

	obs_register_source(&info);
}

void UnRegisterAIManager() {}

// BrowserSource
void BrowserSource::UpdateAIManagerSource(obs_data_t *settings)
{
	if (settings) {
		std::string url_;
		//
		int width_ = 0;
		int height_ = 0;
		bool reroute_audio_ = false;
		std::string streamer_id;
		std::string broadNum = "0";
		std::string theme;

		url_ = obs_data_get_string(settings, "url");
		//theme = obs_data_get_string(settings, "theme");
		//url_ += "/" + theme;
		//streamer_id = obs_data_get_string(settings, "streamer_id");
		//broadNum = obs_data_get_string(settings, "broadNum");
		//url_ += "?streamer_id=" + streamer_id + "&broad_no=" + broadNum;

		width_ = (int)obs_data_get_int(settings, "width");
		height_ = (int)obs_data_get_int(settings, "height");
		reroute_audio_ = obs_data_get_bool(settings, "reroute_audio");

		if (url_ == url && reroute_audio_ == reroute_audio)
		{
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

		//
		url = url_;
		width = width_;
		height = height_;
		reroute_audio = reroute_audio_;

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
