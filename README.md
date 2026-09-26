# Measure

Flood fills the biome located at a given coordinate and measures the size in both 1:4 cells and blocks^2 (cells * 16). 
Specify any coordinates inside the biome you're measuring and whatever biome exists at that point will be measured and output.
Bridges rivers based on what biome would exist if the river didn't exist. Because of this, rivers are only ever bridged across never along.
Also, because of this no border tolerance is needed leading to much more accurate measurements.

## Build

Make sure to set `NUM_THREADS` in the defines before compiling.

MSYS2/MinGW on Windows, or Linux:

    make

## Usage

    ./measure.exe <seed> <x> <z> [LB] [y]

    Seed		World Seed
    X Z		Coords of a point inside the biome you want measured
    LB		Add this if measuring something in Large Biomes mode
    Y		Y level to sample at, defaults to 256. Specify for cave biomes, e.g. 0 or -64

## Examples:

     ./measure.exe 1106030000022708926 11470050 23002448
     Biome: Ocean | 1:4 Size: 1225857 | Blocks^2 Size: 19613712

     ./measure.exe -3677363685801260643 -8820998 -13743929 -64
     Biome: Deep Dark | Y=-64 | 1:4 Size: 3345142 | Blocks^2 Size: 53522272

     ./measure.exe 1106500446090561476 -187465 -105160 LB
     Biome: Desert LB | 1:4 Size: 63683525 | Blocks^2 Size: 1018936400

     ./measure.exe -1757248279604625080 0 0 LB 0
     Biome: Dripstone Caves LB | Y=0 | 1:4 Size: 22039226 | Blocks^2 Size: 352627616