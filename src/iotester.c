#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include "font.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>

#include <stdio.h>
#include <stdlib.h>

#include <linux/limits.h>

static const int SDL_WAKEUPEVENT = SDL_USEREVENT+1;

#if 0
#define system(x) printf(x); printf("\n")
#define DBG(x) printf("%s:%d %s %s\n", __FILE__, __LINE__, __func__, x);
#else
#define system(x) do{}while(0)
#define DBG(x) do{}while(0)
#endif

#define WIDTH  640
#define HEIGHT 480

#define BTN_X			SDLK_x
#define BTN_A			SDLK_a
#define BTN_B			SDLK_b
#define BTN_Y			SDLK_y
#define BTN_L			SDLK_h
#define BTN_R			SDLK_l
#define BTN_L2			SDLK_j
#define BTN_R2			SDLK_k
#define BTN_START		SDLK_m
#define BTN_SELECT		SDLK_n
#define BTN_POWER		SDLK_p
#define BTN_UP			SDLK_w
#define BTN_DOWN		SDLK_s
#define BTN_LEFT		SDLK_q
#define BTN_RIGHT		SDLK_d
#define BTN_VOLUP		SDLK_r
#define BTN_VOLDOWN		SDLK_t
#define BTN_MENU		SDLK_u

const int	HAlignLeft		= 1,
			HAlignRight		= 2,
			HAlignCenter	= 4,
			VAlignTop		= 8,
			VAlignBottom	= 16,
			VAlignMiddle	= 32;

SDL_RWops *rw;
TTF_Font *font = NULL;
SDL_Surface *screen = NULL;
SDL_Surface* img = NULL;
SDL_Rect bgrect;
SDL_Event event;

SDL_Color txtColor = {200, 200, 220};
SDL_Color titleColor = {200, 200, 0};
SDL_Color subTitleColor = {0, 200, 0};
SDL_Color powerColor = {200, 0, 0};

volatile uint32_t *memregs;
volatile uint8_t memdev = 0;
uint16_t mmcPrev, mmcStatus;
uint16_t udcPrev, udcStatus;
uint16_t tvOutPrev, tvOutStatus;
uint16_t phonesPrev, phonesStatus;

static char buf[1024];
uint32_t *mem;

uint8_t *keys;

extern uint8_t rwfont[];

int draw_text(int x, int y, const char buf[64], SDL_Color txtColor, int align) {
	DBG("");

	SDL_Surface *msg = TTF_RenderText_Blended(font, buf, txtColor);

	if (align & HAlignCenter) {
		x -= msg->w / 2;
	} else if (align & HAlignRight) {
		x -= msg->w;
	}

	if (align & VAlignMiddle) {
		y -= msg->h / 2;
	} else if (align & VAlignTop) {
		y -= msg->h;
	}

	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = msg->w;
	rect.h = msg->h;
	SDL_BlitSurface(msg, NULL, screen, &rect);
	SDL_FreeSurface(msg);
	return msg->w;
}

void draw_background(const char buf[64]) {
	DBG("");
	bgrect.w = img->w;
	bgrect.h = img->h;
	bgrect.x = (WIDTH - bgrect.w) / 2;
	bgrect.y = (HEIGHT - bgrect.h) / 2;
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	SDL_BlitSurface(img, NULL, screen, &bgrect);

	// title
	draw_text(620, 2, "RetroFW", titleColor, VAlignBottom | HAlignRight);
	draw_text(20, 2, buf, titleColor, VAlignBottom);
	draw_text(20, 460, "SELECT+START: Exit", txtColor, VAlignMiddle | HAlignLeft);
}

void draw_point(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	// DBG("");
	SDL_Rect rect;
	rect.w = w;
	rect.h = h;
	rect.x = x + bgrect.x;
	rect.y = y + bgrect.y;
	SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 0, 150, 0));
}

void quit(int err) {
	DBG("");
	system("sync");
	if (font) TTF_CloseFont(font);
	font = NULL;
	SDL_Quit();
	TTF_Quit();
	exit(err);
}

uint16_t getMMCStatus() {
	return (memdev > 0 && !(memregs[0x10500 >> 2] >> 0 & 0b1));
}

uint16_t getUDCStatus() {
	return (memdev > 0 && (memregs[0x10300 >> 2] >> 7 & 0b1));
}

