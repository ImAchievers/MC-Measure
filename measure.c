#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_dir(p) mkdir(p, 0755)
#endif

#include "generator.h"
#include "tables/btree263.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "font.h"

#define MC_VERSION MC_NEWEST
#define THREADS 18
#define BG_COLOR 0x000000
#define TILE 64
#define TBITS 6
#define TMASK 63
#define TAREA (TILE * TILE)
#define MAX_CELLS 300000000LL
#define IMG_MAX 8192
#define IMG_PAD 64
#define MAP_ROWS 64
#define RESULTS_FILE "measure_results.txt"
#define IMG_DIR "images"

static const char *BIOME_NAMES[256] = {
    /* surface biomes, sampled at y=256 */
    [ocean] = "Ocean",
    [plains] = "Plains",
    [desert] = "Desert",
    [windswept_hills] = "Windswept Hills",
    [forest] = "Forest",
    [taiga] = "Taiga",
    [swamp] = "Swamp",
    [river] = "River",
    [frozen_ocean] = "Frozen Ocean",
    [frozen_river] = "Frozen River",
    [snowy_plains] = "Snowy Plains",
    [mushroom_fields] = "Mushroom Fields",
    [beach] = "Beach",
    [jungle] = "Jungle",
    [sparse_jungle] = "Sparse Jungle",
    [deep_ocean] = "Deep Ocean",
    [stony_shore] = "Stony Shore",
    [snowy_beach] = "Snowy Beach",
    [birch_forest] = "Birch Forest",
    [dark_forest] = "Dark Forest",
    [snowy_taiga] = "Snowy Taiga",
    [old_growth_pine_taiga] = "Old Growth Pine Taiga",
    [windswept_forest] = "Windswept Forest",
    [savanna] = "Savanna",
    [savanna_plateau] = "Savanna Plateau",
    [badlands] = "Badlands",
    [wooded_badlands] = "Wooded Badlands",
    [warm_ocean] = "Warm Ocean",
    [lukewarm_ocean] = "Lukewarm Ocean",
    [cold_ocean] = "Cold Ocean",
    [deep_lukewarm_ocean] = "Deep Lukewarm Ocean",
    [deep_cold_ocean] = "Deep Cold Ocean",
    [deep_frozen_ocean] = "Deep Frozen Ocean",
    [sunflower_plains] = "Sunflower Plains",
    [windswept_gravelly_hills] = "Windswept Gravelly Hills",
    [flower_forest] = "Flower Forest",
    [ice_spikes] = "Ice Spikes",
    [old_growth_birch_forest] = "Old Growth Birch Forest",
    [old_growth_spruce_taiga] = "Old Growth Spruce Taiga",
    [windswept_savanna] = "Windswept Savanna",
    [eroded_badlands] = "Eroded Badlands",
    [bamboo_jungle] = "Bamboo Jungle",
    [meadow] = "Meadow",
    [grove] = "Grove",
    [snowy_slopes] = "Snowy Slopes",
    [jagged_peaks] = "Jagged Peaks",
    [frozen_peaks] = "Frozen Peaks",
    [stony_peaks] = "Stony Peaks",
    [mangrove_swamp] = "Mangrove Swamp",
    [cherry_grove] = "Cherry Grove",
    [pale_garden] = "Pale Garden",
    [dappled_forest] = "Dappled Forest",

    /* cave biomes, sampled at a specified depth */
    [dripstone_caves] = "Dripstone Caves",
    [lush_caves] = "Lush Caves",
    [deep_dark] = "Deep Dark",
    [sulfur_caves] = "Sulfur Caves",
};

