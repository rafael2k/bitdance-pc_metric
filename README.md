# BitDance PCQA Metric

BitDance: Low Complexity Point Cloud Quality Assessment Metric

BitDance is a fast, OpenMP optimized implementation of the technique described in
[Color and Geometry Texture Descriptors for Point-Cloud Quality Assessment](2021_SLP_Rafael.pdf).
If you find this code useful in your research, please consider citing:

   ```
   @ARTICLE{9450013,
      author={Diniz, Rafael and Freitas, Pedro Garcia and Farias, Mylène C. Q.},
      journal={IEEE Signal Processing Letters}, 
      title={Color and Geometry Texture Descriptors for Point-Cloud Quality Assessment}, 
      year={2021},
      volume={28},
      number={},
      pages={1150-1154},
      doi={10.1109/LSP.2021.3088059}}
   ```

This repository contains the code for all discussed algorithms in the article. The C++ code produces
three binaries: bitdance_pcqa, create_normals and optimize_voxel_size. The bitdance_pcqa calculates
the color and geometry PC feature statistics; create_normals creates PCs with oriented normals; and
optimize_voxel_size uses a parameterized heuristic and constant to provide a voxel size for a given
PC. In directory "distance_calculation", it is present the python script we developed for statistics
distance calculation, which includes the Jensen-Shannon divergence code, among others.


## Compilation

This code depends on the Open3D library: https://github.com/intel-isl/Open3D

The code was compiled and tested on Debian Linux 11 (codename Bullseye) and
Ubuntu 22.04 LTS. To install the Open3D library and other dependencies, run
(as root):

    apt-get install libopen3d-dev libeigen3-dev libpng-dev  zlib1g-dev

In order to compile the code, written in C++, open the Makefile, adjust it to your preferred C++
compiler, set to your Open3D installation prefix path (no need for any
modification on Debian 11 or Ubuntu 22.04),
and run:

    make


## Usage

Prior to the execution of the metric, normals without ambiguities need to be created. For this use:

    create_normals point_cloud.ply pc_with_normals.ply

In order to obtain the target voxel edge size of a PC, given a "k" constant ("k" equals to 6.0 in
the article), using the average distance of 8 nearest neighbors (hardcoded in the code) as
reference, as described in the article, use:

    optimize_voxel_size 3 k point_cloud.ply

To run the CIEDE2000 color-based feature extractor with 8 bits label, neighborhood size of 12,
voxel size (use the output of optimize_voxel_size) "E", with the statistics (histograms) results written to "results-c.csv",  use:

    bitdance_pcqa -i point_cloud.ply -n 12 -m 0,1,0,0,0 -v E -h results-c.csv

To run the geometry-based feature extractor with label of 16-bit, neighborhood size of 6, without the
use of voxelization (voxelization does not help for the geometry-based feature extractor), with
the statistics (histograms) results written to "results-g.csv", use:

    bitdance_pcqa -i pc_with_normals.ply -n 6 -m 0,0,1,0,0 -h results-g.csv


## Authors

- Rafael Diniz (rafael@riseup.net)

- Pedro Garcia Freitas (pedro@sawp.com.br): python code for some distances calculation

## Acknowledgments

- Mylene C.Q. Farias, my PhD advisor

- Pedro Garcia Freitas, my PhD co-advisor

- Open3D by Qian-Yi Zhou and Jaesik Park and Vladlen Koltun: http://open3d.org/

- Color spaces conversion code by Nicolae Berendea: https://github.com/berendeanicolae/ColorSpace

- Financial support by the following Brazilian agencies: CAPES and FAP-DF

## License

This code is licensed under GNU General Public License version 3 or any higher version, while the
color spaces conversion and the Open3D code are MIT licensed.
