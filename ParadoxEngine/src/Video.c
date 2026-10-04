#include "Video.h"

#include "EngineConstants.h"
#include "EnginePalette.h"

#include <string.h>

//Video state
uint8_t vbla_routine;

// The size of the picture. The aspect ratio picks it: the wide ones add columns to the original 320x224, the tall ones rows
// (kept to what the 256-pixel plane holds). Multiples of 8, as the planes are tiles.
int screen_width = 320, screen_height = 224;
int plane_height = 32; // rows of tiles in the planes: 64 once the picture is taller than 224 pixels
static int resolution_mode = RESOLUTION_ORIGINAL;   // the size the picture has now
static int resolution_chosen = RESOLUTION_ORIGINAL; // the size the player chose (menu, settings, command line)
static bool chosen_changed = false;                 // the choice changed since the picture was last resized: the window follows it

static const struct { const char *name; int width, height; } resolutions[RESOLUTION_COUNT] = {
	{ "Original (10:7)", 320, 224 },
	{ "16:9", 400, 224 },
	{ "8:5", 360, 224 },
	{ "5:4", 320, 256 },
	{ "4:3", 320, 240 },
};

const char *Video_ResolutionName(int mode) {
	return (mode >= 0 && mode < RESOLUTION_COUNT) ? resolutions[mode].name : "?";
}

// The size the player chose (what the menu shows, and the size of the picture).
int Video_GetResolution(void) {
	return resolution_chosen;
}

// The size the picture should have right now: the player's choice.
static int WantedResolution(void) {
	return resolution_chosen;
}

// The picture size changed (menu): it takes hold at the next safe point -- a mode starting, or the level's loop, which redraws
// what the new size shows.
void Video_RequestResolution(int mode) {
	if (mode < 0 || mode >= RESOLUTION_COUNT || mode == resolution_chosen)
		return;
	resolution_chosen = mode;
	chosen_changed = true;
}

bool Video_ResolutionPending(void) {
	return WantedResolution() != resolution_mode;
}

extern void Render_SetPictureSize(bool resize_window);

static void SetPictureSize(int mode) {
	resolution_mode = mode;
	screen_width = resolutions[mode].width;
	screen_height = resolutions[mode].height;
	plane_height = screen_height > 224 ? 64 : 32;
	VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
	hbla_counter = (int16_t)(screen_height - 1);
}

// Applies the wanted picture size: the buffers and the window. Returns whether the size changed.
bool Video_ApplyPendingResolution(void) {
	int wanted = WantedResolution();
	if (wanted == resolution_mode)
		return false;
	// The window follows the player's own choice; a demo that needs the original size only letterboxes it
	bool resize_window = chosen_changed;
	chosen_changed = false;
	SetPictureSize(wanted);
	Render_SetPictureSize(resize_window);
	return true;
}

// Picks the picture size without telling anyone (the window does not exist yet, at start-up).
void Video_SelectResolution(int mode) {
	if (mode < 0 || mode >= RESOLUTION_COUNT)
		mode = RESOLUTION_ORIGINAL;
	resolution_chosen = mode;
	chosen_changed = false;
	SetPictureSize(mode);
}

uint8_t sprite_count;

uint8_t hbla_pal;
int16_t hbla_pos;
int16_t hbla_counter = 223; // (the dormant line: the last one of the picture; see Video_SetResolution)

int16_t vid_scrpos_y_dup, vid_bg_scrpos_y_dup, vid_scrpos_x_dup, vid_bg_scrpos_x_dup, vid_bg3_scrpos_y_dup, vid_bg3_scrpos_x_dup;

uint16_t sprite_buffer[BUFFER_SPRITES][4]; //Apparently the last 16 entries of this intrude other memory in the original
                                           //... now how would I emulate that?
int16_t hscroll_buffer[SCREEN_MAX_HEIGHT][2];
uint16_t sprite_buffer_p2[BUFFER_SPRITES][4];
int16_t hscroll_buffer_p2[SCREEN_MAX_HEIGHT][2];
VDPView video_second_view = {
	.plane_a_location = VRAM_FG_P2,
	.plane_b_location = VRAM_BG_P2,
	.hscroll_location = VRAM_HSCROLL_P2,
	.vscroll_a = 0,
	.vscroll_b = 0,
	.sprite_buffer = &sprite_buffer_p2[0][0],
	.palette = NULL,
};