/* default cubiomes viewer colors */
static const uint32_t COLORS[256] = {
    [ocean] = 0x000070,
    [plains] = 0x8db360,
    [desert] = 0xfa9418,
    [windswept_hills] = 0x606060,
    [forest] = 0x056621,
    [taiga] = 0x0b6a5f,
    [swamp] = 0x07f9b2,
    [river] = 0x0000ff,
    [frozen_ocean] = 0x7070d6,
    [frozen_river] = 0xa0a0ff,
    [snowy_plains] = 0xffffff,
    [mushroom_fields] = 0xff00ff,
    [beach] = 0xfade55,
    [jungle] = 0x507b0a,
    [sparse_jungle] = 0x60930f,
    [deep_ocean] = 0x000030,
    [stony_shore] = 0xa2a284,
    [snowy_beach] = 0xfaf0c0,
    [birch_forest] = 0x307444,
    [dark_forest] = 0x40511a,
    [snowy_taiga] = 0x31554a,
    [old_growth_pine_taiga] = 0x596651,
    [windswept_forest] = 0x5b7352,
    [savanna] = 0xbdb25f,
    [savanna_plateau] = 0xa79d64,
    [badlands] = 0xd94515,
    [wooded_badlands] = 0xb09765,
    [warm_ocean] = 0x0000ac,
    [lukewarm_ocean] = 0x000090,
    [cold_ocean] = 0x202070,
    [deep_lukewarm_ocean] = 0x000040,
    [deep_cold_ocean] = 0x202038,
    [deep_frozen_ocean] = 0x404090,
    [sunflower_plains] = 0xb5db88,
    [windswept_gravelly_hills] = 0x888888,
    [flower_forest] = 0x2d8e49,
    [ice_spikes] = 0xb4dcdc,
    [old_growth_birch_forest] = 0x589c6c,
    [old_growth_spruce_taiga] = 0x818e79,
    [windswept_savanna] = 0xe5da87,
    [eroded_badlands] = 0xff6d3d,
    [bamboo_jungle] = 0x849500,
    [meadow] = 0x60a445,
    [grove] = 0x47726c,
    [snowy_slopes] = 0xc4c4c4,
    [jagged_peaks] = 0xdcdcc8,
    [frozen_peaks] = 0xb0b3ce,
    [stony_peaks] = 0x7b8f74,
    [mangrove_swamp] = 0x2ccc8e,
    [cherry_grove] = 0xff91c8,
    [pale_garden] = 0x696d95,
    [dappled_forest] = 0xdf6827,
    [dripstone_caves] = 0x4e3012,
    [lush_caves] = 0x283c00,
    [deep_dark] = 0x031f29,
    [sulfur_caves] = 0xc8d232,
};

/* biomes that bridge across rivers */
static const uint8_t BRIDGE[256] = {
    [plains] = 1,
    [desert] = 1,
    [forest] = 1,
    [dark_forest] = 1,
    [taiga] = 1,
    [snowy_taiga] = 1,
    [snowy_plains] = 1,
    [savanna] = 1,
    [badlands] = 1,
    [wooded_badlands] = 1,
    [beach] = 1,
    [snowy_beach] = 1,
    [stony_shore] = 1,
    [grove] = 1,
    [snowy_slopes] = 1,
};

static inline int64_t tkey(int tx, int tz){ return ((int64_t)(uint32_t)tx << 32) | (uint32_t)tz; }
static inline uint64_t mix(uint64_t x){ x ^= x>>30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x>>27; x *= 0x94d049bb133111ebULL; x ^= x>>31; return x; }

typedef struct { int tx, tz; uint8_t q; int16_t *biome; uint8_t *vis; } Tile;

static Tile *tiles;
static size_t ntiles, tcap;
static int64_t *hkey;
static int *hidx;
static size_t hcap;

static void hgrow(void)
{
    size_t nc = hcap ? hcap << 1 : (1 << 12), m = nc - 1;
    int64_t *nk = malloc(nc * sizeof(int64_t));
    int *ni = malloc(nc * sizeof(int));
    for (size_t i = 0; i < nc; i++) ni[i] = -1;
    for (size_t i = 0; i < hcap; i++)
    {
        if (hidx[i] < 0) continue;
        size_t j = mix((uint64_t)hkey[i]) & m;
        while (ni[j] >= 0) j = (j + 1) & m;
        nk[j] = hkey[i]; ni[j] = hidx[i];
    }
    free(hkey); free(hidx); hkey = nk; hidx = ni; hcap = nc;
}

static int tile_lookup(int tx, int tz)
{
    if (!hcap) return -1;
    int64_t k = tkey(tx, tz);
    size_t m = hcap - 1, i = mix((uint64_t)k) & m;
    while (hidx[i] >= 0) { if (hkey[i] == k) return hidx[i]; i = (i + 1) & m; }
    return -1;
}

static int tile_new(int tx, int tz)
{
    if ((ntiles + 1) * 10 >= hcap * 7) hgrow();
    if (ntiles == tcap) { tcap = tcap ? tcap * 2 : 1024; tiles = realloc(tiles, tcap * sizeof(Tile)); }
    int idx = (int)ntiles++;
    tiles[idx].tx = tx; tiles[idx].tz = tz; tiles[idx].q = 0; tiles[idx].biome = NULL; tiles[idx].vis = NULL;
    int64_t k = tkey(tx, tz);
    size_t m = hcap - 1, i = mix((uint64_t)k) & m;
    while (hidx[i] >= 0) i = (i + 1) & m;
    hkey[i] = k; hidx[i] = idx;
    return idx;
}

static uint64_t g_seed;
static int g_lb, g_sy;

/* the biome tree as a lookup table, based on the JUNO Project genlut.c by bryce219 */
static const int LUT_AXIS[5] = {0, 1, 2, 3, 5};
static int32_t g_cut[5][64];
static int g_ncut[5];
static long g_cells;
static uint8_t *g_lut;
static atomic_long g_lutnext;

