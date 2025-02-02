// Res:
// 	Old PSX dev tutorials ; http://lameguy64.net/svn/pstutorials/chapter1/1-display.html
//	nolibgs xa docs ; https://github.com/ABelliqueux/nolibgs_hello_worlds/wiki/XA
//	Archerite's tile drawing code ; https://discord.com/channels/642647820683444236/642849069378568192/1128603597546995744

#include <sys/types.h>
#include <stdio.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include <libcd.h>
#include <libspu.h>
#include <libmath.h>

#include "types.h"
#include "tims.h"
#include "levels.h"

#define VMODE		0	// Video Mode ; 0: NTSC, 1: PAL
#define SCREENXRES	320	// Screen width
#define SCREENYRES	240	// Screen height

#define MARGINX	0	// margins for text display
#define MARGINY	32

#define FONTSIZE	8 * 7	// Text Field Height

#define OTLEN	180	// Length of ordering table.
					// 16x11 = 176 possible tiles + paddle and 3 balls.

SpuCommonAttr spuSettings;	// SPU attributes.

#define CD_SECTOR_SIZE	2048	// Unsure what this is.

DISPENV disp[2];	// Double buffered DISPENV and DRAWENV
DRAWENV draw[2];

u_long ot[2][OTLEN];	// Double-buffered ordering table.
						// This is used to queue primitives for drawing.

char primbuff[2][32768];		// Double primitive buffer.
char* nextpri = primbuff[0];	// Pointer to the next primitive in primbuff.

short db = 0;	// Which buffer to use.

// BALLs
#define BALL_MAX		32
#define BALL_DRAW_LAYER	1
OBJ_BALL ballArr[BALL_MAX];	// BALLSs to draw.

// PADDLE
#define PADDLE_DRAW_LAYER	3
OBJ_PADDLE paddle;

// Levels
#define LEVEL_DRAW_LAYER	4
#define LEVEL_NUM			2
int levelIndex = 0;
OBJ_LEVEL levels[LEVEL_NUM];

#define XA_SECTOR_OFFSET	4	// Unsure what this is.
								// 4: simple speed, 8: double speed.
#define XA_TRACKS	1			// Number of XA tracks.

typedef struct{
	int start;	// Start of the track in some sectors?
	int end;	// Ditto.
}XA_TRACK;

XA_TRACK musTracks[XA_TRACKS];	// Array of tracks.

static char* musLusamine = "\\M_LUSAMINE;1";	// Name of track to load.

CdlFILE xaPos = {0};	// ?

static int xaStartPos, xaEndPos;	// Start and end pos of track in sectors.
static int xaCurPos = -1;			// Ditto.
static int xaIsPlaying = 0;			// Playback status.
static char xaChannel = 0;			// Current XA channel.
CdlLOC xaCDPos;						// Position of the track on the CD.

void LoadTexture(u_long* tim, TIM_IMAGE* tparam){	// Credit: Lameguy64 ; lameguy64.net/svn/pstutorials/chapter1/3-textures.html
	OpenTIM(tim);		// Opens the TIM binary. Unsure what that means.
	ReadTIM(tparam);	// Inits tparam as a TIM_IMAGE by reading the header data from tim. (I think?)
	
	LoadImage(tparam->prect, tparam->paddr);	// Load image data into VRAM.
	DrawSync(0);								// Wait for drawing to end. (Why?)
	
	if(tparam->mode & 0x8){							// Check if the TIM uses a CLUT (Color Look-up Table).
		LoadImage(tparam->crect, tparam->caddr);	// Load CLUT data into VRAM.
		DrawSync(0);
	}
}

