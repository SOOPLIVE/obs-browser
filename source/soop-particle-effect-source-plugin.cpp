
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

#include "soop-source-plugin.h"

std::string GetParticleUrl(std::string paricleType)
{
	return std::string(PATICLE_URL) + paricleType + ".html";;
}

static bool effect_type_changed(obs_properties_t* props, obs_property_t* prop,
	obs_data_t* settings)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(prop);

	obs_data_set_string(settings, "url", "");

	return true;
}

obs_properties_t *soop_particle_effect_source_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	BrowserSource *bs = static_cast<BrowserSource *>(data);
	UNUSED_PARAMETER(bs);

	obs_properties_add_int(props, "width", obs_module_text("Width"), 1,
			       8192, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1,
			       8192, 1);

	obs_property_t *effect_type = obs_properties_add_list(props, "SETTING_PARTICLE_EFFECT_ID",
							      "Effect Type",
							      OBS_COMBO_TYPE_LIST,
							      OBS_COMBO_FORMAT_STRING);
	obs_property_set_modified_callback(effect_type, effect_type_changed);

	obs_properties_add_button(
		props, "refreshnocache", obs_module_text("RefreshNoCache"),
		[](obs_properties_t*, obs_property_t*, void* data) {
			static_cast<BrowserSource*>(data)->Refresh();
			return false;
		});

	return props;
}

static const char *default_css = "body { \
					background-color: rgba(0, 0, 0, 0); \
					margin: 0px auto; \
					overflow: hidden; \
					}";

void soop_particle_effect_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", 960);
	obs_data_set_default_int(settings, "height", 540);
	obs_data_set_default_int(settings, "fps", 30);
#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	obs_data_set_default_bool(settings, "fps_custom", false);
#else
	obs_data_set_default_bool(settings, "fps_custom", true);
#endif
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", true);
	obs_data_set_default_int(settings, "webpage_control_level",
				(int)DEFAULT_CONTROL_LEVEL);
	obs_data_set_default_string(settings, "css", default_css);
	obs_data_set_default_bool(settings, "reroute_audio", false);
}

extern "C" EXPORT void obs_browser_initialize(void);
//

void RegisterSOOPParticleEffectSource()
{
	struct obs_source_info info = {};
	info.id = "soop_particle_effect_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW |
			    OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;
	//
	info.get_properties = soop_particle_effect_source_get_properties;
	info.get_defaults = soop_particle_effect_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void *) { return obs_module_text("SoopParticleEffect.Source"); };
	info.create = [](obs_data_t *settings,
			 obs_source_t *source) -> void * {
		obs_browser_initialize();
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_AUTO);
		return new BrowserSource(settings, source);
	};
	info.destroy = [](void *data) {
		static_cast<BrowserSource *>(data)->Destroy();
	};
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->Update(
			settings);
	};
	info.get_width = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->width;
	};
	info.get_height = [](void *data) {
		return (uint32_t) static_cast<BrowserSource *>(data)->height;
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

void UnRegisterSOOPParticleEffectSource() {}
