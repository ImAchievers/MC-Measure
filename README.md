# Measure

Flood fills the biome located at a given coordinate and measures the size in both 1:4 cells and blocks^2 (cells * 16). 
Specify any coordinates inside the biome you're measuring and whatever biome exists at that point will be measured and output.
Bridges rivers based on what biome would exist if the river didn't exist. Because of this, rivers are only ever bridged across never along.
Also, because of this no border tolerance is needed leading to much more accurate measurements.

## Build

Settings to adjust in the #defines before compiling:

    THREADS     Worker threads, adjust for your PC
    BG_COLOR    Background color of the images, 0x000000 for black (default), 0xFFFFFF for white, or any other hex code you'd like
    SIDE_TEXT   VERTICAL (default), turns the height label to read upward inside its bracket, HORIZONTAL keeps it flat

MSYS2/MinGW on Windows, or Linux:

    make

## Usage

    ./measure.exe <seed> [x z] [LB] [y] [img] [ring <radius> | box <side>]
    ./measure.exe list <file>

    Seed             World Seed
    X Z              Coords of a point inside the biome you want measured, defaults to 0 0 if only a seed is given
    LB               Add this if measuring something in Large Biomes mode
    Y                Y level to sample at, defaults to 256. Specify for cave biomes, e.g. 0 or -64 (a single number after the seed is the Y level at 0 0)
    img/img4/img16   Flag to save images of the connected biome, see below (img4 or img16 to pick the scale)
    ring/box         Saves the biome map centered on the coords with a red ring (radius) or box (side length) in blocks
    list             Measures every line of a text file, see Batch below

Every measurement is also written to `measure_results.txt` in the folder you run it from:

    Seed: 2147416166582 | Coords: 0 0 | Biome: Badlands | 1:4 Size: 853707 | Blocks^2 Size: 13659312

## Images

With `img`, two PNGs of the same area are saved in an `images` folder where you run it from:

- `<seed>_<Biome>_map.png`: The normal biome map of the same area, framed by the background color.
- `<seed>_<Biome>_only.png`: Only the measured biome on the background color (black by default). Bridged cells stay their own color so you can see where the biome connects.

Both have the seed and coords in the top left, the size in the top right, and the biome's width and height in blocks along the left and bottom. The labels use the [Monocraft](https://github.com/IdreesInc/Monocraft) font (lib/font.h, OFL license in lib/OFL.txt).

Scale for both is 1:4 (a pixel per 4 blocks) by default with a 256 block border on all sides. If that would be over 8192 pixels across (`IMG_MAX`) the scale changes to 1:16, or 1:64 if the image goes above 8192 pixels in 1:16 scale.

Use the `img4` flag to generate the entire image in 1:4 scale no matter the size, and `img16` to only generate with 1:16 scale.

LB and Y levels go in the file names (`_LB`, `_y-64`).

With `ring` or `box` the biome map is saved centered on the coords you gave, with the same border as the other images, as `<seed>_<Biome>_ring2332.png` or `<seed>_<Biome>_box5000.png`. `img` isn't needed for these. The box side works the same as cubiomes-viewer's centered square, so `box 5000` reaches 2500 blocks in each direction.

## Batch

`./measure.exe list seeds.txt` measures every line of seeds.txt, one measurement per line written the same way as the normal command without the `./measure.exe`.
So a .txt file for batch measuring can use any of these formats for example:

    2147416166582
    1106030000022708926 11470050 23002448 img
    -3677363685801260643 -8820998 -13743929 -64
    1106500446090561476 -187465 -105160 LB ring 5000

Lines that don't start with a seed are skipped.

## Examples:

     ./measure.exe 1106030000022708926 11470050 23002448
     Biome: Ocean | 1:4 Size: 1225857 | Blocks^2 Size: 19613712

     ./measure.exe -3677363685801260643 -8820998 -13743929 -64
     Biome: Deep Dark | Y=-64 | 1:4 Size: 3345142 | Blocks^2 Size: 53522272

     ./measure.exe 1106500446090561476 -187465 -105160 LB
     Biome: Desert LB | 1:4 Size: 63683525 | Blocks^2 Size: 1018936400

     ./measure.exe -1757248279604625080 0 0 LB 0
     Biome: Dripstone Caves LB | Y=0 | 1:4 Size: 22039226 | Blocks^2 Size: 352627616

     ./measure.exe 2147416166582 0 0 img
     Biome: Badlands | 1:4 Size: 853707 | Blocks^2 Size: 13659312
     Images: images/2147416166582_Badlands_only.png, images/2147416166582_Badlands_map.png (1:4, 3670x1770)

     ./measure.exe 3437289040000199328 ring 2332
     Biome: River | 1:4 Size: 562 | Blocks^2 Size: 8992
     Image: images/3437289040000199328_River_ring2332.png (1:4, 1297x1297)

     ./measure.exe 3437289040000199328 3252345 897567 LB box 5000
     Biome: Old Growth Spruce Taiga LB | 1:4 Size: 56789 | Blocks^2 Size: 908624
     Image: images/3437289040000199328_Old_Growth_Spruce_Taiga_LB_box5000.png (1:4, 1381x1381)