static int cmp_int(const void *a, const void *b)
{
    int32_t x = *(const int32_t *)a, y = *(const int32_t *)b;
    return (x > y) - (x < y);
}

static void *lut_worker(void *arg)
{
    (void)arg;
    long i;
    while ((i = atomic_fetch_add(&g_lutnext, 4096)) < g_cells)
    {
        long end = i + 4096 < g_cells ? i + 4096 : g_cells;
        for (; i < end; i++)
        {
            uint64_t np[6];
            long r = i;
            for (int a = 4; a >= 0; a--)
            {
                int c = (int)(r % (g_ncut[a] + 1));
                r /= g_ncut[a] + 1;
                np[LUT_AXIS[a]] = (uint64_t)(int64_t)(c ? g_cut[a][c - 1] : g_cut[a][0] - 1);
            }
            np[4] = (uint64_t)(int64_t)-15000;
            g_lut[i] = (uint8_t)climateToBiome(MC_VERSION, np, NULL);
        }
    }
    return NULL;
}

static void build_lut(void)
{
    int n = sizeof(btree263_nodes) / sizeof(uint64_t);
    int32_t *e = malloc(2 * n * sizeof(int32_t));
    g_cells = 1;
    for (int a = 0; a < 5; a++)
    {
        int m = 0;
        for (int i = 0; i < n; i++)
        {
            if ((btree263_nodes[i] >> 48) < (uint64_t)n) continue;
            int p = (btree263_nodes[i] >> (8 * LUT_AXIS[a])) & 0xff;
            e[m++] = btree263_param[p][0];
            e[m++] = btree263_param[p][1] + 1;
        }
        qsort(e, m, sizeof(int32_t), cmp_int);
        g_ncut[a] = 0;
        for (int i = 0; i < m; i++)
            if (!i || e[i] != e[i - 1]) g_cut[a][g_ncut[a]++] = e[i];
        g_cells *= g_ncut[a] + 1;
    }
    free(e);
    g_lut = malloc(g_cells);
    atomic_store(&g_lutnext, 0);
    pthread_t th[THREADS];
    for (int t = 0; t < THREADS; t++) pthread_create(&th[t], NULL, lut_worker, NULL);
    for (int t = 0; t < THREADS; t++) pthread_join(th[t], NULL);
}

static int lut_biome(int mc, int64_t *np)
{
    if (np[4] > 0 || mc != MC_26_3) return climateToBiome(mc, (const uint64_t *)np, NULL);
    long i = 0;
    for (int a = 0; a < 5; a++)
    {
        int64_t v = np[LUT_AXIS[a]];
        int c = 0;
        while (c < g_ncut[a] && v >= g_cut[a][c]) c++;
        if (c == 0 || c == g_ncut[a]) return climateToBiome(mc, (const uint64_t *)np, NULL);
        i = i * (g_ncut[a] + 1) + c;
    }
    return g_lut[i];
}

static void gen_tile(Generator *g, Tile *t)
{
    int16_t *b = malloc(TAREA * sizeof(int16_t));
    int64_t np[6];
    for (int i = 0; i < TAREA; i++)
    {
        int x = (t->tx << TBITS) + (i & TMASK), z = (t->tz << TBITS) + (i >> TBITS);
        sampleBiomeNoise(&g->bn, np, x, g_sy, z, NULL, SAMPLE_NO_BIOME);
        b[i] = (int16_t)lut_biome(g->mc, np);
    }
    t->biome = b;
}

static int *job;
static size_t jobn;
static atomic_size_t jobnext;

static void *gen_worker(void *a)
{
    (void)a;
    Generator g;
    setupGenerator(&g, MC_VERSION, g_lb ? LARGE_BIOMES : 0);
    applySeed(&g, DIM_OVERWORLD, g_seed);
    size_t i;
    while ((i = atomic_fetch_add(&jobnext, 1)) < jobn)
        gen_tile(&g, &tiles[job[i]]);
    return NULL;
}

static void gen_batch(int *idxs, size_t n)
{
    if (!n) return;
    job = idxs; jobn = n; atomic_store(&jobnext, 0);
    int nth = (size_t)THREADS > n ? (int)n : THREADS;
    pthread_t th[nth];
    for (int t = 0; t < nth; t++) pthread_create(&th[t], NULL, gen_worker, NULL);
    for (int t = 0; t < nth; t++) pthread_join(th[t], NULL);
}

static int *stx, *stz;
static size_t scap, sn;

static void spush(int x, int z)
{
    if (sn == scap) { scap = scap ? scap * 2 : (1 << 20); stx = realloc(stx, scap * sizeof(int)); stz = realloc(stz, scap * sizeof(int)); }
    stx[sn] = x; stz[sn] = z; sn++;
}

