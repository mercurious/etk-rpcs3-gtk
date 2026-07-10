// rsx-zfunc-hwtest — does real RSX honor the texture ZFUNC depth-compare on
// COLOR-format fetches?  (RPCS3 issue #11912, GT5P road flicker)
//
// Renders a static grid of textured swatches. Every swatch is drawn by the
// same trivial fragment program (oColor = tex2D(sampler0, uv)); only the
// texture-unit state varies per cell:
//
//   MATRIX A  A8R8G8B8 1x1, texel 0x00000000
//             rows    CONTROL1 low word (remap): 0x0000, 0xAAE4, 0x55E4
//             columns ZFUNC: NEVER(0), GEQUAL(6), LEQUAL(3), ALWAYS(7)
//             The row-0/GEQUAL cell is the exact TIU state GT5P parks its
//             shadow sampler in during dim frames (fmt byte 0x85,
//             CONTROL1 0x00000000, zfunc GEQUAL) — the DECISIVE cell,
//             marked with a yellow frame.
//   MATRIX B  same grid, texel 0xFFFF00FF (magenta) — makes the CONTROL1
//             per-channel-control semantics visible at a glance
//             (0x0000 row black, 0xAAE4 row magenta, 0x55E4 row white).
//   MATRIX C  DEPTH24_D8 1x1, depth=0.5 — the classic depth-compare case,
//             zfunc LEQUAL vs NEVER: proves the harness plumbing is sound.
//   SIZE      the decisive state at texture size 1x1 and 64x64 (rules out
//             degenerate-descriptor effects).
//   REF       solid 0.0 / 0.5 / 1.0 texels through pass-through remap.
//
// The implicit compare reference for every fetch is 0.0: the vertex program
// forwards a float2 texcoord, so the interpolated r/q components are 0.
//
// Register facts (per-TIU, index*8 words apart):
//   SET_TEXTURE_FORMAT   0x1a04  bits[15:8] fmt byte: A8R8G8B8=0x85, DEPTH24_D8=0x90
//   SET_TEXTURE_ADDRESS  0x1a08  bits[31:28] zfunc (0 never .. 6 gequal, 7 always)
//   SET_TEXTURE_CONTROL1 0x1a10  low word: bits[7:0] crossbar, bits[15:8]
//                                per-channel control (0=ZERO,1=ONE,2=REMAP)
// psl1ght writes gcmTexture.remap VERBATIM to CONTROL1 and exposes zfunc as
// an rsxTextureWrapMode() argument (verified against librsx commands_impl.h:
// LoadTexture -> NV40TCL_TEX_SWIZZLE, TextureWrapMode -> zfunc<<28).
//
// Built from the PSL1GHT rsxtest sample skeleton.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>

#include <sys/process.h>

#include <io/pad.h>
#include <rsx/rsx.h>
#include <sysutil/sysutil.h>

#include "rsxutil.h"

#include <debugfont.h>

#include "swatch_vpo.h"
#include "swatch_fpo.h"

SYS_PROCESS_PARAM(1001, 0x100000);

u32 running = 0;

static u32 fp_offset;
static u32 *fp_buffer;

static void *vp_ucode = NULL;
static rsxVertexProgram *vpo = (rsxVertexProgram*)swatch_vpo;

static void *fp_ucode = NULL;
static rsxFragmentProgram *fpo = (rsxFragmentProgram*)swatch_fpo;

static s32 posAttrib = -1;
static s32 texcAttrib = -1;
static s32 samplerAttrib = -1;

// ---------------------------------------------------------------- constants

#define ZF_NEVER	GCM_TEXTURE_ZFUNC_NEVER		// 0
#define ZF_LEQUAL	GCM_TEXTURE_ZFUNC_LEQUAL	// 3
#define ZF_GEQUAL	GCM_TEXTURE_ZFUNC_GEQUAL	// 6
#define ZF_ALWAYS	GCM_TEXTURE_ZFUNC_ALWAYS	// 7

