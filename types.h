#ifndef TYPES_H
#define TYPES_H

// Fixed-point numbers
typedef short			sfx16;
typedef unsigned long	ufx32;
#define PI_16	(157 << 6) / 50
#define PI_32	(157 << 12) / 50

// Vectors
typedef struct{
	int x,y;
}Vec2i;

typedef struct{
	int x,y,w,h;
}Vec4i;

// These already have equivalents in libgte,
// but for the sake of "clean" I'm making my own here.
typedef struct{	// Ideal for normalized vectors.
	sfx16 x,y;	// 64 >> 6 = 1
				// 32 >> 6 = 0.5
}Vec2sfx16;

typedef struct{	// Ideal for positions.
	ufx32 x,y;	// 4096 >> 12 = 1
				// 2048 >> 12 = 0.5
}Vec2ufx32;

// OBJs
#define LEVEL_TILE_SIZE	16
#define LEVEL_WID		(320/LEVEL_TILE_SIZE - 4)
#define LEVEL_HGT		(240/LEVEL_TILE_SIZE - 4)
#define LEVEL_OFFSET_X	32
#define LEVEL_OFFSET_Y	32

#define TILE_TYPE_NONE		0
#define TILE_TYPE_NORMAL	1
#define TILE_TYPE_SPEED		2
#define TILE_TYPE_MULTI		3

typedef struct{
	//TIM_IMAGE timImg;
	//unsigned long* timFile;
	int tiles[LEVEL_HGT*LEVEL_WID];
}OBJ_LEVEL;

typedef struct{
	Vec2ufx32 position;		// Fixed-point position.
	Vec2sfx16 direction;	// Normalized vector.
	unsigned short speed;	// Fixed-point speed.
	Vec2i size;
	u_char r,g,b;
	char active;
}OBJ_BALL;

typedef struct{
	Vec4i rect;
	unsigned short speed;	// Fixed-point speed.
	u_char r,g,b;
}OBJ_PADDLE;

#endif