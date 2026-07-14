
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
static void TimerQuery(void *data, const char *action)
{
}
static void UpdateScoreInfoQuery(void *data)
{
}
static bool GetStyleList(StyleList& list)
{
	return false;
}
static bool GetScoreInfo(const char *key, ScoreInfo& info)
{
	return false;
}
static void SetScoreInfo(obs_data_t *settings, const ScoreInfo &info)
{
	obs_data_set_string(settings, "score_blue_name", info.left_name.c_str());
	obs_data_set_string(settings, "score_red_name", info.right_name.c_str());
	obs_data_set_int(settings, "score_blue_num", info.left_score);
	obs_data_set_int(settings, "score_red_num", info.right_score);
	obs_data_set_bool(settings, "score_set", info.use_set);
	obs_data_set_int(settings, "score_blue_set", info.left_set);
	obs_data_set_int(settings, "score_red_set", info.right_set);
	obs_data_set_bool(settings, "score_timer", info.use_timer);
	obs_data_set_int(settings, "skin_idx", info.thema);

}
static void update_score_info(BrowserSource *source)
{
	if(!source)
		return;
	/*
	nlohmann::json json {
		{"result", "1"},
		{
			"data",
			{"szStyleName", QObject::tr(info.styleName.c_str()).toUtf8().constData()},
			{"nStyleIdx",info.styleIdx},
			{"nTransparency",info.transparency},
			{"nThema",info.thema},
			{"nLeftScore",info.leftScore},
			{"nRightScore",info.rightScore},
			{"bUseSetscore",(info.useSet?"true":"false")},
			{"nLeftSetscore",info.leftSet},
			{"nRightSetscore",info.rightSet},
			{"bUseGameTime",(info.useTimer?"true":"false")},
			{"szLeftTeamName", QObject::tr(info.leftName.c_str()).toUtf8().constData()},
			{"szRightTeamName", QObject::tr(info.rightName.c_str()).toUtf8().constData()}
		}
	};
*/
	ScoreInfo info = source->score_info;
	//
	nlohmann::json jsonData;
	jsonData["szStyleName"] =
		QObject::tr(info.style_name.c_str()).toUtf8().constData();
	jsonData["nStyleIdx"] = info.styleIdx;
	jsonData["nTransparency"] = info.transparency;
	jsonData["nThema"] = info.thema;
	jsonData["nLeftScore"] = info.left_score;
	jsonData["nRightScore"] = info.right_score;
	jsonData["bUseSetscore"] = (info.use_set ? true : false);
	jsonData["nLeftSetscore"] = info.left_set;
	jsonData["nRightSetscore"] = info.right_set;
	jsonData["bUseGameTime"] = (info.use_timer ? true : false);
	jsonData["szLeftTeamName"] =
		QObject::tr(info.left_name.c_str()).toUtf8().constData();
	jsonData["szRightTeamName"] =
		QObject::tr(info.right_name.c_str()).toUtf8().constData();
	nlohmann::json json;
	json["data"] = jsonData;
	json["result"] = "1";
	//
	std::string jsonStr = json.dump();
	blog(LOG_DEBUG, "json dump: %s", jsonStr.c_str());
	//
	if (source && source->cefBrowser) {
		CefRefPtr<CefFrame> frame =
			(source->cefBrowser)->GetMainFrame();
		std::string script = "SetAquaConfigFreecShot";
		script += "('" + json.dump() + "')";
		frame->ExecuteJavaScript(script, frame->GetURL(), 0);
	}
}
//
static bool play_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	TimerQuery(data, "startTimer");

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}
static bool pause_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	TimerQuery(data, "pauseTimer");

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}
static bool reset_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	TimerQuery(data, "resetTimer");

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}
static bool clear_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	//TimerQuery(data, "resetTimer");
	//
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	ScoreInfo &info = bs->score_info;
	info.left_name = obs_module_text("Dowoomi.Score.Default.LeftTeamName");
	info.right_name = obs_module_text("Dowoomi.Score.Default.RightTeamName");
	info.left_score = 0;
	info.right_score = 0;
	info.use_set = true;
	info.left_set = 0;
	info.right_set = 0;
	//info.use_timer = true;
	//info.thema = 1;
	UpdateScoreInfoQuery(data);

	obs_data_t *settings = obs_source_get_settings(bs->source);
	obs_data_set_string(settings, "score_blue_name", info.left_name.c_str());
	obs_data_set_string(settings, "score_red_name", info.right_name.c_str());
	obs_data_set_int(settings, "score_blue_num", info.left_score);
	obs_data_set_int(settings, "score_red_num", info.right_score);
	obs_data_set_bool(settings, "score_set", info.use_set);
	obs_data_set_int(settings, "score_blue_set", info.left_set);
	obs_data_set_int(settings, "score_red_set", info.right_set);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return true;
}
//
static void set_skin_info(int idx, void* data)
{
	BrowserSource *bs = static_cast<BrowserSource *>(data);
	ScoreInfo &info = bs->score_info;
	info.thema = idx + 1;
	UpdateScoreInfoQuery(data);

	obs_data_t *settings = obs_source_get_settings(bs->source);
	std::string key_ = bs->style_list[idx].key;
	obs_data_set_string(settings, "skin_key", key_.c_str());
}
static bool skin0_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	set_skin_info(0, data);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}
static bool skin1_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	set_skin_info(1, data);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}
static bool skin2_button_clicked(obs_properties_t *props,
				 obs_property_t *property, void *data)
{
	set_skin_info(2, data);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	return false;
}
//
static bool change_score_info(void *data, obs_properties_t *props,
			      obs_property_t *property, obs_data_t *settings)
{
	return false;
}
//
obs_properties_t *chat_score_source_get_properties(void *data)
{
    UNUSED_PARAMETER(data);
    
	obs_properties_t *props = obs_properties_create();
	//
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	// setting
	obs_properties_add_button(props, "score_clear",
				  obs_module_text("ScoreClear"),
				  clear_button_clicked);
	// 
	obs_properties_t *sub_props = obs_properties_create();
	obs_property_t *prop = obs_properties_add_text(
		sub_props, "score_blue_name", obs_module_text("ScoreBlueName"),
		OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(prop, change_score_info, data);
	prop = obs_properties_add_text(sub_props, "score_red_name",
				       obs_module_text("ScoreRedName"),
				       OBS_TEXT_DEFAULT);
	obs_property_set_modified_callback2(prop, change_score_info, data);
	obs_properties_add_group(props, "score_name",
				 obs_module_text("ScoreName"), OBS_GROUP_NORMAL,
				 sub_props);
	// 
	sub_props = obs_properties_create();
	prop = obs_properties_add_int(sub_props, "score_blue_num",
				      obs_module_text("ScoreBlueNum"), 0, 99,
				      1);
	obs_property_set_modified_callback2(prop, change_score_info, data);
	prop = obs_properties_add_int(sub_props, "score_red_num",
				      obs_module_text("ScoreRedNum"), 0, 99, 1);
	obs_property_set_modified_callback2(prop, change_score_info, data);
	obs_properties_add_group(props, "score_num",
				 obs_module_text("ScoreNum"), OBS_GROUP_NORMAL,
				 sub_props);
	//
	sub_props = obs_properties_create();
	prop = obs_properties_add_int(sub_props, "score_blue_set",
				      obs_module_text("ScoreBlueSet"), 0, 99,
				      1);
	obs_property_set_modified_callback2(prop, change_score_info, data);
	prop = obs_properties_add_int(sub_props, "score_red_set",
				      obs_module_text("ScoreRedSet"), 0, 99, 1);
	obs_property_set_modified_callback2(prop, change_score_info, data);
	prop = obs_properties_add_group(props, "score_set",
					obs_module_text("ScoreSet"),
					OBS_GROUP_CHECKABLE, sub_props);
	obs_property_set_modified_callback2(prop, change_score_info, data);

	//
	sub_props = obs_properties_create();
	obs_properties_add_button(sub_props, "score_timer_play",
				  obs_module_text("ScoreTimerPlay"),
				  play_button_clicked);
	obs_properties_add_button(sub_props, "score_timer_pause",
				  obs_module_text("ScoreTimerPause"),
				  pause_button_clicked);
	obs_properties_add_button(sub_props, "score_timer_reset",
				  obs_module_text("ScoreTimerReset"),
				  reset_button_clicked);
	prop = obs_properties_add_group(props, "score_timer",
					obs_module_text("ScoreTimer"),
					OBS_GROUP_CHECKABLE, sub_props);
	obs_property_set_modified_callback2(prop, change_score_info, data);

	//
	sub_props = obs_properties_create();
	obs_properties_add_button(sub_props,
				  "score_skin_0",
				  obs_module_text("ScoreSkin0"),
				  skin0_button_clicked);
	obs_properties_add_button(sub_props, "score_skin_1",
				  obs_module_text("ScoreSkin1"),
				  skin1_button_clicked);
	obs_properties_add_button(sub_props, "score_skin_2",
				  obs_module_text("ScoreSkin2"),
				  skin2_button_clicked);
	prop = obs_properties_add_int(sub_props, "skin_idx",
				      "", 1, 3,
				      1);
	obs_property_set_visible(prop, false);
	obs_properties_add_group(props, "score_skin",
				 obs_module_text("ScoreSkin"), OBS_GROUP_NORMAL,
				 sub_props);

	prop = obs_properties_add_text(props, "skin_key", "",
				       obs_text_type::OBS_TEXT_DEFAULT);
	obs_property_set_visible(prop, false);

	prop = obs_properties_add_button(
		props, "update_score_info", "",
		[](obs_properties_t *, obs_property_t *, void *data) {
			BrowserSource *bs = static_cast<BrowserSource *>(data);
			obs_data_t *settings = obs_source_get_settings(bs->source);
			change_score_info(bs, nullptr, nullptr, settings);
			return false;
		});
	obs_property_set_visible(prop, false);

	prop = obs_properties_add_button(props, "refresh_score_info", "",
	[](obs_properties_t *, obs_property_t *, void *data) {
		BrowserSource *bs = static_cast<BrowserSource *>(data);
		obs_data_t *settings = obs_source_get_settings(bs->source);

		StyleList style_list;
		bool success = GetStyleList(style_list);
		if (success) {
			std::string key_ = style_list[0].key;
			std::string url_ = SOOP_AQUA_GET_COMPONENT + key_;
			obs_data_set_string(settings, "url", url_.c_str());
			obs_data_set_string(settings, "skin_key", key_.c_str());
			//
			ScoreInfo &info = bs->score_info;
			success = GetScoreInfo(key_.c_str(), info);
			if (success)
				SetScoreInfo(settings, info);
		}
		return false;
	});
	obs_property_set_visible(prop, false);
	//
	return props;
}

static const char *default_css = "body { \
					background-color: rgba(0, 0, 0, 0); \
					margin: 0px auto; \
					overflow: hidden; \
					}";
void chat_score_source_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "url", "");
	obs_data_set_default_string(settings, "skin_key", "");
	obs_data_set_default_int(settings, "width", 800);
	obs_data_set_default_int(settings, "height", 600);
	obs_data_set_default_int(settings, "fps", 30);
