/*
 * Copyright (C) 2019-2021 Rafael Diniz <rafael@riseup.net>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

#include <unistd.h>
#include <cstdint>
#include <sstream>
#include <iostream>
#include <memory>
#include <thread>

#include <Open3D.h>

using namespace open3d;
using namespace std;

int main(int argc, char *argv[])
{
    auto pc = make_shared<geometry::PointCloud>();

    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s input.ply output.ply\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (io::ReadPointCloud(argv[1], *pc)) {
        fprintf(stderr, "Successfully read %s\n", argv[1]);
    } else {
        fprintf(stderr, "Failed to file %s.\n",argv[1]);
        return EXIT_FAILURE;
    }

    geometry::KDTreeFlann kdtree;
    kdtree.SetGeometry(*pc);

    double average_dist = 0;

    int knn = 9; // 8 nearest neighbor

#pragma omp parallel for num_threads(32) reduction(+:average_dist) schedule(dynamic,1000)
    for (size_t i = 0; i < pc->points_.size(); i++)
    {
        std::vector<int> indices_vec(knn);
        std::vector<double> dists_vec(knn);

        kdtree.SearchKNN(pc->points_[i], knn, indices_vec, dists_vec);

        Eigen::Vector3d points[knn];
        for (int j = 0; j < knn; j++)
        {
            points[j] = pc->points_[indices_vec[j]];
        }

        double dist = 0;
        for (int j = 1; j < knn; j++)
        {
            dist += sqrt ( (pow((points[j][0] - points[0][0]), 2)) +
                           (pow((points[j][1] - points[0][1]), 2)) +
                           (pow((points[j][2] - points[0][2]), 2)) );
        }
        dist /= (knn - 1);

        average_dist += dist;
    }
    average_dist /= pc->points_.size();


    pc->EstimateNormals(geometry::KDTreeSearchParamHybrid(average_dist * 6, 16)); // radius, max_nn

    pc->NormalizeNormals();

    pc->OrientNormalsToAlignWithDirection(Eigen::Vector3d(0.0, 0.0, 1.0)); // here we just pick an arbitrary direction


//    print_pointcloud(*pc, false);

#if  !((OPEN3D_VERSION_MAJOR == 0 ) &&  (OPEN3D_VERSION_MINOR < 10))
        io::WritePointCloudOption pc_params = io::WritePointCloudOption (true, false, false,  NULL);
#endif

    if ( strstr(argv[2], ".ply" ))
#if ((OPEN3D_VERSION_MAJOR == 0 ) &&  (OPEN3D_VERSION_MINOR < 10))
        io::WritePointCloudToPLY(argv[2], *pc, true, false); //
#else
        io::WritePointCloudToPLY(argv[2], *pc, pc_params); //
#endif
    if( strstr(argv[2], ".xyz"))
#if ((OPEN3D_VERSION_MAJOR == 0 ) &&  (OPEN3D_VERSION_MINOR < 10))
        io::WritePointCloudToXYZ(argv[2], *pc, true, false);
#else
        io::WritePointCloudToXYZ(argv[2], *pc, pc_params);
#endif
    if( strstr(argv[2], ".xyzrgb"))
#if ((OPEN3D_VERSION_MAJOR == 0 ) &&  (OPEN3D_VERSION_MINOR < 10))
        io::WritePointCloudToXYZRGB(argv[2], *pc, true, false);
#else
        io::WritePointCloudToXYZRGB(argv[2], *pc, pc_params);
#endif
    if( strstr(argv[2], ".pcd"))
#if ((OPEN3D_VERSION_MAJOR == 0 ) &&  (OPEN3D_VERSION_MINOR < 10))
        io::WritePointCloudToPCD(argv[2], *pc, true, false);
#else
        io::WritePointCloudToPCD(argv[2], *pc, pc_params);
#endif

    return EXIT_SUCCESS;

}