void init(void){
    ResetGraph(0);														// Initialize drawing engine with a complete reset (0)
    SetDefDispEnv(&disp[0], 0, 0, SCREENXRES, SCREENYRES);				// Set display area for both &disp[0] and &disp[1]
    SetDefDispEnv(&disp[1], 0, SCREENYRES, SCREENXRES, SCREENYRES);     // &disp[0] is on top  of &disp[1]
    SetDefDrawEnv(&draw[0], 0, SCREENYRES, SCREENXRES, SCREENYRES);     // Set draw for both &draw[0] and &draw[1]
    SetDefDrawEnv(&draw[1], 0, 0, SCREENXRES, SCREENYRES);				// &draw[0] is below &draw[1]
    
	// Check if PAL.
	if (VMODE){
        SetVideoMode(MODE_PAL);
        disp[0].screen.y += 8;	// add offset ; 240 + 8 + 8 = 256
        disp[1].screen.y += 8;
    }
	
    SetDispMask(1);                 // Display on screen
	
    setRGB0(&draw[0], 50, 50, 50);	// set color for first draw area
    setRGB0(&draw[1], 50, 50, 50);	// set color for second draw area
    
	draw[0].isbg = 1;               // set mask for draw areas. 1 means repainting the area with the RGB color each frame 
    draw[1].isbg = 1;
    
	PutDispEnv(&disp[db]);          // set the disp and draw environnments
    PutDrawEnv(&draw[db]);
    
	FntLoad(960, 0);                // Load font to vram at 960,0(+128)
    FntOpen(MARGINX, SCREENYRES - MARGINY - FONTSIZE, SCREENXRES - MARGINX * 2, FONTSIZE, 0, 280 ); // FntOpen(x, y, width, height,  black_bg, max. nbr. chars
	
	SpuInit();
}

int FindNextBallIndex(){
	for(int i = 0; i < BALL_MAX; i++){
		if(ballArr[i].active == 0) return i;
	}
}

void InitBall(int index, int x, int y, unsigned short speed){
	ballArr[index] = (OBJ_BALL){
		(Vec2ufx32){x << 12, y << 12},
		(Vec2sfx16){(1<<6)/2, (1<<6)/2},
		speed,
		(Vec2i){8,8},
		128,128,128,
		1
	};
}

int CheckOverlap(Vec4i r1, Vec4i r2){
	if(r1.x < r2.x+r2.w && r1.x+r1.w >= r2.x &&
		r1.y < r2.y+r2.h && r1.y+r1.h >= r2.y){
		
		return 1;
	}
	
	return 0;
}

void DrawLevel(){
	// Draw tiles.
	for(int i = 0; i < LEVEL_HGT*LEVEL_WID; i++){
		if(levels[levelIndex].tiles[i] != 0){
			SPRT* tile = (SPRT*) nextpri;
			setSprt(tile);
			//TILE* tile = (TILE*) nextpri;
			//setTile(tile);
			setRGB0(tile, 128, 128, 128);
			setXY0(tile, LEVEL_OFFSET_X + ((i % LEVEL_WID) * LEVEL_TILE_SIZE), LEVEL_OFFSET_Y + ((i / LEVEL_WID) * LEVEL_TILE_SIZE));
			setWH(tile, LEVEL_TILE_SIZE, LEVEL_TILE_SIZE);
			setUV0(tile, (levels[levelIndex].tiles[i]-1)*(LEVEL_TILE_SIZE), 0);
			setClut(tile, timTiles.crect->x, timTiles.crect->y);
			addPrim(ot[db][OTLEN - LEVEL_DRAW_LAYER], tile);
			nextpri += sizeof(SPRT);
			
			DR_TPAGE* tileTPage = (DR_TPAGE*) nextpri;
			setDrawTPage(tileTPage, 0, 1,
				getTPage(timTiles.mode & 0x3, 0,
					timTiles.prect->x, timTiles.prect->y));
			addPrim(ot[db][OTLEN - LEVEL_DRAW_LAYER], tileTPage);
			nextpri += sizeof(DR_TPAGE);
		}
	}
	
	// Draw Marquee.
	// 	Due to textures having a max size of 256x256,
	//	the marquee has to be split into two SPRTs.
	SPRT* marLeftSprt = (SPRT*) nextpri;
	setSprt(marLeftSprt);
	setRGB0(marLeftSprt, 128, 128, 128);
	setXY0(marLeftSprt, 0, 0);
	setWH(marLeftSprt, 160, 240);
	setUV0(marLeftSprt, 0, 0);
	setClut(marLeftSprt, timMarquee_1.crect->x, timMarquee_1.crect->y);
	addPrim(ot[db][OTLEN - LEVEL_DRAW_LAYER+1], marLeftSprt);
	nextpri += sizeof(SPRT);
	DR_TPAGE* marLeftSprtTPage = (DR_TPAGE*) nextpri;
	setDrawTPage(marLeftSprtTPage, 0, 1,
		getTPage(timMarquee_1.mode & 0x3, 0,
			timMarquee_1.prect->x, timMarquee_1.prect->y));
	addPrim(ot[db][OTLEN - LEVEL_DRAW_LAYER+1], marLeftSprtTPage);
	nextpri += sizeof(DR_TPAGE);
	
	SPRT* marRightSprt = (SPRT*) nextpri;
	setSprt(marRightSprt);
	setRGB0(marRightSprt, 128, 128, 128);
	setXY0(marRightSprt, 160, 0);
	setWH(marRightSprt, 160, 240);
	setUV0(marRightSprt, 0, 0);
	setClut(marRightSprt, timMarquee_2.crect->x, timMarquee_2.crect->y);
	addPrim(ot[db][OTLEN - LEVEL_DRAW_LAYER+1], marRightSprt);
	nextpri += sizeof(SPRT);
	DR_TPAGE* marRightSprtTPage = (DR_TPAGE*) nextpri;
	setDrawTPage(marRightSprtTPage, 0, 1,
		getTPage(timMarquee_2.mode & 0x3, 1,
			timMarquee_2.prect->x, timMarquee_2.prect->y));
	addPrim(ot[db][OTLEN - LEVEL_DRAW_LAYER+1], marRightSprtTPage);
	nextpri += sizeof(DR_TPAGE);
}

