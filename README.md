# Measure

Flood fills the biome located at a given coordinate and measures the size in both 1:4 cells and blocks^2 (cells * 16). 
Specify any coordinates inside the biome you're measuring and whatever biome exists at that point will be measured and output.
Bridges rivers based on what biome would exist if the river didn't exist. Because of this, rivers are only ever bridged across never along.
Also, because of this no border tolerance is needed leading to much more accurate measurements.

## Build

Make sure to set `THREADS` in the defines before compiling.

MSYS2/MinGW on Windows, or Linux:

    make

## Usage

    ./measure.exe <seed> <x> <z> [LB] [y] [img]

    Seed		 World Seed
    X Z		 Coords of a point inside the biome you want measured
    LB		 Add this if measuring something in Large Biomes mode
    Y		 Y level to sample at, defaults to 256. Specify for cave biomes, e.g. 0 or -64
    img/img4/img16	 Flag to save images of the connected biome, see below (img4 or img16 to pick the scale)

## Images

With `img`, two PNGs of the same area are saved in the folder you run it from:

- `<seed>_<Biome>_map.png`: The normal biome map, with the same window size as the biome only image.
- `<seed>_<Biome>_only.png`: Only the measured biome on a black background. Bridged cells stay their own color so you can see where the biome connects.

Scale for both is 1:4 (a pixel per 4 blocks) by default with a 256 block border on all sides. If that would be over 8192 pixels across (`IMG_MAX`) the scale changes to 1:16, or 1:64 if the image goes above 8192 pixels in 1:16 scale.

Use the `img4` flag to generate the entire image in 1:4 scale no matter the size, and `img16` to only generate with 1:16 scale.

LB and Y levels go in the file names (`_LB`, `_y-64`).

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
     Images: 2147416166582_Badlands_only.png, 2147416166582_Badlands_map.png (1:4, 3490x1406)
