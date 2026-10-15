#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>

#include "generator.h"
#include "tables/btree263.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MC_VERSION MC_NEWEST
#define THREADS 18
#define TILE 64
#define TBITS 6
#define TMASK 63
#define TAREA (TILE * TILE)
#define MAX_CELLS 300000000LL
#define IMG_MAX 8192
#define IMG_PAD 64
#define MAP_ROWS 64

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

/* two images of the same area: connected cells only on a black background and the normal biomes */
static void write_images(const char *base, int target, const int *gap, int f, int x0, int z0, int x1, int z1)
{
    x0 = floor_div(x0 - IMG_PAD, f); z0 = floor_div(z0 - IMG_PAD, f);
    x1 = floor_div(x1 + IMG_PAD, f); z1 = floor_div(z1 + IMG_PAD, f);
    int w = x1 - x0 + 1, h = z1 - z0 + 1;
    size_t n = (size_t)w * h;
    char path[256];

    /* pixels with at least one counted or bridged cell in them */
    uint8_t *in = calloc(n, 1);
    for (size_t t = 0; t < ntiles; t++)
    {
        if (!tiles[t].vis) continue;
        for (int i = 0; i < TAREA; i++)
        {
            if (tiles[t].vis[i] < 2) continue;
            int px = floor_div((tiles[t].tx << TBITS) + (i & TMASK), f) - x0;
            int pz = floor_div((tiles[t].tz << TBITS) + (i >> TBITS), f) - z0;
            in[(size_t)pz * w + px] = 1;
        }
    }

    g_ids = malloc(n * sizeof(int16_t));
    g_mx = x0; g_mz = z0; g_mw = w; g_mh = h; g_ms = 4 * f; g_my = g_sy;
    atomic_store(&g_row, 0);
    pthread_t th[THREADS];
    for (int t = 0; t < THREADS; t++) pthread_create(&th[t], NULL, map_worker, NULL);
    for (int t = 0; t < THREADS; t++) pthread_join(th[t], NULL);

    uint8_t *pix = malloc(n * 3);
    for (size_t i = 0; i < n; i++) put(pix + i * 3, g_ids[i]);
    snprintf(path, sizeof path, "%s_map.png", base);
    stbi_write_png(path, w, h, 3, pix, w * 3);

    /* biome and bridged rivers */
    for (size_t i = 0; i < n; i++)
    {
        int id = g_ids[i];
        if (in[i] && (id == target || id == gap[0] || id == gap[1] || id == gap[2] || id == gap[3])) put(pix + i * 3, id);
        else memset(pix + i * 3, 0, 3);
    }
    snprintf(path, sizeof path, "%s_only.png", base);
    stbi_write_png(path, w, h, 3, pix, w * 3);
    free(in); free(g_ids); free(pix);

    printf("Images: %s_only.png, %s_map.png (1:%d, %dx%d)\n", base, base, 4 * f, w, h);
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

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <seed> <x> <z> [LB] [y] [img|img4|img16]\n", argv[0]);
        return 1;
    }
    uint64_t seed = (uint64_t)strtoll(argv[1], NULL, 10);
    int bx = atoi(argv[2]);
    int bz = atoi(argv[3]);
    int lb = 0, yset = 0, by = 256, img = 0;
    for (int i = 4; i < argc; i++)
    {
        if (!strcmp(argv[i], "LB") || !strcmp(argv[i], "lb")) lb = 1;
        else if (!strcmp(argv[i], "img")) img = 1;
        else if (!strcmp(argv[i], "img4")) img = 4;
        else if (!strcmp(argv[i], "img16")) img = 16;
        else { by = atoi(argv[i]); yset = 1; }
    }

    g_seed = seed; g_lb = lb; g_sy = by >> 2;
    int scx = bx >> 2, scz = bz >> 2;
    build_lut();

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
            for (size_t i = 0; i < wn; i++) spush(wtx[i], wtz[i]);
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
        for (int i = 0; i < 4; i++)
        {
            int nx = x + dxu[i], nz = z + dzu[i];
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
        fprintf(stderr, "patch exceeds %lld cells at this point; check the coordinates and Y depth\n", (long long)MAX_CELLS);
        return 1;
    }

    const char *nm = (target >= 0 && target < 256) ? BIOME_NAMES[target] : NULL;
    char ys[32] = "";
    if (yset) snprintf(ys, sizeof ys, "Y=%d | ", by);
    if (nm)
        printf("Biome: %s%s | %s1:4 Size: %lld | Blocks^2 Size: %lld\n", nm, lb ? " LB" : "", ys, count, count * 16);
    else
        printf("Biome: id %d%s | %s1:4 Size: %lld | Blocks^2 Size: %lld\n", target, lb ? " LB" : "", ys, count, count * 16);

    if (img)
    {
        int f = img == 16 ? 4 : 1, span = x1 - x0 > z1 - z0 ? x1 - x0 : z1 - z0;
        if (img == 1) while ((span + 2 * IMG_PAD) / f > IMG_MAX && f < 16) f *= 4;
        char base[160], nb[64];
        if (nm) { snprintf(nb, sizeof nb, "%s", nm); for (char *c = nb; *c; c++) if (*c == ' ') *c = '_'; }
        else snprintf(nb, sizeof nb, "id%d", target);
        int n = snprintf(base, sizeof base, "%lld_%s%s", (long long)seed, nb, lb ? "_LB" : "");
        if (yset) snprintf(base + n, sizeof base - n, "_y%d", by);
        int gap[4] = {-1, -1, -1, -1};
        if (bridge) { gap[0] = river; gap[1] = frozen_river; gap[2] = b0; gap[3] = b1; }
        write_images(base, target, gap, f, x0, z0, x1, z1);
    }
    return 0;
}