// CONTROL1 low words under test (crossbar bits[7:0], control bits[15:8])
#define REMAP_ZEROED		0x0000	// GT5P parked state: force-ZERO x4, zeroed crossbar
#define REMAP_PASSTHROUGH	0xAAE4	// REMAP x4, identity crossbar
#define REMAP_ONE			0x55E4	// ONE x4, identity crossbar

// raw fmt bytes 0x85 / 0x90 = 0x80 | psl1ght format (LoadTexture ORs the 0x80)
#define FMT_ARGB8	GCM_TEXTURE_FORMAT_A8R8G8B8		// 5  -> raw 0x85 (swizzled)
#define FMT_DEPTH24	16								// DEPTH24_D8 -> raw 0x90 (swizzled); not named in this psl1ght era

// ------------------------------------------------------------------- scenes

typedef struct {
	u32 offset;		// RSX-local offset of texel data
	u16 width;
	u16 height;
} TexBuf;

static TexBuf tex_zero1;	// A8R8G8B8 1x1   0x00000000
static TexBuf tex_zero64;	// A8R8G8B8 64x64 0x00000000
static TexBuf tex_mag1;		// A8R8G8B8 1x1   0xFFFF00FF
static TexBuf tex_depth1;	// DEPTH24_D8 1x1 depth 0.5 (word 0x80000000)
static TexBuf tex_black1;	// A8R8G8B8 1x1   0xFF000000
static TexBuf tex_gray1;	// A8R8G8B8 1x1   0xFF808080
static TexBuf tex_white1;	// A8R8G8B8 1x1   0xFFFFFFFF
static TexBuf tex_yellow1;	// A8R8G8B8 1x1   0xFFFFFF00

typedef struct {
	const char *tag;	// TTY log tag
	const TexBuf *tex;
	u8 fmt;				// psl1ght format value (raw byte = 0x80|fmt)
	u16 remap;			// CONTROL1 low word, written verbatim
	u8 zfunc;
	u16 x,y,w,h;		// design-space rect (1280x720)
	u8 decisive;		// yellow frame + wide border
} Cell;

#define MAX_CELLS 40
static Cell cells[MAX_CELLS];
static u32 cell_count = 0;

typedef struct { f32 x,y,z,w,u,v; } Vertex;

static Vertex *vertex_buffer;
static u32 vertex_offset;

static f32 scale_x = 1.0f, scale_y = 1.0f;	// actual res / 1280x720 design res

// ------------------------------------------------------------------ helpers

static void addCell(const char *tag,const TexBuf *tex,u8 fmt,u16 remap,u8 zfunc,
					u16 x,u16 y,u16 w,u16 h,u8 decisive)
{
	Cell *c = &cells[cell_count++];
	c->tag = tag; c->tex = tex;
	c->fmt = fmt; c->remap = remap; c->zfunc = zfunc;
	c->x = x; c->y = y; c->w = w; c->h = h;
	c->decisive = decisive;
}

static TexBuf makeSolidTexture(u16 width,u16 height,u32 texel)
{
	TexBuf t;
	u32 i,count = (u32)width*height;
	u32 *buf = (u32*)rsxMemalign(128,count*4);

	for(i=0;i<count;i++) buf[i] = texel;	// constant fill: swizzled layout == linear layout

	rsxAddressToOffset(buf,&t.offset);
	t.width = width;
	t.height = height;
	return t;
}