static int *wtx, *wtz;
static size_t wcap, wn;

static void wpush(int x, int z)
{
    if (wn == wcap) { wcap = wcap ? wcap * 2 : (1 << 16); wtx = realloc(wtx, wcap * sizeof(int)); wtz = realloc(wtz, wcap * sizeof(int)); }
    wtx[wn] = x; wtz[wn] = z; wn++;
}

static int *pend;
static size_t pendn, pendcap;

static void pend_add(int idx)
{
    if (pendn == pendcap) { pendcap = pendcap ? pendcap * 2 : 4096; pend = realloc(pend, pendcap * sizeof(int)); }
    pend[pendn++] = idx;
}

static void flush_pending(void)
{
    gen_batch(pend, pendn);
    for (size_t i = 0; i < pendn; i++) tiles[pend[i]].q = 0;
    pendn = 0;
}

static int cache_tx = INT_MIN, cache_tz = INT_MIN, cache_idx = -1;

static int tile_slot(int tx, int tz)
{
    if (tx == cache_tx && tz == cache_tz) return cache_idx;
    int idx = tile_lookup(tx, tz);
    if (idx < 0) idx = tile_new(tx, tz);
    cache_tx = tx; cache_tz = tz; cache_idx = idx;
    return idx;
}

static inline int floor_div(int a, int b){ return a >= 0 ? a / b : -((-a + b - 1) / b); }

static void put(uint8_t *p, int id)
{
    uint32_t c = id >= 0 && id < 256 ? COLORS[id] : 0;
    p[0] = c >> 16; p[1] = c >> 8; p[2] = c;
}

typedef struct { uint8_t *p; int w, h; } Img;

static void fill_rect(Img *im, int x, int y, int w, int h, const uint8_t *col)
{
    for (int j = y < 0 ? 0 : y; j < y + h && j < im->h; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < im->w; i++)
            memcpy(im->p + ((size_t)j * im->w + i) * 3, col, 3);
}

static int text_width(const char *s, int sc)
{
    int n = (int)strlen(s);
    return n ? (6 * n - 1) * sc : 0;
}

static void draw_text(Img *im, int x, int y, int sc, const char *s, const uint8_t *col)
{
    for (; *s; s++)
    {
        if (*s < 32 || *s >= 127) continue;
        const uint8_t *gl = FONT[*s - 32];
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 5; c++)
                if (gl[r] >> c & 1) fill_rect(im, x + c * sc, y + r * sc, sc, sc, col);
        x += 6 * sc;
    }
}

static void commas(char *buf, long long v)
{
    char t[32];
    int n = snprintf(t, sizeof t, "%lld", v), k = 0;
    for (int i = 0; i < n; i++)
    {
        if (i && (n - i) % 3 == 0) buf[k++] = ',';
        buf[k++] = t[i];
    }
    buf[k] = 0;
}

/* full biome map */
static int16_t *g_ids;
static int g_mx, g_mz, g_mw, g_mh, g_ms, g_my;
static atomic_int g_row;

static void *map_worker(void *a)
{
    (void)a;
    Generator g;
    setupGenerator(&g, MC_VERSION, g_lb ? LARGE_BIOMES : 0);
    applySeed(&g, DIM_OVERWORLD, g_seed);
    int *buf = malloc((size_t)g_mw * MAP_ROWS * sizeof(int));
    int r0;
    while ((r0 = atomic_fetch_add(&g_row, MAP_ROWS)) < g_mh)
    {
        int n = g_mh - r0 < MAP_ROWS ? g_mh - r0 : MAP_ROWS;
        Range r = {g_ms, g_mx, g_mz + r0, g_mw, n, g_my, 1};
        genBiomes(&g, buf, r);
        for (size_t i = 0; i < (size_t)g_mw * n; i++) g_ids[(size_t)r0 * g_mw + i] = (int16_t)buf[i];
    }
    free(buf);
    return NULL;
}

static void gen_map(int x0, int z0, int w, int h, int f)
{
    g_ids = malloc((size_t)w * h * sizeof(int16_t));
    g_mx = x0; g_mz = z0; g_mw = w; g_mh = h; g_ms = 4 * f; g_my = g_sy;
    atomic_store(&g_row, 0);
    pthread_t th[THREADS];
    for (int t = 0; t < THREADS; t++) pthread_create(&th[t], NULL, map_worker, NULL);
    for (int t = 0; t < THREADS; t++) pthread_join(th[t], NULL);
}

static const uint8_t BG[3] = { BG_COLOR >> 16 & 255, BG_COLOR >> 8 & 255, BG_COLOR & 255 };

