#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>

#include "generator.h"

#define MC_VERSION MC_NEWEST
#define THREADS 18
#define TILE 64
#define TBITS 6
#define TMASK 63
#define TAREA (TILE * TILE)
#define MAX_CELLS 300000000LL

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

static void gen_tile(Generator *g, Tile *t)
{
    int *buf = malloc(TAREA * sizeof(int));
    Range r = {4, t->tx << TBITS, t->tz << TBITS, TILE, TILE, g_sy, 1};
    genBiomes(g, buf, r);
    int16_t *b = malloc(TAREA * sizeof(int16_t));
    for (int i = 0; i < TAREA; i++) b[i] = (int16_t)buf[i];
    free(buf);
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

static int *pend;
static size_t pendn, pendcap;

static void pend_add(int idx)
{
    if (pendn == pendcap) { pendcap = pendcap ? pendcap * 2 : 4096; pend = realloc(pend, pendcap * sizeof(int)); }
    pend[pendn++] = idx;
}

static void flush_pending(void)
{
    for (size_t i = 0, base = pendn; i < base; i++)
    {
        int tx = tiles[pend[i]].tx, tz = tiles[pend[i]].tz;
        for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++)
        {
            int j = tile_lookup(tx + dx, tz + dz);
            if (j < 0) j = tile_new(tx + dx, tz + dz);
            if (!tiles[j].biome && !tiles[j].q) { tiles[j].q = 1; pend_add(j); }
        }
    }
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

static int edge_hit(const int16_t *b, int dir, int target)
{
    if (dir == 0) { for (int x = 0; x < TILE; x++) if (b[x] == target) return 1; }
    else if (dir == 1) { for (int x = 0; x < TILE; x++) if (b[(TILE - 1) * TILE + x] == target) return 1; }
    else if (dir == 2) { for (int z = 0; z < TILE; z++) if (b[z * TILE] == target) return 1; }
    else { for (int z = 0; z < TILE; z++) if (b[z * TILE + TILE - 1] == target) return 1; }
    return 0;
}

static void prefetch(int s0, int target)
{
    int dnx[4] = {0, 0, -1, 1}, dnz[4] = {-1, 1, 0, 0};
    int *fr = malloc(sizeof(int));
    fr[0] = s0;
    size_t frn = 1;
    tiles[s0].q = 1;
    int *gen = NULL, *nf = NULL;
    size_t gc = 0, nfc = 0;
    while (frn)
    {
        size_t gn = 0, nfn = 0;
        for (size_t i = 0; i < frn; i++)
        {
            int tx = tiles[fr[i]].tx, tz = tiles[fr[i]].tz;
            for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++)
            {
                if ((!dx && !dz) || tile_lookup(tx + dx, tz + dz) >= 0) continue;
                int k = tile_new(tx + dx, tz + dz);
                if (gn == gc) { gc = gc ? gc * 2 : 256; gen = realloc(gen, gc * sizeof(int)); }
                gen[gn++] = k;
            }
        }
        gen_batch(gen, gn);
        for (size_t i = 0; i < frn; i++)
        {
            const int16_t *b = tiles[fr[i]].biome;
            int tx = tiles[fr[i]].tx, tz = tiles[fr[i]].tz;
            for (int dir = 0; dir < 4; dir++)
            {
                if (!edge_hit(b, dir, target)) continue;
                int nb = tile_lookup(tx + dnx[dir], tz + dnz[dir]);
                if (nb < 0 || tiles[nb].q) continue;
                tiles[nb].q = 1;
                if (nfn == nfc) { nfc = nfc ? nfc * 2 : 256; nf = realloc(nf, nfc * sizeof(int)); }
                nf[nfn++] = nb;
            }
        }
        free(fr); fr = nf; frn = nfn;
        nf = NULL; nfc = 0;
        if ((long long)ntiles * TAREA > MAX_CELLS) break;
    }
    free(fr); free(gen);
    for (size_t i = 0; i < ntiles; i++) tiles[i].q = 0;
}

/* rivers generate where weirdness is within 0.05 of 0, this decides what would generate if the river didn't exist */
static int under_river(Generator *g, int x, int z)
{
    int64_t np[6];
    sampleBiomeNoise(&g->bn, np, x, g_sy, z, NULL, SAMPLE_NO_BIOME);
    if (np[5] > 500 || np[5] < -500) return -1;
    np[5] = np[5] >= 0 ? 501 : -501;
    return climateToBiome(g->mc, (const uint64_t *)np, NULL);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <seed> <x> <z> [LB] [y]\n", argv[0]);
        return 1;
    }
    uint64_t seed = (uint64_t)strtoll(argv[1], NULL, 10);
    int bx = atoi(argv[2]);
    int bz = atoi(argv[3]);
    int lb = 0, yset = 0, by = 256;
    for (int i = 4; i < argc; i++)
    {
        if (!strcmp(argv[i], "LB") || !strcmp(argv[i], "lb")) lb = 1;
        else { by = atoi(argv[i]); yset = 1; }
    }

    g_seed = seed; g_lb = lb; g_sy = by >> 2;
    int scx = bx >> 2, scz = bz >> 2;

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

    prefetch(s0, target);

    long long count = 0;
    int capped = 0;
    int dxu[4] = {0, 0, -1, 1}, dzu[4] = {-1, 1, 0, 0};
    tiles[s0].vis = calloc(TAREA, 1);
    tiles[s0].vis[(scz & TMASK) * TILE + (scx & TMASK)] = 1;
    spush(scx, scz);
    while (sn)
    {
        sn--;
        int x = stx[sn], z = stz[sn];
        int ti = tile_slot(x >> TBITS, z >> TBITS);
        if (!tiles[ti].biome) flush_pending();
        int bm = tiles[ti].biome[(z & TMASK) * TILE + (x & TMASK)];
        if (bm == target) { if (++count >= MAX_CELLS) { capped = 1; break; } }
        else if (!bridge || (bm != river && bm != frozen_river && bm != b0 && bm != b1) || under_river(&g, x, z) != target) continue;
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
    return 0;
}