void StepBalls(){
	for(int i = 0; i < BALL_MAX; i++){
		if(ballArr[i].active == 1){
			// Collision with screen borders.
			int ballX = ballArr[i].position.x >> 12;
			int ballY = ballArr[i].position.y >> 12;
			int nextX = ballArr[i].position.x + (ballArr[i].speed * ballArr[i].direction.x) >> 12;
			int nextY = ballArr[i].position.y + (ballArr[i].speed * ballArr[i].direction.y) >> 12;
			
			if(nextX < 32 || nextX > SCREENXRES - ballArr[i].size.x - 32)
				ballArr[i].direction.x = -ballArr[i].direction.x;
			if(nextY < 32 || nextY > SCREENYRES - ballArr[i].size.y - 32)
				ballArr[i].direction.y = -ballArr[i].direction.y;
			
			// Collision with paddle.
			if(nextX < paddle.rect.x + paddle.rect.w && nextX + ballArr[i].size.x > paddle.rect.x &&
				nextY < paddle.rect.y + paddle.rect.h && nextY + ballArr[i].size.y > paddle.rect.y){
				
				// TODO:
				//	Ball direction should be based on how the paddle is currently moving.
				if(nextX+(ballArr[i].size.x/2) < paddle.rect.x+(paddle.rect.w/2))
					ballArr[i].direction.x = -((1 << 6) / 2);
				else
					ballArr[i].direction.x = ((1 << 6) / 2);
				ballArr[i].direction.y = -ballArr[i].direction.y;
			}
			
			// Collision with tiles.
			//	TODO: This is bad and should be rewritten.
			FntPrint("%d, %d\n", nextX, nextY);
			
			for(int j = 0; j < LEVEL_HGT*LEVEL_WID; j++){
				int tileX = (j%LEVEL_WID)*LEVEL_TILE_SIZE+LEVEL_OFFSET_X;
				int tileY = (j/LEVEL_WID)*LEVEL_TILE_SIZE+LEVEL_OFFSET_Y;
				
				if(levels[levelIndex].tiles[j] != 0){
					if(CheckOverlap((Vec4i){tileX, tileY, LEVEL_TILE_SIZE, LEVEL_TILE_SIZE},
						(Vec4i){nextX, ballY, ballArr[i].size.x, ballArr[i].size.y})){
						if(levels[levelIndex].tiles[j] == TILE_TYPE_SPEED)
							ballArr[i].speed += (1 << 6) / 2;
						if(levels[levelIndex].tiles[j] == TILE_TYPE_MULTI){
							int nextIndex = FindNextBallIndex();
							InitBall(nextIndex, ballX, ballY, ballArr[i].speed);
							ballArr[nextIndex].direction.x = -ballArr[i].direction.x;
							ballArr[nextIndex].direction.y = -ballArr[i].direction.y;
						}
							
						levels[levelIndex].tiles[j] = 0;
						
						ballArr[i].direction.x = -ballArr[i].direction.x;
						j = LEVEL_HGT*LEVEL_WID;
					}
					
					if(CheckOverlap((Vec4i){tileX, tileY, LEVEL_TILE_SIZE, LEVEL_TILE_SIZE},
						(Vec4i){ballX, nextY, ballArr[i].size.x, ballArr[i].size.y})){
						if(levels[levelIndex].tiles[j] == TILE_TYPE_SPEED)
							ballArr[i].speed += (1 << 6) / 2;
						if(levels[levelIndex].tiles[j] == TILE_TYPE_MULTI){
							int nextIndex = FindNextBallIndex();
							InitBall(nextIndex, ballX, ballY, ballArr[i].speed);
							ballArr[nextIndex].direction.x = -ballArr[i].direction.x;
							ballArr[nextIndex].direction.y = -ballArr[i].direction.y;
						}
						levels[levelIndex].tiles[j] = 0;
						
						ballArr[i].direction.y = -ballArr[i].direction.y;
						j = LEVEL_HGT*LEVEL_WID;
					}
				}
			}
			
			ballArr[i].position.x += (ballArr[i].direction.x * ballArr[i].speed);
			ballArr[i].position.y += (ballArr[i].direction.y * ballArr[i].speed);
			
			// Draw BALL.
			SPRT* tile = (SPRT*) nextpri;
			setSprt(tile);
			setXY0(tile,
				(ballArr[i].position.x >> 12),
				(ballArr[i].position.y >> 12));
			setWH(tile, ballArr[i].size.x, ballArr[i].size.y);
			setRGB0(tile, ballArr[i].r, ballArr[i].g, ballArr[i].b);
			setClut(tile, timBall.crect->x, timBall.crect->y);
			setUV0(tile, 0, 0);
			addPrim(ot[db][OTLEN - BALL_DRAW_LAYER], tile);
			nextpri += sizeof(SPRT);
		}
	}
	
	DR_TPAGE* tileTPage = (DR_TPAGE*) nextpri;
	setDrawTPage(tileTPage, 0, 1,
		getTPage(timBall.mode & 0x3, 0,
			timBall.prect->x, timBall.prect->y));
	addPrim(ot[db][OTLEN - BALL_DRAW_LAYER], tileTPage);
	nextpri += sizeof(DR_TPAGE);
}