// program one TIU with a cell's exact state
static void setCellTexture(u8 unit,const Cell *c)
{
	gcmTexture texture;

	rsxInvalidateTextureCache(context,GCM_INVALIDATE_TEXTURE);

	texture.format		= c->fmt;					// swizzled (no LIN): raw byte 0x80|fmt
	texture.mipmap		= 1;
	texture.dimension	= GCM_TEXTURE_DIMS_2D;
	texture.cubemap		= GCM_FALSE;
	texture.remap		= c->remap;					// CONTROL1 low word, verbatim
	texture.width		= c->tex->width;
	texture.height		= c->tex->height;
	texture.depth		= 1;
	texture.location	= GCM_LOCATION_RSX;
	texture.pitch		= (u32)c->tex->width*4;		// ignored for swizzled layouts
	texture.offset		= c->tex->offset;
	rsxLoadTexture(context,unit,&texture);
	rsxTextureControl(context,unit,GCM_TRUE,0<<8,12<<8,GCM_TEXTURE_MAX_ANISO_1);
	rsxTextureFilter(context,unit,GCM_TEXTURE_NEAREST,GCM_TEXTURE_NEAREST,GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	rsxTextureWrapMode(context,unit,GCM_TEXTURE_CLAMP_TO_EDGE,GCM_TEXTURE_CLAMP_TO_EDGE,
					   GCM_TEXTURE_CLAMP_TO_EDGE,0 /* unsigned remap normal */,
					   c->zfunc,0);
}

// append one quad (design-space pixel rect) to the vertex buffer
static u32 vb_used = 0;
static u32 addQuad(u16 x,u16 y,u16 w,u16 h)
{
	u32 first = vb_used;
	f32 px0 = (f32)x*scale_x, py0 = (f32)y*scale_y;
	f32 px1 = (f32)(x+w)*scale_x, py1 = (f32)(y+h)*scale_y;
	f32 cx0 = (px0/(f32)display_width)*2.0f - 1.0f;
	f32 cx1 = (px1/(f32)display_width)*2.0f - 1.0f;
	f32 cy0 = 1.0f - (py0/(f32)display_height)*2.0f;
	f32 cy1 = 1.0f - (py1/(f32)display_height)*2.0f;
	Vertex *v = &vertex_buffer[vb_used];

	v[0].x = cx0; v[0].y = cy0; v[0].u = 0.0f; v[0].v = 0.0f;
	v[1].x = cx1; v[1].y = cy0; v[1].u = 1.0f; v[1].v = 0.0f;
	v[2].x = cx1; v[2].y = cy1; v[2].u = 1.0f; v[2].v = 1.0f;
	v[3].x = cx0; v[3].y = cy1; v[3].u = 0.0f; v[3].v = 1.0f;
	for(int i=0;i<4;i++) { v[i].z = 0.0f; v[i].w = 1.0f; }

	vb_used += 4;
	return first;
}

// per-cell quads, generated once at init
static u32 cell_quad[MAX_CELLS];
static u32 frame_quad[MAX_CELLS];

// border cell used for frames: pass-through white/yellow, zfunc NEVER
static Cell borderWhite, borderYellow;

static void buildScene()
{
	// textures
	tex_zero1	= makeSolidTexture(1,1,0x00000000);
	tex_zero64	= makeSolidTexture(64,64,0x00000000);
	tex_mag1	= makeSolidTexture(1,1,0xFFFF00FF);
	tex_depth1	= makeSolidTexture(1,1,0x80000000);	// D24=0x800000 (0.5), S8=0
	tex_black1	= makeSolidTexture(1,1,0xFF000000);
	tex_gray1	= makeSolidTexture(1,1,0xFF808080);
	tex_white1	= makeSolidTexture(1,1,0xFFFFFFFF);
	tex_yellow1	= makeSolidTexture(1,1,0xFFFFFF00);

	static const u16 remaps[3] = { REMAP_ZEROED, REMAP_PASSTHROUGH, REMAP_ONE };
	static const u8  zfuncs[4] = { ZF_NEVER, ZF_GEQUAL, ZF_LEQUAL, ZF_ALWAYS };
	static const char *atags[12] = {
		"A[0x0000][NEVER]","A[0x0000][GEQUAL]","A[0x0000][LEQUAL]","A[0x0000][ALWAYS]",
		"A[0xAAE4][NEVER]","A[0xAAE4][GEQUAL]","A[0xAAE4][LEQUAL]","A[0xAAE4][ALWAYS]",
		"A[0x55E4][NEVER]","A[0x55E4][GEQUAL]","A[0x55E4][LEQUAL]","A[0x55E4][ALWAYS]" };
	static const char *btags[12] = {
		"B[0x0000][NEVER]","B[0x0000][GEQUAL]","B[0x0000][LEQUAL]","B[0x0000][ALWAYS]",
		"B[0xAAE4][NEVER]","B[0xAAE4][GEQUAL]","B[0xAAE4][LEQUAL]","B[0xAAE4][ALWAYS]",
		"B[0x55E4][NEVER]","B[0x55E4][GEQUAL]","B[0x55E4][LEQUAL]","B[0x55E4][ALWAYS]" };

	const u16 CS = 84, CP = 92;		// cell size / pitch
	const u16 AX = 140, AY = 72;	// matrix A origin
	const u16 BY = 388;				// matrix B origin (same columns)

	// Matrix A: texel 0x00000000 — the decisive matrix
	for(int r=0;r<3;r++)
		for(int cn=0;cn<4;cn++)
			addCell(atags[r*4+cn],&tex_zero1,FMT_ARGB8,remaps[r],zfuncs[cn],
					AX+cn*CP,AY+r*CP,CS,CS,(r==0 && cn==1));

	// Matrix B: texel 0xFFFF00FF — control-mask semantics anchor
	for(int r=0;r<3;r++)
		for(int cn=0;cn<4;cn++)
			addCell(btags[r*4+cn],&tex_mag1,FMT_ARGB8,remaps[r],zfuncs[cn],
					AX+cn*CP,BY+r*CP,CS,CS,0);

	// Matrix C: DEPTH24_D8 depth=0.5 — harness sanity anchor
	addCell("C[DEPTH][LEQUAL]",&tex_depth1,FMT_DEPTH24,REMAP_PASSTHROUGH,ZF_LEQUAL, 700, 72,CS,CS,0);
	addCell("C[DEPTH][NEVER]", &tex_depth1,FMT_DEPTH24,REMAP_PASSTHROUGH,ZF_NEVER,  792, 72,CS,CS,0);

	// Size variants of the decisive state
	addCell("SIZE[1x1]",  &tex_zero1, FMT_ARGB8,REMAP_ZEROED,ZF_GEQUAL, 700,232,CS,CS,1);
	addCell("SIZE[64x64]",&tex_zero64,FMT_ARGB8,REMAP_ZEROED,ZF_GEQUAL, 792,232,CS,CS,1);

	// Reference bars 0.0 / 0.5 / 1.0
	addCell("REF[0.0]",&tex_black1,FMT_ARGB8,REMAP_PASSTHROUGH,ZF_NEVER, 700,412,CS,CS,0);
	addCell("REF[0.5]",&tex_gray1, FMT_ARGB8,REMAP_PASSTHROUGH,ZF_NEVER, 792,412,CS,CS,0);
	addCell("REF[1.0]",&tex_white1,FMT_ARGB8,REMAP_PASSTHROUGH,ZF_NEVER, 884,412,CS,CS,0);

	// vertex buffer: frame quad + cell quad per cell
	vertex_buffer = (Vertex*)rsxMemalign(128,cell_count*8*sizeof(Vertex));
	for(u32 i=0;i<cell_count;i++) {
		u16 b = cells[i].decisive ? 4 : 2;
		frame_quad[i] = addQuad(cells[i].x-b,cells[i].y-b,cells[i].w+2*b,cells[i].h+2*b);
		cell_quad[i]  = addQuad(cells[i].x,cells[i].y,cells[i].w,cells[i].h);
	}
	rsxAddressToOffset(vertex_buffer,&vertex_offset);

	// frame pseudo-cells (texture state only; quads come from frame_quad[])
	borderWhite.tex = &tex_white1;  borderWhite.fmt = FMT_ARGB8;
	borderWhite.remap = REMAP_PASSTHROUGH; borderWhite.zfunc = ZF_NEVER;
	borderYellow.tex = &tex_yellow1; borderYellow.fmt = FMT_ARGB8;
	borderYellow.remap = REMAP_PASSTHROUGH; borderYellow.zfunc = ZF_NEVER;

	// machine-readable state table on the TTY (shows in the RPCS3 log)
	printf("rsx-zfunc-hwtest cell map (fmt byte = 0x80|fmt):\n");
	for(u32 i=0;i<cell_count;i++)
		printf("  %-18s fmt=0x%02x ctrl1=0x%04x zfunc=%u tex=%ux%u rect=(%u,%u,%u,%u)%s\n",
			   cells[i].tag,0x80|cells[i].fmt,cells[i].remap,cells[i].zfunc,
			   cells[i].tex->width,cells[i].tex->height,
			   cells[i].x,cells[i].y,cells[i].w,cells[i].h,
			   cells[i].decisive ? "  <-- DECISIVE" : "");
}

// ------------------------------------------------------------------ drawing

static void setDrawEnv()
{
	rsxSetColorMask(context,GCM_COLOR_MASK_B |
							GCM_COLOR_MASK_G |
							GCM_COLOR_MASK_R |
							GCM_COLOR_MASK_A);

	rsxSetColorMaskMRT(context,0);

	u16 x,y,w,h;
	f32 min, max;
	f32 scale[4],offset[4];

	x = 0;
	y = 0;
	w = display_width;
	h = display_height;
	min = 0.0f;
	max = 1.0f;
	scale[0] = w*0.5f;
	scale[1] = h*-0.5f;
	scale[2] = (max - min)*0.5f;
	scale[3] = 0.0f;
	offset[0] = x + w*0.5f;
	offset[1] = y + h*0.5f;
	offset[2] = (max + min)*0.5f;
	offset[3] = 0.0f;

	rsxSetViewport(context,x, y, w, h, min, max, scale, offset);
	rsxSetScissor(context,x,y,w,h);

	rsxSetDepthTestEnable(context,GCM_FALSE);
	rsxSetDepthWriteEnable(context,GCM_FALSE);
	rsxSetBlendEnable(context,GCM_FALSE);
	rsxSetCullFaceEnable(context,GCM_FALSE);
	rsxSetShadeModel(context,GCM_SHADE_MODEL_SMOOTH);
}

static void drawQuadWithState(const Cell *state,u32 first)
{
	setCellTexture(samplerAttrib,state);
	rsxDrawVertexArray(context,GCM_TYPE_QUADS,first,4);
}

static void drawFrame()
{
	u32 i;

	setDrawEnv();

	rsxSetClearColor(context,0xFF303840);	// dark slate: black cells stay visible
	rsxSetClearDepthValue(context,0xffffff00);
	rsxClearSurface(context,GCM_CLEAR_R |
							GCM_CLEAR_G |
							GCM_CLEAR_B |
							GCM_CLEAR_A |
							GCM_CLEAR_S |
							GCM_CLEAR_Z);

	for(i=0;i<8;i++)
		rsxSetViewportClip(context,i,display_width,display_height);

	rsxBindVertexArrayAttrib(context,posAttrib,vertex_offset,
							 sizeof(Vertex),4,GCM_VERTEX_DATA_TYPE_F32,GCM_LOCATION_RSX);
	rsxBindVertexArrayAttrib(context,texcAttrib,vertex_offset+4*sizeof(f32),
							 sizeof(Vertex),2,GCM_VERTEX_DATA_TYPE_F32,GCM_LOCATION_RSX);

	rsxLoadVertexProgram(context,vpo,vp_ucode);
	rsxLoadFragmentProgramLocation(context,fpo,fp_offset,GCM_LOCATION_RSX);

	for(i=0;i<cell_count;i++) {
		drawQuadWithState(cells[i].decisive ? &borderYellow : &borderWhite,frame_quad[i]);
		drawQuadWithState(&cells[i],cell_quad[i]);
	}
}

static void px(s32 x,s32 y)		// design-space -> actual-pixel text position
{
	DebugFont::setPosition((s32)(x*scale_x),(s32)(y*scale_y));
}

static void drawLabels()
{
	static const char *colhdr[4] = { "NEVER","GEQUAL","LEQUAL","ALWAYS" };
	static const char *rowhdr[3] = { "0x0000","0xAAE4","0x55E4" };

	DebugFont::setColor(1.0f,1.0f,1.0f,1.0f);
	px(140,16);  DebugFont::print("RSX TEXTURE ZFUNC vs CONTROL1 HWTEST (RPCS3 ISSUE 11912)");

	px(140,40);  DebugFont::print("MATRIX A: A8R8G8B8(0x85) 1x1 TEXEL=0x00000000");
	for(int c=0;c<4;c++) { px(140+c*92+18,58); DebugFont::print(colhdr[c]); }
	for(int r=0;r<3;r++) { px(80,72+r*92+38);  DebugFont::print(rowhdr[r]); }

	px(140,356); DebugFont::print("MATRIX B: TEXEL=0xFFFF00FF (MAGENTA)");
	for(int c=0;c<4;c++) { px(140+c*92+18,372); DebugFont::print(colhdr[c]); }
	for(int r=0;r<3;r++) { px(80,388+r*92+38);  DebugFont::print(rowhdr[r]); }

	px(700,40);  DebugFont::print("MATRIX C: DEPTH24_D8(0x90) DEPTH=0.5");
	px(700,58);  DebugFont::print("LEQUAL");
	px(792,58);  DebugFont::print("NEVER");

	px(700,200); DebugFont::print("DECISIVE STATE: CTRL1=0x0000 ZFUNC=GEQUAL");
	px(700,218); DebugFont::print("1x1");
	px(792,218); DebugFont::print("64x64");

	px(700,380); DebugFont::print("REFERENCE");
	px(700,398); DebugFont::print("0.0");
	px(792,398); DebugFont::print("0.5");
	px(884,398); DebugFont::print("1.0");

	DebugFont::setColor(1.0f,1.0f,0.0f,1.0f);
	px(700,540); DebugFont::print("YELLOW FRAME = GT5P PARKED-SAMPLER STATE");
	px(700,558); DebugFont::print("FMT 0x85 CTRL1 0x0000 ZFUNC GEQUAL TEXEL 0");
}

// Dump the just-rendered back buffer as a 24bpp BMP. On RPCS3, /app_home
// maps to the directory the ELF was booted from, giving a pixel-exact
// host-side reference image. Fail-silent everywhere else (real hardware:
// the TV photo is the artifact).
static void dumpFramebufferBMP(u32 fb_index)
{
	const char *paths[2] = { "/app_home/rpcs3_framedump.bmp", "/dev_hdd0/rpcs3_framedump.bmp" };
	u32 w = display_width, h = display_height;
	u32 rowbytes = w*3;					// 1280*3 and 640*3 are 4-byte aligned already
	u32 imgsize = rowbytes*h;
	u8 header[54];
	FILE *f = NULL;

	for(int i=0;i<2 && !f;i++) f = fopen(paths[i],"wb");
	if(!f) { printf("framedump: no writable path\n"); return; }

	memset(header,0,54);
	header[0]='B'; header[1]='M';
	u32 fsize = 54+imgsize;
	header[2]=fsize&0xff; header[3]=(fsize>>8)&0xff; header[4]=(fsize>>16)&0xff; header[5]=(fsize>>24)&0xff;
	header[10]=54;
	header[14]=40;
	header[18]=w&0xff; header[19]=(w>>8)&0xff;
	header[22]=h&0xff; header[23]=(h>>8)&0xff;
	header[26]=1;
	header[28]=24;
	header[34]=imgsize&0xff; header[35]=(imgsize>>8)&0xff; header[36]=(imgsize>>16)&0xff; header[37]=(imgsize>>24)&0xff;
	fwrite(header,1,54,f);

	u8 *row = (u8*)malloc(rowbytes);
	const u8 *fbbytes = (const u8*)color_buffer[fb_index];
	for(s32 y=(s32)h-1;y>=0;y--) {		// BMP rows are bottom-up
		const u8 *src = fbbytes + (u32)y*color_pitch;
		for(u32 x=0;x<w;x++) {			// guest XRGB word = bytes [X][R][G][B]
			row[x*3+0] = src[x*4+3];	// B
			row[x*3+1] = src[x*4+2];	// G
			row[x*3+2] = src[x*4+1];	// R
		}
		fwrite(row,1,rowbytes,f);
	}
	free(row);
	fclose(f);
	printf("framedump: wrote %ux%u BMP\n",w,h);
}

// -------------------------------------------------------------------- setup

static void init_shader()
{
	u32 fpsize = 0;

	vp_ucode = rsxVertexProgramGetUCode(vpo);
	fp_ucode = rsxFragmentProgramGetUCode(fpo, &fpsize);

	fp_buffer = (u32*)rsxMemalign(64,fpsize);
	memcpy(fp_buffer,fp_ucode,fpsize);
	rsxAddressToOffset(fp_buffer,&fp_offset);

	posAttrib		= rsxVertexProgramGetAttrib(vpo,"position");
	texcAttrib		= rsxVertexProgramGetAttrib(vpo,"texcoord");
	samplerAttrib	= rsxFragmentProgramGetAttrib(fpo,"sampler0");

	printf("shaders: fp %u bytes, sampler unit %d, pos attr %d, texc attr %d\n",
		   fpsize,samplerAttrib,posAttrib,texcAttrib);
}

extern "C" {
static void program_exit_callback()
{
	gcmSetWaitFlip(context);
	rsxFinish(context,1);
}

static void sysutil_exit_callback(u64 status,u64 param,void *usrdata)
{
	switch(status) {
		case SYSUTIL_EXIT_GAME:
			running = 0;
			break;
		default:
			break;
	}
}
}

int main(int argc,const char *argv[])
{
	padInfo padinfo;
	padData paddata;
	void *host_addr = memalign(HOST_ADDR_ALIGNMENT,HOSTBUFFER_SIZE);

	printf("rsx-zfunc-hwtest starting...\n");

	init_screen(host_addr,HOSTBUFFER_SIZE);
	ioPadInit(7);
	init_shader();

	scale_x = (f32)display_width/1280.0f;
	scale_y = (f32)display_height/720.0f;
	printf("display %ux%u (scale %.3f,%.3f)\n",display_width,display_height,scale_x,scale_y);

	buildScene();

	DebugFont::init();
	DebugFont::setScreenRes(display_width, display_height);
	DebugFont::setSafeArea(0,0,0,0);	// label positions are already laid out cell-safe

	atexit(program_exit_callback);
	sysUtilRegisterCallback(0,sysutil_exit_callback,NULL);

	setDrawEnv();
	setRenderTarget(curr_fb);

	running = 1;
	u32 frame = 0;
	while(running) {
		sysUtilCheckCallback();

		ioPadGetInfo(&padinfo);
		for(int i=0; i < MAX_PADS; i++){
			if(padinfo.status[i]){
				ioPadGetData(i, &paddata);
				if(paddata.BTN_CROSS)
					goto done;
			}
		}

		drawFrame();
		drawLabels();

		if(++frame == 90) {			// scene is static; dump one settled frame
			rsxFinish(context,1);	// let RSX drain before the CPU reads the back buffer
			dumpFramebufferBMP(curr_fb);
		}

		flip();
	}

done:
	printf("rsx-zfunc-hwtest done.\n");
	program_exit_callback();
	return 0;
}
