/*
 * Game loader for the Quake port (Data Frog SF3500, TreeFrogUI).
 *
 * Runs before the game. Looks in the port folder for game folders that hold a pak0.pak
 * (id1 = Quake, hipnotic and rogue = the mission packs, any other folder = a mod packed
 * into a pak0.pak), lets the player pick one with the gamepad and saves the choice in
 * game.txt, which run.sh hands to the game.
 *
 * Exit status: 0 = go on and start the game, 10 = leave the port (B, or Select + Start).
 * With only one game to choose from it exits 0 at once and draws nothing.
 *
 * Usage: quakeloader [port-folder] [state-file]
 *
 * Display, input and font code are the small modules the other SF3500 ports use
 * (hwdisp.c, joykey.c, bitfont.c).
 */
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "bitfont.h"
#include "hwdisp.h"
#include "joykey.h"

#define W 640
#define H 480
#define RGB(r, g, b) ((unsigned short)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

#define EXIT_START 0
#define EXIT_LEAVE 10
#define MAXGAMES 64
#define VISIBLE 7

/* Folders the game knows by name; anything else is shown by its folder name. */
static const struct { const char *dir; const char *label; } known[] = {
	{ "id1",     "Quake" },
	{ "hipnotic", "Scourge of Armagon" },
	{ "rogue",   "Dissolution of Eternity" },
	{ "quoth",   "Quoth" },
};
#define NKNOWN ((int)(sizeof(known) / sizeof(known[0])))

typedef struct {
	char dir[64];     /* folder name */
	char pak[64];     /* the pak0 file as it is named on the card */
	char label[64];   /* what the list shows */
	long size;        /* bytes in that pak */
	int order;        /* known games first, in table order */
} game_t;

static game_t games[MAXGAMES];
static int n_games;
static unsigned short fb[W * H];

/* ---------- scanning ---------- */

static int safe_name(const char *n)
{
	int i;
	for (i = 0; n[i]; i++)
		if (!(isalnum((unsigned char)n[i]) || n[i] == '.' || n[i] == '_' || n[i] == '-' || n[i] == ' '))
			return 0;
	return i > 0 && i < 60;
}

static int cmp_game(const void *a, const void *b)
{
	const game_t *x = a, *y = b;
	if (x->order != y->order)
		return x->order - y->order;
	return strcasecmp(x->dir, y->dir);
}

static void scan(const char *root)
{
	DIR *d = opendir(root);
	struct dirent *e;
	if (!d)
		return;
	while ((e = readdir(d)) != NULL && n_games < MAXGAMES) {
		char path[512], inner[512];
		struct stat st;
		DIR *sub;
		struct dirent *f;
		game_t *g = &games[n_games];
		int k;
		if (e->d_name[0] == '.' || !safe_name(e->d_name))
			continue;
		snprintf(path, sizeof(path), "%s/%s", root, e->d_name);
		if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
			continue;
		sub = opendir(path);
		if (!sub)
			continue;
		g->pak[0] = 0;
		while ((f = readdir(sub)) != NULL) {
			if (strcasecmp(f->d_name, "pak0.pak") == 0) {
				snprintf(g->pak, sizeof(g->pak), "%s", f->d_name);
				break;
			}
		}
		closedir(sub);
		if (!g->pak[0])
			continue;
		snprintf(inner, sizeof(inner), "%s/%s", path, g->pak);
		g->size = stat(inner, &st) == 0 ? (long)st.st_size : 0;
		snprintf(g->dir, sizeof(g->dir), "%s", e->d_name);
		g->order = NKNOWN;
		snprintf(g->label, sizeof(g->label), "%s", g->dir);
		for (k = 0; k < NKNOWN; k++) {
			if (strcasecmp(g->dir, known[k].dir) == 0) {
				g->order = k;
				snprintf(g->label, sizeof(g->label), "%s", known[k].label);
			}
		}
		n_games++;
	}
	closedir(d);
	qsort(games, n_games, sizeof(games[0]), cmp_game);
}

/* game.txt holds one line: <folder>/<pak file>. Returns the index it names, or 0. */
static int load_state(const char *file)
{
	char line[200];
	FILE *f = fopen(file, "r");
	int i;
	if (!f)
		return 0;
	if (!fgets(line, sizeof(line), f))
		line[0] = 0;
	fclose(f);
	line[strcspn(line, "\r\n")] = 0;
	for (i = 0; i < n_games; i++) {
		char want[140];
		snprintf(want, sizeof(want), "%s/%s", games[i].dir, games[i].pak);
		if (strcasecmp(want, line) == 0)
			return i;
	}
	return 0;
}

static int save_state(const char *file, int idx)
{
	FILE *f = fopen(file, "w");
	if (!f)
		return 0;
	fprintf(f, "%s/%s\n", games[idx].dir, games[idx].pak);
	return fclose(f) == 0;
}

/* ---------- drawing ---------- */

static const unsigned short C_BG = RGB(22, 14, 10), C_TITLE = RGB(226, 150, 70),
	C_BOX = RGB(52, 36, 28), C_BOXHI = RGB(104, 60, 36), C_TEXT = RGB(240, 232, 220),
	C_DIM = RGB(165, 140, 120), C_FOCUS = RGB(255, 190, 70), C_ERR = RGB(255, 120, 90);