void StepPaddle(int pad){
	// Process PADDLE.
	// TODO:
	//	Transition to using fixed point for position.
	//	If speed is below 1 then it will add nothing to position.
	if(pad & PADLleft && paddle.rect.x - 32 > 0)							paddle.rect.x -= (paddle.speed >> 6);
	if(pad & PADLright && paddle.rect.x + paddle.rect.w + 32 < SCREENXRES)	paddle.rect.x += (paddle.speed >> 6);
	
	// Draw PADDLE.
	SPRT* tile = (SPRT*) nextpri;
	setSprt(tile);
	setRGB0(tile, paddle.r, paddle.g, paddle.b);
	setXY0(tile,
		paddle.rect.x,
		paddle.rect.y);
	setWH(tile, paddle.rect.w, paddle.rect.h);
	setUV0(tile, 0, 0);
	setClut(tile, timPaddle.crect->x, timPaddle.crect->y);
	addPrim(ot[db][OTLEN - PADDLE_DRAW_LAYER], tile);
	nextpri += sizeof(SPRT);
	
	DR_TPAGE* tileTPage = (DR_TPAGE*) nextpri;
	setDrawTPage(tileTPage, 0, 1,
		getTPage(timPaddle.mode & 0x3, 0,
			timPaddle.prect->x, timPaddle.prect->y));
	addPrim(ot[db][OTLEN - PADDLE_DRAW_LAYER], tileTPage);
	nextpri += sizeof(DR_TPAGE);
}