/* the width and height brackets just outside the map, same for both images so they line up */
typedef struct
{
    int sc, m, gap, tick, lw, th, ww, hw;
    int px0, pz0, px1, pz1, vx, hy, vin, hin, vlx, vly, hlx, hly;
    int minx, minz, maxx, maxz;
    char wl[48], hl[48];
} Marks;

static void place(Marks *k, int w, int h)
{
    int big = w > h ? w : h, sc = (big + 240) / 480;
    if (sc < 1) sc = 1;
    k->sc = sc; k->m = 6 * sc; k->gap = 4 * sc; k->tick = 3 * sc; k->lw = sc / 2 < 1 ? 1 : sc / 2; k->th = 7 * sc;
    k->ww = text_width(k->wl, sc); k->hw = text_width(k->hl, sc);

    /* the side label goes to the left of the bracket and the bottom one in its bracket if it fits or underneath if not */
    k->vx = -10 * sc - k->lw;
    k->hy = h + 10 * sc + k->th / 2;
    k->vin = k->th + 2 * k->gap < k->pz1 - k->pz0;
    k->hin = k->ww + 2 * k->gap < k->px1 - k->px0;
    k->vlx = k->vx - k->gap - k->hw;
    k->vly = (k->pz0 + k->pz1) / 2 - k->th / 2;
    k->hlx = (k->px0 + k->px1) / 2 - k->ww / 2;
    k->hly = k->hin ? k->hy - k->th / 2 : k->hy + k->gap;

    k->minx = k->vlx - k->m;
    if (k->hlx - k->m < k->minx) k->minx = k->hlx - k->m;
    k->minz = -(17 * sc + 2 * k->m);
    k->maxx = w + k->m;
    if (k->hlx + k->ww + k->m > k->maxx) k->maxx = k->hlx + k->ww + k->m;
    k->maxz = k->hly + k->th + k->m;
}

static Img annotate(Img src, const Marks *k, const char *l1, const char *l2)
{
    const uint8_t light[3] = {200, 204, 212}, dark[3] = {48, 50, 56};
    const uint8_t *col = BG[0] + BG[1] + BG[2] > 384 ? dark : light;
    Img im = { NULL, k->maxx - k->minx, k->maxz - k->minz };
    im.p = malloc((size_t)im.w * im.h * 3);
    for (size_t i = 0; i < (size_t)im.w * im.h; i++) memcpy(im.p + i * 3, BG, 3);
    int ox = -k->minx, oz = -k->minz;
    for (int j = 0; j < src.h; j++) memcpy(im.p + ((size_t)(j + oz) * im.w + ox) * 3, src.p + (size_t)j * src.w * 3, (size_t)src.w * 3);

    int sc = k->sc, gap = k->gap, lw = k->lw, th = k->th, tick = k->tick, ww = k->ww;
    int vx = k->vx + ox, vly = k->vly + oz, hy = k->hy + oz, hlx = k->hlx + ox;
    int vz0 = k->pz0 + oz, vz1 = k->pz1 + 1 + oz, hx0 = k->px0 + ox, hx1 = k->px1 + 1 + ox;
    if (k->vin)
    {
        fill_rect(&im, vx, vz0, lw, vly - gap - vz0, col);
        fill_rect(&im, vx, vly + th + gap, lw, vz1 - vly - th - gap, col);
    }
    else fill_rect(&im, vx, vz0, lw, vz1 - vz0, col);
    fill_rect(&im, vx, vz0, tick, lw, col);
    fill_rect(&im, vx, vz1 - lw, tick, lw, col);
    if (k->hin)
    {
        fill_rect(&im, hx0, hy, hlx - gap - hx0, lw, col);
        fill_rect(&im, hlx + ww + gap, hy, hx1 - hlx - ww - gap, lw, col);
    }
    else fill_rect(&im, hx0, hy, hx1 - hx0, lw, col);
    fill_rect(&im, hx0, hy - tick + lw, lw, tick, col);
    fill_rect(&im, hx1 - lw, hy - tick + lw, lw, tick, col);
    draw_text(&im, k->vlx + ox, vly, sc, k->hl, col);
    draw_text(&im, hlx, k->hly + oz, sc, k->wl, col);
    draw_text(&im, k->m, k->m, sc, l1, col);
    draw_text(&im, k->m, k->m + 10 * sc, sc, l2, col);
    return im;
}

