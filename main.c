// Res:
// 	http://lameguy64.net/svn/pstutorials/chapter1/1-display.html
//	https://github.com/ABelliqueux/nolibgs_hello_worlds/wiki/XA

#include <sys/types.h>
#include <stdio.h>
#include <libgte.h>
#include <libetc.h>
#include <libgpu.h>
#include <libcd.h>
#include <libspu.h>

#define VMODE		0	// Video Mode ; 0: NTSC, 1: PAL
#define SCREENXRES	320	// Screen width
#define SCREENYRES	240	// Screen height

#define MARGINX	0	// margins for text display
#define MARGINY	32

#define FONTSIZE	8 * 7	// Text Field Height

#define OTLEN	16	// Length of ordering table.

SpuCommonAttr spuSettings;	// SPU attributes.

#define CD_SECTOR_SIZE	2048	// Unsure what this is.

DISPENV disp[2];	// Double buffered DISPENV and DRAWENV
DRAWENV draw[2];

u_long ot[2][OTLEN];	// Double-buffered ordering table.
						// This is used to queue primitives for drawing.

char primbuff[2][32768];		// Double primitive buffer.
char* nextpri = primbuff[0];	// Pointer to the next primitive in primbuff.

short db = 0;	// Which buffer to use.

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
	
	if(tparam->mode & 0x8){	// Check if the TIM uses a CLUT (Color Look-up Table).
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
	
	// Init TILEs.
	TILE* blueTile;
	TILE* pinkTile;
	TILE* redTile;
	
	// Init gamepad.
	int pad = 0;
	PadInit(0);
	
    while (1){
        // Clear reversed ordering table.
		ClearOTagR(ot[db], OTLEN);
		
		// Poll gamepad.
		pad = PadRead(0);
		
		// Draw TILEs.
		blueTile = (TILE*) nextpri;				// Set blueTile to point towards the "empty" space in the primitive buffer.
		setTile(blueTile);						// Init blueTile as a TILE. Not sure what's going on behind the scenes here.
		setXY0(blueTile, 0, 0);
		setWH(blueTile, 32, 32);
		setRGB0(blueTile, 0, 0, 255);
		addPrim(ot[db][OTLEN - 1], blueTile);	// Add blueTile to the ot.
		nextpri += sizeof(TILE);				// Point nextpri towards the next available space in the primitive buffer.
		
		pinkTile = (TILE*) nextpri;
		setTile(pinkTile);
		setXY0(pinkTile, 16, 16);
		setWH(pinkTile, 64, 64);
		setRGB0(pinkTile, 255, 32, 255);
		addPrim(ot[db][OTLEN - 2], pinkTile);	// Add pinkTile to the ot. The higher index is drawn after (in front of) the lower index.
		nextpri += sizeof(TILE);
		
		redTile = (TILE*) nextpri;
		setTile(redTile);
		setXY0(redTile, 64, 64);
		setWH(redTile, 32, 32);
		setRGB0(redTile, 255, 0, 0);
		addPrim(ot[db][OTLEN - 1], redTile);
		nextpri += sizeof(TILE);
		
		FntPrint("...aaaand we're live.");
        FntFlush(-1);	// Draw print stream, clear print buffer.
        
		display();
    }
	
    return 0;
}