/*
 * Copyright (C) 2019 Rafael Diniz <rafael@riseup.net>
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

#define BIG_NUMBER 1000000000 // 1bi points max

#define KNN 9 // get 8-NN plus the own point

int main(int argc, char *argv[])
{
    int pc_number = 0;
    uint64_t min_pc_points = BIG_NUMBER;
    int min_pc_index = 0;
    double voxel_size = 0;
    double knob1 = 0.6;
    int voxel_strategy = 1;
    int knn = KNN;

    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s {voxelization_strategy:1,2,3} \"k\" point_cloud1.ply point_cloud2.ply point_cloud3.ply ...\n", argv[0]);
        fprintf(stderr, "Voxelization strategy: \n");
        fprintf(stderr, "1 - Percentage of output points in relation to original\n");
        fprintf(stderr, "2 - Multiplier \"k\" for the average NN\n");
        fprintf(stderr, "3 - Multiplier \"k\" for the average k-NN\n");
        return EXIT_SUCCESS;
    }
    pc_number = argc-3;

    voxel_strategy = atoi(argv[1]);
    knob1 = atof(argv[2]);

    fprintf(stderr, "knob1 = %f\n", knob1);

    std::shared_ptr<geometry::PointCloud> pc[pc_number];

    // get PC with less points
    for (int i = 0; i < pc_number; i++)
    {
        pc[i] = make_shared<geometry::PointCloud>();
        if (io::ReadPointCloud(argv[i+3], *pc[i]))
        {
            // fprintf(stderr, "Successfully read %s\n", argv[i+3]);
        }
        else {
            fprintf(stderr, "Failed to read %s.\n", argv[i+1]);
            return EXIT_FAILURE;
        }

        if (pc[i]->points_.size() < min_pc_points)
        {
            min_pc_points = pc[i]->points_.size();
            min_pc_index = i;
        }

        // print_pointcloud(*pc[i], false);
    }

    double average_dist = 0;
    geometry::KDTreeFlann kdtree;

    kdtree.SetGeometry(*pc[min_pc_index]);

    if (voxel_strategy == 2)
    {
#pragma omp parallel for num_threads(32) reduction(+:average_dist) schedule(dynamic,1000)
        for (size_t i = 0; i < pc[min_pc_index]->points_.size(); i++)
        {
            std::vector<int> indices_vec(2);
            std::vector<double> dists_vec(2);

            kdtree.SearchKNN(pc[min_pc_index]->points_[i], 2, indices_vec, dists_vec);

            Eigen::Vector3d point_close = pc[min_pc_index]->points_[i];
            Eigen::Vector3d point_distant = pc[min_pc_index]->points_[indices_vec[1]];

            double dist = sqrt ( (pow((point_distant[0] - point_close[0]), 2)) +
                                 (pow((point_distant[1] - point_close[1]), 2)) +
                                 (pow((point_distant[2] - point_close[2]), 2)) );

            // #pragma omp atomic
            average_dist += dist;
        }
        average_dist /= pc[min_pc_index]->points_.size();
    }


    if (voxel_strategy == 3)
    {
#pragma omp parallel for num_threads(32) reduction(+:average_dist) schedule(dynamic,1000)
        for (size_t i = 0; i < pc[min_pc_index]->points_.size(); i++)
        {
            std::vector<int> indices_vec(knn);
            std::vector<double> dists_vec(knn);

            kdtree.SearchKNN(pc[min_pc_index]->points_[i], knn, indices_vec, dists_vec);

            Eigen::Vector3d points[knn];
            for (int j = 0; j < knn; j++)
            {
                points[j] = pc[min_pc_index]->points_[indices_vec[j]];
            }

            double dist = 0;
            for (int j = 1; j < knn; j++)
            {
                dist += sqrt ( (pow((points[j][0] - points[0][0]), 2)) +
                                     (pow((points[j][1] - points[0][1]), 2)) +
                                     (pow((points[j][2] - points[0][2]), 2)) );
            }
            dist /= (knn - 1);
            // #pragma omp atomic
            average_dist += dist;
        }
        average_dist /= pc[min_pc_index]->points_.size();
    }


    if (voxel_strategy == 1)
    {
        double step = average_dist / 4;

        double local_voxel_size = average_dist;
        auto pc_copy = pc[min_pc_index];

        while (pc_copy->points_.size() > (pc[min_pc_index]->points_.size() * knob1))
        {
            local_voxel_size += step;
            fprintf(stderr, "local_voxel_size = %0.16f\n", local_voxel_size);
            pc_copy = pc[min_pc_index]->VoxelDownSample(local_voxel_size);
            // fprintf(stderr, "pc %d: %ld\n", min_pc_index, pc[min_pc_index]->points_.size());
        }
        voxel_size = local_voxel_size;

    }

    if (voxel_strategy == 2 || voxel_strategy == 3)
    {
        voxel_size = average_dist * knob1;
    }

    printf("%0.16f\n", voxel_size);
}