/* the normal biome map and the biome only map, on the same canvas so they line up */
static void write_images(const char *base, const char *l1, const char *l2, int target, const int *gap, int f, int x0, int z0, int x1, int z1)
{
    Marks k;
    char num[32];
    commas(num, (long long)(x1 - x0 + 1) * 4); snprintf(k.wl, sizeof k.wl, "%s blocks", num);
    commas(num, (long long)(z1 - z0 + 1) * 4); snprintf(k.hl, sizeof k.hl, "%s blocks", num);
    int mx0 = floor_div(x0 - IMG_PAD, f), mz0 = floor_div(z0 - IMG_PAD, f);
    k.px0 = floor_div(x0, f) - mx0; k.pz0 = floor_div(z0, f) - mz0;
    k.px1 = floor_div(x1, f) - mx0; k.pz1 = floor_div(z1, f) - mz0;
    int w = floor_div(x1 + IMG_PAD, f) - mx0 + 1, h = floor_div(z1 + IMG_PAD, f) - mz0 + 1;
    size_t n = (size_t)w * h;
    char path[300];

    /* pixels with at least one counted or bridged cell in them */
    uint8_t *in = calloc(n, 1);
    for (size_t t = 0; t < ntiles; t++)
    {
        if (!tiles[t].vis) continue;
        for (int i = 0; i < TAREA; i++)
        {
            if (tiles[t].vis[i] < 2) continue;
            int px = floor_div((tiles[t].tx << TBITS) + (i & TMASK), f) - mx0;
            int pz = floor_div((tiles[t].tz << TBITS) + (i >> TBITS), f) - mz0;
            in[(size_t)pz * w + px] = 1;
        }
    }

    gen_map(mx0, mz0, w, h, f);
    uint8_t *map = malloc(n * 3), *only = malloc(n * 3);
    for (size_t i = 0; i < n; i++)
    {
        int id = g_ids[i];
        put(map + i * 3, id);
        if (in[i] && (id == target || id == gap[0] || id == gap[1] || id == gap[2] || id == gap[3])) put(only + i * 3, id);
        else memcpy(only + i * 3, BG, 3);
    }

    Img ms = { map, w, h }, os = { only, w, h };
    place(&k, w, h);
    Img im = annotate(ms, &k, l1, l2);
    snprintf(path, sizeof path, IMG_DIR "/%s_map.png", base);
    stbi_write_png(path, im.w, im.h, 3, im.p, im.w * 3);
    free(im.p);
    im = annotate(os, &k, l1, l2);
    snprintf(path, sizeof path, IMG_DIR "/%s_only.png", base);
    stbi_write_png(path, im.w, im.h, 3, im.p, im.w * 3);
    free(im.p); free(in); free(g_ids); free(map); free(only);

    printf("Images: " IMG_DIR "/%s_only.png, " IMG_DIR "/%s_map.png (1:%d, %dx%d)\n", base, base, 4 * f, im.w, im.h);
}

/* the biome map centered on the coords with a ring or box drawn */
static void write_ring(const char *base, int box, int r, int bx, int bz, int img)
{
    int rc = (box ? r / 2 : r) / 4 + 1, cx = floor_div(bx, 4), cz = floor_div(bz, 4);
    int f = img == 16 ? 4 : 1;
    if (img != 4) while ((2 * (rc + IMG_PAD)) / f > IMG_MAX && f < 16) f *= 4;
    int x0 = floor_div(cx - rc - IMG_PAD, f), z0 = floor_div(cz - rc - IMG_PAD, f);
    int x1 = floor_div(cx + rc + IMG_PAD, f), z1 = floor_div(cz + rc + IMG_PAD, f);
    int w = x1 - x0 + 1, h = z1 - z0 + 1;
    size_t n = (size_t)w * h;

    gen_map(x0, z0, w, h, f);
    uint8_t *pix = malloc(n * 3);
    for (size_t i = 0; i < n; i++) put(pix + i * 3, g_ids[i]);

    const uint8_t red[3] = {230, 30, 30};
    double bpp = 4.0 * f, half = (w > 2500 ? 3 : w > 800 ? 2 : 1) * bpp / 2;
    for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
    {
        double dx = (x0 + i + 0.5) * bpp - bx, dz = (z0 + j + 0.5) * bpp - bz;
        double d = box ? (fabs(dx) > fabs(dz) ? fabs(dx) : fabs(dz)) : sqrt(dx * dx + dz * dz);
        if (fabs(d - (box ? r / 2.0 : r)) <= half) memcpy(pix + ((size_t)j * w + i) * 3, red, 3);
    }

    char path[300];
    snprintf(path, sizeof path, IMG_DIR "/%s_%s%d.png", base, box ? "box" : "ring", r);
    stbi_write_png(path, w, h, 3, pix, w * 3);
    free(g_ids); free(pix);
    printf("Image: %s (1:%d, %dx%d)\n", path, 4 * f, w, h);
}