uint16_t getTVOutStatus() {
	return (memdev > 0 && !(memregs[0x10300 >> 2] >> 25 & 0b1));
}

uint16_t getPhonesStatus() {
	return (memdev > 0 && !(memregs[0x10300 >> 2] >> 6 & 0b1));
}

void pushEvent() {
	SDL_Event user_event;
	user_event.type = SDL_KEYUP;
	SDL_PushEvent(&user_event);
}

int main(int argc, char* argv[]) {
	DBG("");
	signal(SIGINT, &quit);
	signal(SIGSEGV,&quit);
	signal(SIGTERM,&quit);

	char title[64] = "";
	keys = SDL_GetKeyState(NULL);

	sprintf(title, "IO TESTER");
	printf("%s\n", title);

	setenv("SDL_NOMOUSE", "1", 1);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	SDL_PumpEvents();
	SDL_ShowCursor(0);

	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, SDL_SWSURFACE);

	SDL_EnableKeyRepeat(1, 1);

	if (TTF_Init() == -1) {
		printf("failed to TTF_Init\n");
		return -1;
	}
	rw = SDL_RWFromMem(rwfont, sizeof(rwfont));
	font = TTF_OpenFontRW(rw, 1, 16);
	TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
	TTF_SetFontOutline(font, 0);

	SDL_Surface* _img = IMG_Load("backdrop.png");
	img = SDL_DisplayFormat(_img);
	SDL_FreeSurface(_img);

	int loop = 1, running = 0;
	do {
		draw_background(title);

		int nextline = 16;	

		if (event.key.keysym.sym) {
			sprintf(buf, "Last key: %s", SDL_GetKeyName(event.key.keysym.sym));
			draw_text(24 + 8, 30 + nextline, buf, subTitleColor, VAlignBottom);
			nextline += 16;

			sprintf(buf, "Keysym.sym: %d", event.key.keysym.sym);
			draw_text(24 + 8, 30 + nextline, buf, subTitleColor, VAlignBottom);
			nextline += 16;

			sprintf(buf, "Keysym.scancode: %d", event.key.keysym.scancode);
			draw_text(24 + 8, 30 + nextline, buf, subTitleColor, VAlignBottom);
			nextline += 16;
		}

		// if (keys[BTN_SELECT] && keys[BTN_START]) loop = 0;
		if (keys[BTN_START])   draw_point(159, 346+9, 36, 27);
		if (keys[BTN_SELECT])  draw_point(111, 346+9, 36, 27);
		if (keys[BTN_POWER])   draw_point(293,  63+9, 13, 29);
		if (keys[BTN_L])       draw_point(568, 144+9, 60, 55);
		if (keys[BTN_R])       draw_point(335, 144+9, 60, 55);
		if (keys[BTN_LEFT])    draw_point( 28, 295+9, 28, 28);
		if (keys[BTN_RIGHT])   draw_point( 85, 295+9, 28, 28);
		if (keys[BTN_UP])      draw_point( 56, 266+9, 28, 28);
		if (keys[BTN_DOWN])    draw_point( 56, 323+9, 28, 28);
		if (keys[BTN_A])       draw_point(250, 294+9, 30, 30);
		if (keys[BTN_B])       draw_point(221, 323+9, 30, 30);
		if (keys[BTN_X])       draw_point(221, 264+9, 30, 30);
		if (keys[BTN_Y])       draw_point(192, 294+9, 30, 30);
		if (keys[BTN_L2])      draw_point(516, 144+9, 48, 55);
		if (keys[BTN_R2])      draw_point(400, 144+9, 48, 55);
		if (keys[BTN_VOLUP])   draw_point(  7,  61+9, 10, 24);
		if (keys[BTN_VOLDOWN]) draw_point(  7,  86+9, 10, 24);
		if (keys[BTN_MENU])    draw_point(143,   288, 20, 20);

		SDL_Flip(screen);

		while (SDL_WaitEvent(&event)) {
			if (event.type == SDL_KEYDOWN) {
				if (keys[BTN_SELECT] && keys[BTN_START]) loop = 0;
				break;
			}

			if (event.type == SDL_KEYUP) {
				break;
			}
		}
	} while (loop);

	if (memdev > 0) close(memdev);

	quit(0);
	return 0;
}