void display(void){
    DrawSync(0);                    // Wait for all drawing to terminate
    VSync(0);                       // Wait for the next vertical blank
    PutDispEnv(&disp[db]);          // set alternate disp and draw environnments
    PutDrawEnv(&draw[db]);
	
	// Draw everything in ordering table.
	// Since a reverse ot is used, the last element is drawn first.
	// Idk what dictates what ot to use.
	DrawOTag(&ot[db][OTLEN - 1]);
	
	// Switch buffers.
    db = !db;
	nextpri = primbuff[db];
}

int main(void){
    init();
	
	// Set SPU settings.
	// Unsure what these represent?
	spuSettings.mask = (SPU_COMMON_MVOLL | SPU_COMMON_MVOLR | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDMIX);
	spuSettings.mvol.left = 0x6000;
	spuSettings.mvol.right = 0x6000;
	spuSettings.cd.volume.left = 0x6000;
	spuSettings.cd.volume.right = 0x6000;
	spuSettings.cd.mix = SPU_ON;				// Turn on CD input.
	SpuSetCommonAttr(&spuSettings);
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	CdInit();
	
	// Load XA files.
	CdSearchFile(&xaPos, musLusamine);
	
	// Set track positions.
	xaStartPos = musTracks[0].start = CdPosToInt(&xaPos.pos);
	xaEndPos = musTracks[0].end = xaStartPos + (xaPos.size / CD_SECTOR_SIZE) - 1;
	
	// XA parameters.
	u_char param[4];
	param[0] = CdlModeSpeed | 			// Set drive speed to double.
		CdlModeRT | 					// Use ASPCM play?
		CdlModeSF | 					// Use subheader filter.
		CdlModeSize1;					// Set sector size.
	CdControlB(CdlSetmode, param, 0);	// Apply the params from above.
	CdControlF(CdlPause, 0);			// Pause at current position.
	
	// Init filter.
	CdlFILTER filter;
	filter.file = 1;
	filter.chan = xaChannel;
	CdControlF(CdlSetfilter, (u_char*)(&filter));
	
	CdlLOC loc;
	xaCurPos = xaStartPos;
	
	// Init gamepad.
	int pad = 0;
	PadInit(0);
	
	// Init BALLs.
	ballArr[0] = (OBJ_BALL){
		(Vec2ufx32){SCREENXRES/2 << 12, SCREENYRES/2 << 12},
		(Vec2sfx16){-((1 << 6) / 2), -((1 << 6) / 2)},
		2 << 6,
		(Vec2i){8, 8},
		128,128,128,
		1
	};
	
	// Init PADDLE.
	paddle = (OBJ_PADDLE){
		(Vec4i){SCREENXRES/2, SCREENYRES-48, 64, 8},
		2 << 6,
		//255, 0, 255
		128,128,128
	};
	
	// Init Levels.
	InitLevels(levels, LEVEL_NUM);
	
	// Load textures.
	LoadTexture(_binary_res_paddle_tim_start, &timPaddle);
	LoadTexture(_binary_res_ball_tim_start, &timBall);
	//LoadTexture(_binary_res_marquee_tim_start, &timMarquee);
	LoadTexture(_binary_res_marquee1_tim_start, &timMarquee_1);
	LoadTexture(_binary_res_marquee2_tim_start, &timMarquee_2);
	LoadTexture(_binary_res_tiles_tim_start, &timTiles);
	
    while (1){
        // Clear reversed ordering table.
		ClearOTagR(ot[db], OTLEN);
		
		// Poll gamepad.
		pad = PadRead(0);
	
		// Draw level + marquee.
		DrawLevel();
		
		// Step OBJs.
		StepBalls();
		StepPaddle(pad);
		
		FntFlush(-1);	// Draw print stream, clear print buffer.
		display();
    }
	
    return 0;
}