/* rivers generate where weirdness is within 0.05 of 0, this decides what would generate if the river didn't exist */
static int under_river(Generator *g, int x, int z)
{
    int64_t np[6];
    sampleBiomeNoise(&g->bn, np, x, g_sy, z, NULL, SAMPLE_NO_BIOME);
    if (np[5] > 500 || np[5] < -500) return -1;
    np[5] = np[5] >= 0 ? 501 : -501;
    return lut_biome(g->mc, np);
}

static void reset_state(void)
{
    for (size_t t = 0; t < ntiles; t++) { free(tiles[t].biome); free(tiles[t].vis); }
    ntiles = 0;
    for (size_t i = 0; i < hcap; i++) hidx[i] = -1;
    sn = wn = pendn = 0;
    cache_tx = cache_tz = INT_MIN; cache_idx = -1;
}

static int is_num(const char *s)
{
    char *e;
    strtoll(s, &e, 10);
    return *s && !*e;
}

static void usage(const char *exe)
{
    fprintf(stderr, "usage: %s <seed> [x z] [LB] [y] [img|img4|img16] [ring <radius>|box <side>]\n", exe);
    fprintf(stderr, "       %s list <file>\n", exe);
}

static int measure(int argc, char **argv, const char *exe)
{
    if (argc < 1 || !is_num(argv[0])) { usage(exe); return 1; }
    uint64_t seed = (uint64_t)strtoll(argv[0], NULL, 10);
    int bx = 0, bz = 0, i = 1;
    if (argc >= 3 && is_num(argv[1]) && is_num(argv[2])) { bx = atoi(argv[1]); bz = atoi(argv[2]); i = 3; }
    int lb = 0, yset = 0, by = 256, img = 0, ring = 0, box = 0;
    for (; i < argc; i++)
    {
        if (!strcmp(argv[i], "LB") || !strcmp(argv[i], "lb")) lb = 1;
        else if (!strcmp(argv[i], "img")) img = 1;
        else if (!strcmp(argv[i], "img4")) img = 4;
        else if (!strcmp(argv[i], "img16")) img = 16;
        else if (!strcmp(argv[i], "ring") && i + 1 < argc && is_num(argv[i + 1]) && atoi(argv[i + 1]) > 0) ring = atoi(argv[++i]);
        else if (!strcmp(argv[i], "box") && i + 1 < argc && is_num(argv[i + 1]) && atoi(argv[i + 1]) > 0) box = atoi(argv[++i]);
        else if (is_num(argv[i])) { by = atoi(argv[i]); yset = 1; }
        else { usage(exe); return 1; }
    }

    g_seed = seed; g_lb = lb; g_sy = by >> 2;
    int scx = bx >> 2, scz = bz >> 2;
    reset_state();

    Generator g;
    setupGenerator(&g, MC_VERSION, lb ? LARGE_BIOMES : 0);
    applySeed(&g, DIM_OVERWORLD, seed);

    /* determine the biome at the given block then step to the nearest 1:4 cell of it. */
    int target = getBiomeAt(&g, 1, bx, by, bz);
    if (getBiomeAt(&g, 4, scx, g_sy, scz) != target)
    {
        int found = 0;
        for (int r = 1; r <= 24 && !found; r++)
        for (int dx = -r; dx <= r && !found; dx++)
        for (int dz = -r; dz <= r && !found; dz++)
        {
            if (abs(dx) != r && abs(dz) != r) continue;
            if (getBiomeAt(&g, 4, scx + dx, g_sy, scz + dz) == target) { scx += dx; scz += dz; found = 1; }
        }
        if (!found) target = getBiomeAt(&g, 4, scx, g_sy, scz);
    }

    int s0 = tile_new(scx >> TBITS, scz >> TBITS);
    gen_tile(&g, &tiles[s0]);
    int bridge = target >= 0 && target < 256 && BRIDGE[target];
    /* grove/snowy slopes generate these instead of rivers */
    int b0 = river, b1 = frozen_river;
    if (target == grove) { b0 = snowy_taiga; b1 = taiga; }
    else if (target == snowy_slopes) { b0 = snowy_plains; b1 = ice_spikes; }

    long long count = 0;
    int capped = 0, x0 = scx, z0 = scz, x1 = scx, z1 = scz;
    int dxu[4] = {0, 0, -1, 1}, dzu[4] = {-1, 1, 0, 0};
    tiles[s0].vis = calloc(TAREA, 1);
    tiles[s0].vis[(scz & TMASK) * TILE + (scx & TMASK)] = 1;
    spush(scx, scz);

    while (sn || wn)
    {
        if (!sn)
        {
            flush_pending();
            for (size_t k = 0; k < wn; k++) spush(wtx[k], wtz[k]);
            wn = 0;
        }
        sn--;
        int x = stx[sn], z = stz[sn];
        int ti = tile_slot(x >> TBITS, z >> TBITS);
        if (!tiles[ti].biome) { wpush(x, z); continue; }
        int bm = tiles[ti].biome[(z & TMASK) * TILE + (x & TMASK)];
        if (bm == target) { if (++count >= MAX_CELLS) { capped = 1; break; } }
        else if (!bridge || (bm != river && bm != frozen_river && bm != b0 && bm != b1) || under_river(&g, x, z) != target) continue;
        tiles[ti].vis[(z & TMASK) * TILE + (x & TMASK)] = 2;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (z < z0) z0 = z;
        if (z > z1) z1 = z;
        for (int k = 0; k < 4; k++)
        {
            int nx = x + dxu[k], nz = z + dzu[k];
            int nti = tile_slot(nx >> TBITS, nz >> TBITS);
            Tile *nt = &tiles[nti];
            if (!nt->biome && !nt->q) { nt->q = 1; pend_add(nti); }
            if (!nt->vis) nt->vis = calloc(TAREA, 1);
            int nloc = (nz & TMASK) * TILE + (nx & TMASK);
            if (nt->vis[nloc]) continue;
            nt->vis[nloc] = 1;
            spush(nx, nz);
        }
    }

    if (capped)
    {
        fprintf(stderr, "%lld %d %d: patch exceeds %lld cells at this point; check the coordinates and Y depth\n",
            (long long)seed, bx, bz, (long long)MAX_CELLS);
        return 1;
    }

    char nm[64], ys[32] = "";
    if (target >= 0 && target < 256 && BIOME_NAMES[target]) snprintf(nm, sizeof nm, "%s%s", BIOME_NAMES[target], lb ? " LB" : "");
    else snprintf(nm, sizeof nm, "id %d%s", target, lb ? " LB" : "");
    if (yset) snprintf(ys, sizeof ys, "Y=%d | ", by);
    printf("Biome: %s | %s1:4 Size: %lld | Blocks^2 Size: %lld\n", nm, ys, count, count * 16);
    FILE *out = fopen(RESULTS_FILE, "a");
    if (out)
    {
        fprintf(out, "Seed: %lld | Coords: %d %d | Biome: %s | %s1:4 Size: %lld | Blocks^2 Size: %lld\n",
            (long long)seed, bx, bz, nm, ys, count, count * 16);
        fclose(out);
    }

    if (img || ring || box)
    {
        make_dir(IMG_DIR);
        char base[160], nb[64];
        snprintf(nb, sizeof nb, "%s", nm);
        for (char *c = nb; *c; c++) if (*c == ' ') *c = '_';
        int n = snprintf(base, sizeof base, "%lld_%s", (long long)seed, nb);
        if (yset) snprintf(base + n, sizeof base - n, "_y%d", by);
        if (img)
        {
            int f = img == 16 ? 4 : 1, span = x1 - x0 > z1 - z0 ? x1 - x0 : z1 - z0;
            if (img == 1) while ((span + 2 * IMG_PAD) / f > IMG_MAX && f < 16) f *= 4;
            int gap[4] = {-1, -1, -1, -1};
            if (bridge) { gap[0] = river; gap[1] = frozen_river; gap[2] = b0; gap[3] = b1; }
            char l1[64], l2[64];
            n = snprintf(l1, sizeof l1, "%lld%s", (long long)seed, lb ? " [LB]" : "");
            if (yset) snprintf(l1 + n, sizeof l1 - n, " [Y=%d]", by);
            snprintf(l2, sizeof l2, "%d %d", bx, bz);
            write_images(base, l1, l2, target, gap, f, x0, z0, x1, z1);
        }
        if (ring) write_ring(base, 0, ring, bx, bz, img);
        if (box) write_ring(base, 1, box, bx, bz, img);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || (!strcmp(argv[1], "list") && argc < 3)) { usage(argv[0]); return 1; }
    build_lut();
    if (strcmp(argv[1], "list")) return measure(argc - 1, argv + 1, argv[0]);

    FILE *in = fopen(argv[2], "r");
    if (!in) { perror(argv[2]); return 1; }
    char line[512];
    int done = 0, failed = 0;
    while (fgets(line, sizeof line, in))
    {
        char *tok[16];
        int nt = 0;
        for (char *t = strtok(line, " \t\r\n"); t && nt < 16; t = strtok(NULL, " \t\r\n")) tok[nt++] = t;
        if (!nt || !is_num(tok[0])) continue;
        printf("%s: ", tok[0]);
        fflush(stdout);
        if (measure(nt, tok, argv[0])) failed++;
        else done++;
    }
    fclose(in);
    if (failed) printf("Done: %d measured, %d failed\n", done, failed);
    else printf("Done: %d measured\n", done);
    return 0;
}