#ifdef ENABLE_BROWSER_SHARED_TEXTURE
	obs_data_set_default_bool(settings, "fps_custom", false);
#else
	obs_data_set_default_bool(settings, "fps_custom", true);
#endif
	obs_data_set_default_bool(settings, "shutdown", false);
	obs_data_set_default_bool(settings, "restart_when_active", false);
	obs_data_set_default_int(settings, "webpage_control_level", (int)DEFAULT_CONTROL_LEVEL);
	obs_data_set_default_string(settings, "css", default_css);
	obs_data_set_default_bool(settings, "reroute_audio", false);
	obs_data_set_default_string(settings, "score_blue_name", obs_module_text("ScoreBlueName"));
	obs_data_set_default_string(settings, "score_red_name", obs_module_text("ScoreRedName"));
	obs_data_set_default_int(settings, "score_blue_num", 0);
	obs_data_set_default_int(settings, "score_red_num", 0);
	obs_data_set_default_bool(settings, "score_set", true);
	obs_data_set_default_int(settings, "score_blue_set", 0);
	obs_data_set_default_int(settings, "score_red_set", 0);
	obs_data_set_default_bool(settings, "score_timer", true);
	obs_data_set_default_int(settings, "skin_idx", 1);
}

extern "C" EXPORT void obs_browser_initialize(void);
//
void RegisterChatScoreSource()
{
	struct obs_source_info info = {};
	//
	info.id = "soop_chat_source_score";
	info.get_name = [](void *) {
		return obs_module_text("Dowoomi.GetName.Score");
	};
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO |
		OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_INTERACTION |
		OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB |
		OBS_SOURCE_NOT_DRAW_PREVIEW;

	info.get_properties = chat_score_source_get_properties;
	info.get_defaults = chat_score_source_get_defaults;
	info.icon_type = OBS_ICON_TYPE_BROWSER;
	//
	info.create = [](obs_data_t *settings,
			 obs_source_t *source) -> void * {
		obs_browser_initialize();
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_AUTO);
		BrowserSource* bs = new BrowserSource(settings, source);

		StyleList style_list;
		bool success = GetStyleList(style_list);
		if (success) {
			std::string key_ = style_list[0].key;
			std::string url_ = SOOP_AQUA_GET_COMPONENT + key_;
			obs_data_set_string(settings, "url", url_.c_str());
			obs_data_set_string(settings, "skin_key", key_.c_str());
			//
			ScoreInfo& info = bs->score_info;
			success = GetScoreInfo(key_.c_str(), info);
			if (success)
				SetScoreInfo(settings, info);
		}

		return bs;
	};
	info.destroy = [](void *data) {
		static_cast<BrowserSource *>(data)->Destroy();
	};
	info.update = [](void *data, obs_data_t *settings) {
		static_cast<BrowserSource *>(data)->UpdateChatScoreSource(
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

void UnRegisterChatScoreSource()
{
}

// BrowserSource
void BrowserSource::UpdateChatScoreSource(obs_data_t *settings)
{
	if (settings) {
		std::string url_;
		std::string key_;
		//
		int width_ = 0;
		int height_ = 0;

		bool shutdown_ = false;

		url_ = obs_data_get_string(settings, "url");
		key_ = obs_data_get_string(settings, "skin_key");
		if (style_list.empty())
		{
			bool success = GetStyleList(style_list);
			if (success) {
				key_ = style_list[0].key;
				url_ = SOOP_AQUA_GET_COMPONENT + key_;
				obs_data_set_string(settings, "url", url_.c_str());
				obs_data_set_string(settings, "skin_key", key_.c_str());
				//
				success = GetScoreInfo(key_.c_str(), score_info);
				if(success)
					SetScoreInfo(settings, score_info);
			}
		}

		if (style_list.empty()) {
			url_ = "about:blank";
		}

		// Recv ToolBar Setting
		{
			if (score_info.left_score != obs_data_get_int(settings, "score_blue_num") ||
				score_info.right_score != obs_data_get_int(settings, "score_red_num") ||
				score_info.left_set != obs_data_get_int(settings, "score_blue_set") ||
				score_info.right_set != obs_data_get_int(settings, "score_red_set"))
			{
				score_info.left_score = obs_data_get_int(settings, "score_blue_num");
				score_info.right_score = obs_data_get_int(settings, "score_red_num");
				score_info.left_set = obs_data_get_int(settings, "score_blue_set");
				score_info.right_set = obs_data_get_int(settings, "score_red_set");

				UpdateScoreInfoQuery(this);
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
