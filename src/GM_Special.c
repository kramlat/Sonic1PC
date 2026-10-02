#include "GM_Special.h"

#include "Game.h"
#include "Console.h"
#include "GM_Level.h"
#include "SpecialStage.h"
#include "Level.h"
#include "LevelScroll.h"
#include "LevelDraw.h"
#include "LevelCollision.h"
#include "Video.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "PLC.h"
#include "Nemesis.h"
#include "Demo.h"
#include "Object/Sonic.h"
#include "Sound.h"
#include "HUD.h"
#include "Object/SpecialResult.h"

#include <string.h>

//Special stage gamemode
static void SS_RunStage(void) {
	//Fade out
	PlaySound(sfx_EnterSS);
	PaletteWhiteOut();
	
	//Reset screen
	ClearScreen();
	VDP_SeekVRAM(0x5000);
	VDP_FillVRAM(0, 0x7000);
	
	//Load the background first, then the art: the block art sits in VRAM areas that overlap the background's
	//canvases, and has to win (the original loads them in this order too)
	VDP_SetPlaneSize(64, 64); //64x64 planes: the background switches between canvases by moving them around in VRAM
	SS_BGLoad();
	QuickPLC(PlcId_SpecialStage);
	
	//Clear object memory
	memset(objects, 0, sizeof(objects));
	
	//Clear F700 to F800
	scrpos_x.v = 0;
	scrpos_y.v = 0;
	bg_scrpos_x.v = 0;
	bg_scrpos_y.v = 0;
	bg2_scrpos_x.v = 0;
	bg2_scrpos_y.v = 0;
	bg3_scrpos_x.v = 0;
	bg3_scrpos_y.v = 0;
	
	limit_left1 = 0;
	limit_right1 = 0;
	limit_top1 = 0;
	limit_btm1 = 0;
	limit_left2 = 0;
	limit_right2 = 0;
	limit_top2 = 0;
	limit_btm2 = 0;
	limit_left3 = 0;
	
	scrshift_x = 0;
	scrshift_y = 0;
	
	look_shift = 0;
	dle_routine = 0;
	nobgscroll = false;
	
	fg_xblock = 0;
	fg_yblock = 0;
	bg1_xblock = 0;
	bg1_yblock = 0;
	bg2_xblock = 0;
	bg2_yblock = 0;
	bg3_xblock = 0;
	bg3_yblock = 0;
	
	fg_scroll_flags = 0;
	bg1_scroll_flags = 0;
	bg2_scroll_flags = 0;
	bg3_scroll_flags = 0;
	bgscrollvert = false;
	sonspeed_max = 0;
	sonspeed_acc = 0;
	sonspeed_dec = 0;
	sonframe_num = 0;
	sonframe_chg = 0;
	angle_buffer0 = 0;
	angle_buffer1 = 0;
	
	opl_routine = 0;
	opl_screen = 0;
	opl_ptr0 = NULL;
	opl_ptr4 = NULL;
	opl_ptr8 = NULL;
	opl_ptrC = NULL;
	
	ss_angle.v = 0;
	ss_rotate = 0;
	btn_pushtime1 = 0;
	btn_pushtime2 = 0;
	pal_chgspeed = 0;
	memset(coll_index, 0, sizeof(coll_index));
	palss_num = 0;
	palss_time = 0;
	
	btn_pushtime1 = 0;
	btn_pushtime2 = 0;
	obj31_ypos = 0;
	boss_status = 0;
	track_pos.v = 0;
	lock_screen = 0;
	memset(level_schunks, 0, sizeof(level_schunks));
	memset(level_anim, 0, sizeof(level_anim));
	gfx_big_ring = 0;
	convey_rev = 0;
	memset(obj63, 0, sizeof(obj63));
	tunnel_mode = 0;
	lock_multi = 0;
	tunnel_allow = 0;
	jump_only = 0;
	obj6B = 0;
	lock_ctrl = false;
	big_ring = 0;
	item_bonus = 0;
	time_bonus = 0;
	ring_bonus = 0;
	endact_bonus = 0;
	sonicend = 0;
	lz_deform = 0;
	memset(f_switch, 0, sizeof(f_switch));
	
	scroll_block1_size = 0;
	scroll_block2_size = 0;
	scroll_block3_size = 0;
	scroll_block4_size = 0;
	
	//FE60 to FF00
	memset(oscillatory.state, 0, sizeof(oscillatory.state));
	memset(sprite_anim, 0, sizeof(sprite_anim));
	sprite_anim_3buf = 0;
	
	limit_top_db = 0;
	limit_btm_db = 0;
	
	//Clear Nemesis buffer
	memset(nemesis_buffer, 0, sizeof(nemesis_buffer));
	
	//Clear other memory
	VDPDisableWaterSplit(); // also clears wtr_state -- special stages are entered straight from levels (incl. LZ)
	restart = false;
	
	//Load special stage palette and layout
	PalLoad1(PalId_Special);
	SS_Load();
	
	//Initialize special stage
	scrpos_x.v = 0;
	scrpos_y.v = 0;
	
	player->type = ObjId_SpecialSonic;
	PCycle_SS();
	
	ss_angle.v = 0;
	ss_rotate = 0x0040;
	//The special stage music never speeds up in Sonic 1 (only Sonic 3 does that): drop any speed-shoes tempo carried over
	//from the level, so neither this song nor the next level's starts sped up
	SlowDownMusic();
	PlayMusic(bgm_SS);
	
	//Start the demo input from its first record (the stage demo is hardcoded to the Special Stage entry)
	btn_pushtime1 = 0;
	btn_pushtime2 = (cli_demo_override ? cli_demo_override : intro_demo_ptr[7])[1] - 1;
	
	rings = 0;
	life_num = 0;
	debug_use = false;
	demo_length = (cli_demo_override && cli_demo_length >= 0) ? (uint16_t)cli_demo_length : 1800;
	
	//Handle debug mode cheat. Debug builds skip the "hold A" requirement
	//too -- debug_cheat alone (itself unconditionally on in debug builds,
	//see GM_Title.c) is enough.
#ifndef NDEBUG
	if (debug_cheat)
		debug_mode = true;
#else
	if (debug_cheat && (jpad1_hold1 & JPAD_A))
		debug_mode = true;
#endif
	
	//Fade in
	PaletteWhiteIn();
	
	//Start special stage loop
	while (1) {
		//Handle pausing the game when pressing Start
		PauseGame();
		ConsoleUpdate(); // the console or a tool window (SMPS Inspector) may have frozen the game

		//Run frame
		vbla_routine = 0x0A;
		WaitForVBla();

		//The mode was changed from outside (e.g. an in-app demo recording request): leave. Sonic's own exit
		//(GameMode_Level, set by the GOAL block) is handled below.
		if ((gamemode & 0x7F) != GameMode_Special && (gamemode & 0x7F) != GameMode_Level)
			return;
		
		MoveSonicInDemo();
		jpad1_hold2  = jpad1_hold1;
		jpad1_press2 = jpad1_press1;
		
		//Run and draw stage
		ExecuteObjects();
		
		uint8_t sprite_i;
		BuildSprites(&sprite_i);
		SS_ShowLayout(sprite_i);
		SS_BGAnimate();

		//End the demo once its timer runs out
		if (demo && !demo_length) {
			gamemode = GameMode_Sega;
			return;
		}

		//Exiting the stage?
		if ((gamemode & 0x7F) != GameMode_Special)
			break;
	}

	//A demo that exits goes back to the Sega screen
	if (demo) {
		gamemode = GameMode_Sega;
		return;
	}

	//Back to the level: the next one (Got Through already picked it); past the last, the first
	gamemode = GameMode_Level;
	if (level_id > LEVEL_ID(ZoneId_SBZ, 2))
		level_id = 0;

	//Fade out to white while the stage spins
	demo_length = 60;
	palette_fade.ind = 0;
	palette_fade.len = 0x40;
	pal_chgspeed = 0;
	do {
		vbla_routine = 0x16;
		WaitForVBla();
		MoveSonicInDemo();
		jpad1_hold2  = jpad1_hold1;
		jpad1_press2 = jpad1_press1;
		ExecuteObjects();
		uint8_t sprite_i;
		BuildSprites(&sprite_i);
		SS_ShowLayout(sprite_i);
		SS_BGAnimate();
		if (--pal_chgspeed < 0) {
			pal_chgspeed = 2;
			WhiteOut_ToWhite();
		}
	} while (demo_length);

	//Results screen
	SS_Results();
}

