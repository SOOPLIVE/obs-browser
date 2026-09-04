#pragma once

#include <string>

// aqua source api
#define SOOP_AQUA_DEFAULT_URL	""

#define SOOP_AQUA_CALL_API	""
#define SOOP_AQUA_GET_COMPONENT	""

#define SOOP_AQUA_MENU		""
#define SOOP_AQUA_CALL_API	""
#define SOOP_AQUA_GET_COMPONENT	""

// api query string
#define AQUA_CHAT_STYLE_LIST_QUERY	""
#define AQUA_SCORE_STYLE_LIST_QUERY     ""
#define AQUA_ANMATION_SUBTITLE_STYLE_LIST_QUERY ""
#define AQUA_SCORE_INFO_QUERY		""

// kbo api
#define URL_GRAPHICBROAD_LIST	""

// football api
#define URL_GRAPHICBROAD_LIST_FOOTBALL ""
//
#define URL_COMMERCE_SETTING	""
#define URL_COMMERCE		""

// particle source
#define PATICLE_URL		""


#define URL_VIDEOBALLOON_SOURCE	""

// mood check
//#define URL_CHATMOODCHECK	""
#define URL_CHATMOODCHECK	""

// chat source type
extern const int chatSourceCount;

extern void RegisterChatSource(int idx);
extern void UnRegisterChatSource();

// chat score source type
extern void RegisterChatScoreSource();
extern void UnRegisterChatScoreSource();

// commerce
const int commerce_source_type_count = 2;

extern void RegisterCommerceSource(int idx);
extern void UnRegisterCommerceSource();

// kbo graphic
extern const int kboSourceCount;

extern void RegisterKBOGraphicSource(int type);
extern void UnRegisterKBOGraphicSource();

// football graphic
extern const int footballSourceCount;

extern void RegisterFootballGraphicSource(int type);
extern void UnRegisterFootballGraphicSource();

// mission (battle_joinusers, battle_fundingrank)
const int mission_source_type_count = 3;

extern void RegisterMissionSource(int idx);
extern void UnRegisterMissionSource();

// video balloon
extern void RegisterVideoBalloonSource();
extern void UnRegisterVideoBalloonSource();

// particle source
extern void RegisterSOOPParticleEffectSource();
extern void UnRegisterSOOPParticleEffectSource();

// mood check
extern void RegisterChatMoodCheckSource();
extern void UnRegisterChatMoodCheckSource();

// AI Manager
extern void RegisterAIManager();
extern void UnRegisterAIManager();

// painter source
extern void RegisterPainterSource();
extern void UnRegisterPainterSource();

// animatin subtitle
extern void RegisterAnimeSubtitleSource();
extern void UnRegisterAnimeSubtitleSource();