void Video_UploadHScrollP2(void) {
	VDP_SeekVRAM(VRAM_HSCROLL_P2);
	VDP_WriteVRAM((const uint8_t*)hscroll_buffer_p2, sizeof(hscroll_buffer_p2));
}

//Video interface
uint8_t wtr_state;
bool hblank_pal;
bool doupdatesinhblank;

// Turns the water palette split off: no h-interrupt, line back at its dormant (the last line of the picture), nothing deferred to
// HBlank, screen not "all underwater". Matches the original writing $8004 (8-colour mode, h-int disabled) when the title screen and
// special stages set up the VDP -- leaving LZ with the split still armed made the title screen draw with the water palette.
void VDPDisableWaterSplit(void) {
	VDP_SetHIntEnable(false);
	hbla_counter = SCREEN_HEIGHT - 1;
	VDP_SetHIntCounter(SCREEN_HEIGHT - 1);
	hblank_pal = false;
	doupdatesinhblank = false;
	wtr_state = 0;
}

void VDPSetupGame(void) {
	//Initialize VDP state
	VDP_SetPlaneALocation(VRAM_FG);
	VDP_SetPlaneBLocation(VRAM_BG);
	VDP_SetSpriteLocation(VRAM_SPRITES); // unused once VDP_SetSpriteBuffer is registered below, kept set for consistency/documentation
	VDP_SetSpriteBuffer(&sprite_buffer[0][0]); // sprite table lives in its own buffer, not VRAM -- see VDP_SetSpriteBuffer's own comment
	VDP_SetHScrollLocation(VRAM_HSCROLL);
	VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
	VDP_SetBackgroundColour(0);
	
	//Clear VRAM and CRAM
	VDP_SeekVRAM(0);
	VDP_FillVRAM(0x00, VRAM_SIZE);
	VDP_SeekCRAM(0);
	VDP_FillCRAM(0x0000, COLOURS);
	
	//Clear internal palette
	memset(dry_palette, 0, sizeof(dry_palette));
	memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
	memset(wet_palette, 0, sizeof(wet_palette));
	memset(wet_palette_dup, 0, sizeof(wet_palette_dup));

	// The original's VDPSetupArray writes $8004: h-interrupt off.
	VDPDisableWaterSplit();
}

void WaitForVBla(void) {
	//Render the VDP
	VDP_Render();
}

void ClearScreen(void) {
	//Clear foreground and background planes
	VDP_SeekVRAM(VRAM_FG);
	VDP_FillVRAM(0x00, (PLANE_WIDTH * PLANE_HEIGHT) << 1);
	VDP_SeekVRAM(VRAM_BG);
	VDP_FillVRAM(0x00, (PLANE_WIDTH * PLANE_HEIGHT) << 1);
	
	//Reset screen position duplicates
	vid_scrpos_y_dup = 0;
	vid_bg_scrpos_y_dup = 0;
	vid_scrpos_x_dup = 0;
	vid_bg_scrpos_x_dup = 0;
	
	//Clear sprite buffer and hscroll buffer
	memset(sprite_buffer, 0, sizeof(sprite_buffer));
	memset(sprite_buffer_p2, 0, sizeof(sprite_buffer_p2));
	memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
	memset(hscroll_buffer_p2, 0, sizeof(hscroll_buffer_p2));
}

void CopyTilemap(const uint8_t *tilemap, size_t offset, size_t width, size_t height) {
	while (height-- > 0) {
		VDP_SeekVRAM(offset);
		for (size_t x = 0; x < width; x++) {
			uint16_t v = (tilemap[0] << 8) | (tilemap[1] << 0);
			tilemap += 2;
			VDP_WriteVRAM((const uint8_t*)&v, 2);
		}
		offset += PLANE_WIDTH * 2;
	}
}
