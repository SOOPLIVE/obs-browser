
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
#include <qfileinfo.h>

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

// use platform-windows.cpp
static inline bool check_path(const char* data, const char* path,
	std::string& output)
{
	std::ostringstream str;
	str << path << data;
	output = str.str();

	blog(LOG_DEBUG, "Attempted path: %s", output.c_str());

	return os_file_exists(output.c_str());
}

bool GetDataFilePath(const char* data, std::string& output)
{
	if (check_path(data, "data/obs-studio/", output))
		return true;

	return check_path(data, OBS_DATA_PATH "/obs-studio/", output);
}

//
obs_properties_t* painter_source_get_properties(void* data)
{
	obs_properties_t* props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	BrowserSource* bs = static_cast<BrowserSource*>(data);
	UNUSED_PARAMETER(bs);

	obs_property_t* p = obs_properties_add_button(
		props, "refreshnocache", obs_module_text("PainterSource.AllClear"),
		[](obs_properties_t *, obs_property_t *, void *data) {

			BrowserSource* bs = static_cast<BrowserSource*>(data);
			obs_data_t* settings = obs_source_get_settings(bs->source);

			int tool_ = obs_data_get_int(settings, "tool");
			int r_ = obs_data_get_int(settings, "line_color_r");
			int g_ = obs_data_get_int(settings, "line_color_g");
			int b_ = obs_data_get_int(settings, "line_color_b");
			int thickness_ = obs_data_get_int(settings, "thickness");

			std::string _script = "updatePainterInfo";
			_script += "(";
			_script += std::to_string(tool_);
			_script += ",";
			_script += std::to_string(r_);
			_script += ",";
			_script += std::to_string(g_);
			_script += ",";
			_script += std::to_string(b_);
			_script += ",";
			_script += std::to_string(thickness_);
			_script += ")";

			bs->script = _script;
			bs->Refresh();
			return false;
		});
	obs_property_set_label_text(p, obs_module_text("PainterSource.Clear"));

	p = obs_properties_add_button(
		props, "undo_painter", obs_module_text("UndoPainter"),
		[](obs_properties_t*, obs_property_t*, void* data) {

			std::string _script = "undo()";
			static_cast<BrowserSource*>(data)->ExcuteJavaScript(_script);
			return false;
		});
	obs_property_set_visible(p, false);

	p = obs_properties_add_button(
		props, "redo_painter", obs_module_text("RedoPainter"),
		[](obs_properties_t*, obs_property_t*, void* data) {
			std::string _script = "redo()";
			static_cast<BrowserSource*>(data)->ExcuteJavaScript(_script);
			return false;
		});
	obs_property_set_visible(p, false);

	p = obs_properties_add_button(
		props, "update_random_linecolor", obs_module_text("UpdateRandomLineColor"),
		[](obs_properties_t*, obs_property_t*, void* data) {
			std::string _script = "setPenRandomColor()";
			static_cast<BrowserSource*>(data)->ExcuteJavaScript(_script);
			return false;
		});
	obs_property_set_visible(p, false);

	p = obs_properties_add_button(
		props, "update_tool", obs_module_text("UpdateTool"),
		[](obs_properties_t*, obs_property_t*, void* data) {

			BrowserSource* bs = static_cast<BrowserSource*>(data);
			obs_data_t* settings = obs_source_get_settings(bs->source);

			int tool_ = obs_data_get_int(settings, "tool");

			std::string _script = "selectTool";
			_script += "(";
			_script += std::to_string(tool_);
			_script += ")";
			static_cast<BrowserSource*>(data)->ExcuteJavaScript(_script);
			return false;
		});
	obs_property_set_visible(p, false);

	p = obs_properties_add_button(
		props, "update_linecolor", obs_module_text("UpdateLineColor"),
		[](obs_properties_t*, obs_property_t*, void* data) {

			BrowserSource* bs = static_cast<BrowserSource*>(data);
			obs_data_t* settings = obs_source_get_settings(bs->source);

			int r_ = obs_data_get_int(settings, "line_color_r");
			int g_ = obs_data_get_int(settings, "line_color_g");
			int b_ = obs_data_get_int(settings, "line_color_b");

			std::string _script = "selectLineColor";
			_script += "(";
			_script += std::to_string(r_);
			_script += ",";
			_script += std::to_string(g_);
			_script += ",";
			_script += std::to_string(b_);
			_script += ")";

			static_cast<BrowserSource*>(data)->ExcuteJavaScript(_script);
			return false;
		});
	obs_property_set_visible(p, false);


	p = obs_properties_add_button(
		props, "update_thickness", obs_module_text("UpdateThickness"),
		[](obs_properties_t*, obs_property_t*, void* data) {

			BrowserSource* bs = static_cast<BrowserSource*>(data);
			obs_data_t* settings = obs_source_get_settings(bs->source);

			int thickness_ = obs_data_get_int(settings, "thickness");

			std::string _script = "selectThickness";
			_script += "(";
			_script += std::to_string(thickness_);
			_script += ")";

			static_cast<BrowserSource*>(data)->ExcuteJavaScript(_script);
			return false;
		});
	obs_property_set_visible(p, false);

	return props;
}

extern void soop_browser_source_get_defaults(obs_data_t* settings);
static const char *default_css = "body { \
					background-color: rgba(0, 0, 0, 0); \
					margin: 0px auto; \
					overflow: hidden; \
					}";