void GM_Special(void) {
	SS_RunStage();

	//The stage runs the VDP with 64x64 planes that its background moves around; put the usual planes back for
	//whatever comes next (the level, the Sega screen...)
	VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
	VDP_SetPlaneALocation(VRAM_FG);
	VDP_SetPlaneBLocation(VRAM_BG);
	vid_scrpos_y_dup = vid_bg_scrpos_y_dup = 0;
}

//The special stage results screen: score tally, ring bonus, the emeralds collected so far
void SS_Results(void) {
	ClearScreen();
	VDP_SetPlaneALocation(VRAM_FG);
	VDP_SetPlaneBLocation(VRAM_BG);
	VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
	memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
	scrpos_x.v = scrpos_y.v = 0;
	vid_scrpos_y_dup = vid_bg_scrpos_y_dup = 0;

	QuickPLC(PlcId_TitleCard); //the title card font, used by the results text
	HUD_Base();
	PalLoad2(PalId_SSResults); //straight to the displayed palette (PalLoad1 would only set the fade target)
	NewPLC(PlcId_Main);
	AddPLC(PlcId_SSResult);

	score_count = 1; //update the score counter
	endact_bonus = true; //update the ring bonus counter
	ring_bonus = rings * 10; //100 points for each ring collected in the stage
	PlaySound(bgm_GotThrough);

	memset(objects, 0, sizeof(objects));
	objects[SSR_CARD_SLOT].type = ObjId_SSResult;
	restart = false;

	do {
		PauseGame();
		vbla_routine = 0x0C;
		WaitForVBla();
		ExecuteObjects();
		BuildSprites(NULL);
		RunPLC();
	} while (!restart || plc_buffer[0].art != NULL); //the results object signals when it is done

	PlaySound(sfx_EnterSS);
	PaletteWhiteOut();
	restart = false;
}