static void fill(int x, int y, int w, int h, unsigned short c)
{
	int xx, yy;
	for (yy = y; yy < y + h && yy < H; yy++) {
		if (yy < 0)
			continue;
		for (xx = x; xx < x + w && xx < W; xx++)
			if (xx >= 0)
				fb[yy * W + xx] = c;
	}
}

static void frame(int x, int y, int w, int h, int t, unsigned short c)
{
	fill(x, y, w, t, c);
	fill(x, y + h - t, w, t, c);
	fill(x, y, t, h, c);
	fill(x + w - t, y, t, h, c);
}

static void text(int x, int y, int scale, unsigned short c, const char *s)
{
	bitfont_draw_string(fb, W, W, H, x, y, scale, 0, c, s);
}

static void text_center(int cx, int y, int scale, unsigned short c, const char *s)
{
	text(cx - bitfont_string_width((int)strlen(s), scale, 0) / 2, y, scale, c, s);
}

static void shorten(char *dst, size_t n, const char *s, int maxchars)
{
	snprintf(dst, n, "%s", s);
	if ((int)strlen(dst) > maxchars) {
		dst[maxchars - 2] = '.';
		dst[maxchars - 1] = '.';
		dst[maxchars] = 0;
	}
}

#define BTN(b) (1u << (b))

static void sleep_ms(int ms)
{
	struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
	nanosleep(&t, NULL);
}

int wl_main(int argc, char **argv)
{
	const char *root = argc > 1 ? argv[1] : ".";
	const char *state = argc > 2 ? argv[2] : "game.txt";
	int sel, top = 0, result = EXIT_START, running = 1;
	unsigned prev, held_dir = 0, held_for = 0;
	char msg[64] = "";

	scan(root);
	if (n_games <= 1)
		return EXIT_START;

	sel = load_state(state);
	if (sel >= VISIBLE)
		top = sel - VISIBLE + 1;

	if (hwdisp_init() != 0 && !hwdisp_active())
		return EXIT_START;
	hwdisp_set_target_aspect(4, 3);
	joykey_init();
	prev = joykey_read();   /* ignore buttons already held at launch */

	while (running) {
		unsigned now = joykey_read(), pressed = now & ~prev, ev = pressed;
		int i, rows;

		/* Auto-repeat for held up / down. */
		if (now & (BTN(JK_UP) | BTN(JK_DOWN))) {
			unsigned dir_bit = now & (BTN(JK_UP) | BTN(JK_DOWN));
			if (dir_bit != held_dir) {
				held_dir = dir_bit;
				held_for = 0;
			} else if (++held_for > 20 && (held_for % 5) == 0) {
				ev |= dir_bit;
			}
		} else {
			held_dir = 0;
			held_for = 0;
		}
		prev = now;

		if ((now & BTN(JK_SELECT)) && (now & BTN(JK_START)) &&
		    (pressed & (BTN(JK_SELECT) | BTN(JK_START)))) {
			result = EXIT_LEAVE;
			break;
		}
		if (ev & BTN(JK_UP) && sel > 0)
			sel--;
		if (ev & BTN(JK_DOWN) && sel < n_games - 1)
			sel++;
		if (sel < top)
			top = sel;
		if (sel > top + VISIBLE - 1)
			top = sel - VISIBLE + 1;
		if (pressed & BTN(JK_B)) {
			result = EXIT_LEAVE;
			break;
		}
		if (pressed & (BTN(JK_A) | BTN(JK_START))) {
			if (save_state(state, sel)) {
				result = EXIT_START;
				break;
			}
			snprintf(msg, sizeof(msg), "Could not save the choice");
		}

		/* ----- draw ----- */
		fill(0, 0, W, H, C_BG);
		text_center(W / 2 + 4, 24 + 4, 6, RGB(10, 4, 2), "QUAKE");
		text_center(W / 2, 24, 6, C_TITLE, "QUAKE");
		text_center(W / 2, 92, 2, C_DIM, "Choose a game");

		rows = n_games < VISIBLE ? n_games : VISIBLE;
		for (i = 0; i < rows; i++) {
			int idx = top + i, y = 128 + i * 40, focus = (idx == sel);
			char line[64], tag[24];
			shorten(line, sizeof(line), games[idx].label, 27);
			snprintf(tag, sizeof(tag), "%ld MB", (games[idx].size + 524288) / 1048576);
			fill(40, y, 560, 36, focus ? C_BOXHI : C_BOX);
			frame(40, y, 560, 36, focus ? 3 : 1, focus ? C_FOCUS : C_DIM);
			text(54, y + 10, 2, focus ? C_FOCUS : C_TEXT, line);
			text(586 - bitfont_string_width((int)strlen(tag), 2, 0), y + 10, 2, C_DIM, tag);
		}
		if (top > 0)
			text(612, 128, 2, C_DIM, "^");
		if (top + rows < n_games)
			text(612, 128 + rows * 40 - 24, 2, C_DIM, "v");

		if (msg[0])
			text_center(W / 2, 424, 2, C_ERR, msg);
		else
			text_center(W / 2, 424, 2, C_DIM, "A  Play        B  Exit");

		hwdisp_present(fb, W, H, W * 2);
		sleep_ms(16);
	}

	hwdisp_deinit();
	return result;
}

#ifndef QL_NO_MAIN
int main(int argc, char **argv)
{
	return wl_main(argc, argv);
}
#endif