void painter_source_get_defaults(obs_data_t *settings)
{
	soop_browser_source_get_defaults(settings);

	const char* painter_path = obs_module_file("painter/canvas.html");
	QString painterDirAbs = QFileInfo(QString::fromUtf8(painter_path)).absoluteFilePath();

	obs_data_set_default_string(settings, "url", painterDirAbs.toStdString().c_str());

	video_t* video = obs_get_video();
	const struct video_output_info* info = video_output_get_info(video);

	obs_data_set_default_int(settings, "width", info->width);
	obs_data_set_default_int(settings, "height", info->height);
	obs_data_set_default_string(settings, "css", default_css);

	obs_data_set_default_int(settings, "line_color_r", 255);
	obs_data_set_default_int(settings, "line_color_g", 36);
	obs_data_set_default_int(settings, "line_color_b", 36);
	obs_data_set_default_int(settings, "thickness", 4);
	obs_data_set_default_int(settings, "tool", 0);
}

extern "C" EXPORT void obs_browser_initialize(void);

// BrowserSource
void BrowserSource::UpdatePainterSource(obs_data_t *settings)
{
	if (settings) {
		std::string url_ = obs_data_get_string(settings, "url");
		int width_ = (int)obs_data_get_int(settings, "width");
		int height_ = (int)obs_data_get_int(settings, "height");

		int tool_ = obs_data_get_int(settings, "tool");
		int r_ = obs_data_get_int(settings, "line_color_r");
		int g_ = obs_data_get_int(settings, "line_color_g");
		int b_ = obs_data_get_int(settings, "line_color_b");
		int thickness_ = obs_data_get_int(settings, "thickness");

		std::string _script = "updatePainterInfo";
		_script += "(";
		_script += std::to_string(tool_);
		_script += ",";
		_script += std::to_string(r_);
		_script += ",";
		_script += std::to_string(g_);
		_script += ",";
		_script += std::to_string(b_);
		_script += ",";
		_script += std::to_string(thickness_);
		_script += ")";

		script = _script;
		ExcuteJavaScript(script);

		if (url_ == url)
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

void RegisterPainterSource()
{
	struct obs_source_info info = {};
	info.id = "painter_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
		OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB;
	info.get_properties = painter_source_get_properties;
	info.get_defaults = painter_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;

	info.get_name = [](void*) { return obs_module_text("PainterSource.GetName"); };
	info.create = [](obs_data_t* settings,
		obs_source_t* source) -> void* {
			obs_browser_initialize();
			return new BrowserSource(settings, source);
	};
	info.destroy = [](void* data) {
		static_cast<BrowserSource*>(data)->Destroy();
	};
	info.update = [](void* data, obs_data_t* settings) {
		static_cast<BrowserSource*>(data)->UpdatePainterSource(
			settings);
	};
	info.get_width = [](void* data) {
		return (uint32_t) static_cast<BrowserSource*>(data)
			->width;
	};
	info.get_height = [](void* data) {
		return (uint32_t) static_cast<BrowserSource*>(data)
			->height;
	};
	info.video_tick = [](void* data, float) {
		static_cast<BrowserSource*>(data)->Tick();
	};
	info.video_render = [](void* data, gs_effect_t*) {
		static_cast<BrowserSource*>(data)->Render();
	};
#if CHROME_VERSION_BUILD < 4103
	info.audio_mix = [](void* data, uint64_t* ts_out,
		struct audio_output_data* audio_output,
		size_t channels, size_t sample_rate) {
			return static_cast<BrowserSource*>(data)->AudioMix(
				ts_out, audio_output, channels, sample_rate);
	};
	info.enum_active_sources = [](void* data,
		obs_source_enum_proc_t cb,
		void* param) {
			static_cast<BrowserSource*>(data)->EnumAudioStreams(
				cb, param);
	};
#endif
	info.mouse_click = [](void* data,
		const struct obs_mouse_event* event,
		int32_t type, bool mouse_up,
		uint32_t click_count) {
			static_cast<BrowserSource*>(data)->SendMouseClick(
				event, type, mouse_up, click_count);
	};
	info.mouse_move = [](void* data,
		const struct obs_mouse_event* event,
		bool mouse_leave) {
			static_cast<BrowserSource*>(data)->SendMouseMove(
				event, mouse_leave);
	};
	info.mouse_wheel = [](void* data,
		const struct obs_mouse_event* event,
		int x_delta, int y_delta) {
			static_cast<BrowserSource*>(data)->SendMouseWheel(
				event, x_delta, y_delta);
	};
	info.focus = [](void* data, bool focus) {
		static_cast<BrowserSource*>(data)->SendFocus(focus);
	};
	info.key_click = [](void* data,
		const struct obs_key_event* event,
		bool key_up) {
			static_cast<BrowserSource*>(data)->SendKeyClick(
				event, key_up);
	};
	info.show = [](void* data) {
		static_cast<BrowserSource*>(data)->SetShowing(true);
	};
	info.hide = [](void* data) {
		static_cast<BrowserSource*>(data)->SetShowing(false);
	};
	info.activate = [](void* data) {
		BrowserSource* bs = static_cast<BrowserSource*>(data);
		if (bs->restart)
			bs->Refresh();
		bs->SetActive(true);
	};
	info.deactivate = [](void* data) {
		static_cast<BrowserSource*>(data)->SetActive(false);
	};

	obs_register_source(&info);
}

void UnRegisterPainterSource()
{

}
