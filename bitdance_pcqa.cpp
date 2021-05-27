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

#include <inttypes.h>
#include <stdlib.h>

#include "bitdance_pcqa.h"

#include "ColorSpace/ColorSpace.h"
#include "ColorSpace/Comparison.h"

#define USE_OPENMP__ 1

using namespace open3d;
using namespace std;

int main(int argc, char *argv[])
{
    bool create_histogram = false;
    bool divide_color_by_255 = false;
    bool voxelize = false;
    bool split_files = false;
    double voxel_size = 0;

    int neiborhood_list_size = 0;
    int max_neiborhood_size = 0;
    int neiborhood_size[MAX_NN_LIST_SIZE];

    uint8_t metric_enabled[MAX_NR_METRICS];

    char metrics_enabled_list[MAX_FILENAME] = {0};
    char neiborhood_sizes_list[MAX_FILENAME] = {0};
    char input_filename[MAX_FILENAME] = {0};
    char histogram_filename[MAX_FILENAME] = {0};


    if (argc < 3){
    usage_info:
        fprintf(stderr, "Usage: %s [OPTIONS]\n", argv[0]);
        fprintf(stderr, "Usage example: %s -i input_pc.ply -n 12,11,10,9,8 -h fv_file_prot.csv -m 1,1,1,1,1\n", argv[0]);
        fprintf(stderr, "\nOPTIONS:\n");
        fprintf(stderr, "    -i input_pc.ply         PC to be evaluated\n");
        fprintf(stderr, "    -h fv.csv               Save the feature vector as csv output (or use this paramenter as basename, in case \"-s\" is used).\n");
        fprintf(stderr, "    -n neiborhood_sizes_list Comma separated neiborhood size to test(eg: \"12,10,8\"\n");
        fprintf(stderr, "    -m metrics_enabled_list  Comma separated boolean values of the enabled metrics, in the following order: DE2000 12-bit, DE2000 8-bit, Geo 16-bit, Geo 12-bit, Geo 8-bit)\n");
        fprintf(stderr, "    -v voxel_size           Voxelize and use voxel size as specified\n");
        fprintf(stderr, "    -y                      Divide color attributes by 255\n");
        fprintf(stderr, "    -s                      Split results files (many output files!)\n");
        return EXIT_SUCCESS;
    }

    int opt;
    while ((opt = getopt(argc, argv, "i:h:n:m:v:ys")) != -1){
        switch (opt){
        case 'i':
            strncpy (input_filename, optarg, MAX_FILENAME);
            break;
        case 's':
            split_files = true;
            break;
        case 'h':
            create_histogram = true;
            strncpy (histogram_filename, optarg, MAX_FILENAME);
            break;
        case 'n':
            strncpy (neiborhood_sizes_list, optarg, MAX_FILENAME);
            break;
        case 'm':
            strncpy (metrics_enabled_list, optarg, MAX_FILENAME);
            break;
        case 'v':
            voxelize = true;
            voxel_size = atof(optarg);
            break;
        case 'y':
            divide_color_by_255 = true;
            break;
        default:
            fprintf(stderr, "Wrong command line.\n");
            goto usage_info;
        }
    }


// COMMAND LINE PARSING //

    if (neiborhood_sizes_list[0] == 0)
    {
        fprintf(stderr, "Specify at least one neighbour size.\n");
        goto usage_info;
    }

    // parse input neighborhood list
    char *tok = neiborhood_sizes_list;
    strtok(neiborhood_sizes_list, ",");
    do
    {
        sscanf(tok, "%d", &neiborhood_size[neiborhood_list_size]);
        if (neiborhood_size[neiborhood_list_size] > max_neiborhood_size)
            max_neiborhood_size = neiborhood_size[neiborhood_list_size];
        neiborhood_list_size++;
    } while ((tok = strtok(NULL, ",")) && neiborhood_list_size < MAX_NN_LIST_SIZE);

#if 1
    fprintf(stderr, "total neighbours: %d max %d\n", neiborhood_list_size, max_neiborhood_size);
    for (int i = 0; i < neiborhood_list_size; i++)
        fprintf(stderr, "neighbours: %d\n", neiborhood_size[i]);
#endif

    // parse metrics selection list
    int metrics_counter = 0;
    tok = metrics_enabled_list;
    strtok(metrics_enabled_list, ",");
    do
    {
        sscanf(tok, "%hhu", &metric_enabled[metrics_counter]);
        // fprintf(stderr, "%d\n", metric_enabled[metrics_counter]);
        metrics_counter++;
    } while ((tok = strtok(NULL, ",")) && metrics_counter < MAX_NR_METRICS);

#if 1
    fprintf(stderr, "total available metrics: %d\n", metrics_counter);
    for (int i = 0; i < metrics_counter; i++)
        fprintf(stderr, "metrics[%d]: %s\n", i, metric_enabled[i]? "enabled":"disabled");
#endif


    // 2D vector with the histograms
    double *histogram[MAX_NR_METRICS][neiborhood_list_size];


#if USE_OPENMP__ == 1
    int max_threads = omp_get_max_threads();
    fprintf(stderr, "OpenMP reported max threads = %d\n", max_threads);
#else
    fprintf(stderr, "OpenMP build disabled.\n");
#endif

    // METRICS INITIALIZATION //

    int end_of_scale[MAX_NR_METRICS][neiborhood_list_size];

    for (int j = 0; j < neiborhood_list_size; j++)
    {
        end_of_scale[DLCP_12B][j] = 4096;
        end_of_scale[DLCP_8B][j] = 256;

        end_of_scale[DGEO_16B][j] = 65536;
        end_of_scale[DGEO_12B][j] = 4096;
        end_of_scale[DGEO_8B][j] = 256;
    }

    for (int i = 0; i < MAX_NR_METRICS; i++)
    {
        if (metric_enabled[i] != 0)
        {
            for (int j = 0; j < neiborhood_list_size; j++)
            {
                histogram[i][j] = (double *) calloc(end_of_scale[i][j], sizeof(double));
                memset (histogram[i][j], 0, sizeof(double) * end_of_scale[i][j]);
            }
        }
    }

    // OPEN THE PC FILE //
    auto pc = make_shared<geometry::PointCloud>();

    if (io::ReadPointCloud(input_filename, *pc))
    {
        fprintf(stderr, "Successfully read PC %s\n", input_filename);
    }
    else {
        fprintf(stderr, "Failed to read PC %s.\n", input_filename);
        return EXIT_FAILURE;
    }

    // workaround ".xyzrgb" 3dtk 2^8 unsigned integer rgb range not read correctly by Open3D
    if (strstr (input_filename, ".xyzrgb") || divide_color_by_255)
    {
        for (size_t i = 0; i < pc->points_.size(); i++) {
            pc->colors_[i](0) = pc->colors_[i](0) / 255.0;
            pc->colors_[i](1) = pc->colors_[i](1) / 255.0;
            pc->colors_[i](2) = pc->colors_[i](2) / 255.0;
        }
    }

    if (voxelize)
    {
        pc = pc->VoxelDownSample(voxel_size);
    }

    // print_pointcloud(*pc, false);

    // METRIC PROCESSING //

    geometry::KDTreeFlann kdtree;
    kdtree.SetGeometry(*pc);

    // for each point in the PC - parallel execution using OpenMP
#if USE_OPENMP__ == 1
#pragma omp parallel for num_threads(max_threads) schedule(dynamic,1000)
#endif
    for (size_t i = 0; i < pc->points_.size(); i++) {
        int label[MAX_NR_METRICS];
        memset(label, 0, sizeof(int) * MAX_NR_METRICS);

        // for fast retrieval of nearest neighbor we use kd-tree
        std::vector<int> indices_vec(max_neiborhood_size + 1);
        std::vector<double> dists_vec(max_neiborhood_size + 1);

        // get the nearest neighbors of point with index "i"
        kdtree.SearchKNN(pc->points_[i], max_neiborhood_size + 1, indices_vec, dists_vec);

        for (int foo = 0; foo < neiborhood_list_size; foo++)
        {
            int nn = neiborhood_size[foo] + 1;

            const Eigen::Vector3d &point_color = pc->colors_[i];
            const Eigen::Vector3d &point_normal = pc->normals_[i];


            for (int j = 1 ; j < nn; j++)
            { // starting from 1, as index 0 refers to the own point.
                const Eigen::Vector3d &color = pc->colors_[indices_vec[j]];
                const Eigen::Vector3d &normal = pc->normals_[indices_vec[j]];
                // const Eigen::Vector3d &point = pc->points_[indices_vec[j]];


                double diff = 0;

                if (metric_enabled[DLCP_8B] != 0 || metric_enabled[DLCP_12B] != 0)
                {

                    ColorSpace::Rgb a(point_color(0) * 255, point_color(1) * 255, point_color(2) * 255);
                    ColorSpace::Rgb b(color(0) * 255, color(1) * 255, color(2) * 255);
                    // CIE LAB Delta E 2000 (CIEDE2000)
                    diff = ColorSpace::Cie2000Comparison::Compare(&a, &b);

#if 0 // for debugging purposes...

                    ColorSpace::Lab lab_a, lab_b;
                    a.To<ColorSpace::Lab>(&lab_a);
                    b.To<ColorSpace::Lab>(&lab_b);

                    fprintf(stderr, "CIE2000 diff = %.5f, l = %f a = %f b = %f  l = %f a = %f b = %f\n", diff, lab_a.l, lab_a.a, lab_a.b, lab_b.l, lab_b.a, lab_b.b);
#endif

                    if (metric_enabled[DLCP_8B] != 0)
                    {
                        // if (diff >= 0 && diff < 2.5) // ~ Just Noticeable Difference
                        //  label[DLCP_8B] |= 0;

                        if (diff >= 2.5 && diff < 5.0)
                            label[DLCP_8B] |= 1 << 0;

                        if (diff >= 5.0 && diff < 7.5)
                            label[DLCP_8B] |= 1 << 1;

                        if (diff >= 7.5 && diff < 10.0)
                            label[DLCP_8B] |= 1 << 2;

                        if (diff >= 10.0 && diff < 12.5)
                            label[DLCP_8B] |= 1 << 3;

                        if (diff >= 12.5  && diff < 15.0)
                            label[DLCP_8B] |= 1 << 4;

                        if (diff >= 15.0 && diff < 17.5)
                            label[DLCP_8B] |= 1 << 5;

                        if (diff >= 17.5 && diff < 20.0)
                            label[DLCP_8B] |= 1 << 6;

                        if (diff >= 20.0)
                            label[DLCP_8B] |= 1 << 7;
                    }

                    if (metric_enabled[DLCP_12B] != 0)
                    {
                        // if (diff >= 0 && diff < 1.5) // ~ Just Noticeable Difference
                        //  label[DLCP_12B] |= 0;

                        if (diff >= 1.5 && diff < 3.0)
                            label[DLCP_12B] |= 1 << 0;

                        if (diff >= 3.0 && diff < 4.5)
                            label[DLCP_12B] |= 1 << 1;

                        if (diff >= 4.5 && diff < 6.0)
                            label[DLCP_12B] |= 1 << 2;

                        if (diff >= 6.0 && diff < 7.5)
                            label[DLCP_12B] |= 1 << 3;

                        if (diff >= 7.5  && diff < 9.0)
                            label[DLCP_12B] |= 1 << 4;

                        if (diff >= 9.0 && diff < 10.5)
                            label[DLCP_12B] |= 1 << 5;

                        if (diff >= 10.5 && diff < 12.0)
                            label[DLCP_12B] |= 1 << 6;

                        if (diff >= 12.0 && diff < 13.5)
                            label[DLCP_12B] |= 1 << 7;

                        if (diff >= 13.5 && diff < 15.0)
                            label[DLCP_12B] |= 1 << 8;

                        if (diff >= 15.0 && diff < 16.5)
                            label[DLCP_12B] |= 1 << 9;

                        if (diff >= 16.5 && diff < 18.0)
                            label[DLCP_12B] |= 1 << 10;

                        if (diff >= 18.0)
                            label[DLCP_12B] |= 1 << 11;
                    }
                }


                double dist = 0;

                if (metric_enabled[DGEO_16B] || metric_enabled[DGEO_12B] || metric_enabled[DGEO_8B])
                {
                    dist =  sqrt ( (pow((point_normal[0] - normal[0]), 2)) +
                                   (pow((point_normal[1] - normal[1]), 2)) +
                                   (pow((point_normal[2] - normal[2]), 2)) );
                    // fprintf(stderr, "dist: %.8f central pt normal: %.4f %.4f %.4f\nneighbo pt normal: %.4f %.4f %.4f\n\n", dist, point_normal(0), point_normal(1), point_normal(2), normal(0), normal(1), normal(2));
                }


                if (metric_enabled[DGEO_16B] != 0)
                {
                    // if (dist >= 0 && dist < 0.05)
                    // label[DGEO_16B] |= 0;

                    if (dist >= 0.05 && dist < 0.1)
                        label[DGEO_16B] |= 1 << 0;

                    if (dist >= 0.1 && dist < 0.175)
                        label[DGEO_16B] |= 1 << 1;

                    if (dist >= 0.175 && dist < 0.275)
                        label[DGEO_16B] |= 1 << 2;

                    if (dist >= 0.275 && dist < 0.4)
                        label[DGEO_16B] |= 1 << 3;

                    if (dist >= 0.4 && dist < 0.525)
                        label[DGEO_16B] |= 1 << 4;

                    if (dist >= 0.525 && dist < 0.65)
                        label[DGEO_16B] |= 1 << 5;

                    if (dist >= 0.65 && dist < 0.775)
                        label[DGEO_16B] |= 1 << 6;

                    if (dist >= 0.775 && dist < 0.9)
                        label[DGEO_16B] |= 1 << 7;

                    if (dist >= 0.9 && dist < 1.025)
                        label[DGEO_16B] |= 1 << 8;

                    if (dist >= 1.025 && dist < 1.15)
                        label[DGEO_16B] |= 1 << 9;

                    if (dist >= 1.15 && dist < 1.275)
                        label[DGEO_16B] |= 1 << 10;

                    if (dist >= 1.275 && dist < 1.4)
                        label[DGEO_16B] |= 1 << 11;

                    if (dist >= 1.4 && dist < 1.525)
                        label[DGEO_16B] |= 1 << 12;

                    if (dist >= 1.525 && dist < 1.65)
                        label[DGEO_16B] |= 1 << 13;

                    if (dist >= 1.65 && dist < 1.8)
                        label[DGEO_16B] |= 1 << 14;

                    if (dist >= 1.8 && dist < 2.0)
                        label[DGEO_16B] |= 1 << 15;

                }

                if (metric_enabled[DGEO_12B] != 0)
                {
                   // if (dist >= 0 && dist < 0.05)
                    // label[DGEO_16B] |= 0;

                    if (dist >= 0.05 && dist < 0.1)
                        label[DGEO_12B] |= 1 << 0;

                    if (dist >= 0.1 && dist < 0.3)
                        label[DGEO_12B] |= 1 << 1;

                    if (dist >= 0.3 && dist < 0.45)
                        label[DGEO_12B] |= 1 << 2;

                    if (dist >= 0.45 && dist < 0.6)
                        label[DGEO_12B] |= 1 << 3;

                    if (dist >= 0.6 && dist < 0.75)
                        label[DGEO_12B] |= 1 << 4;

                    if (dist >= 0.75 && dist < 0.9)
                        label[DGEO_12B] |= 1 << 5;

                    if (dist >= 0.9 && dist < 1.05)
                        label[DGEO_12B] |= 1 << 6;

                    if (dist >= 1.05 && dist < 1.2)
                        label[DGEO_12B] |= 1 << 7;

                    if (dist >= 1.2 && dist < 1.35)
                        label[DGEO_12B] |= 1 << 8;

                    if (dist >= 1.35 && dist < 1.55)
                        label[DGEO_12B] |= 1 << 9;

                    if (dist >= 1.55 && dist < 1.75)
                        label[DGEO_12B] |= 1 << 10;

                    if (dist >= 1.75 && dist < 2.0)
                        label[DGEO_12B] |= 1 << 11;

                }

                if (metric_enabled[DGEO_8B] != 0)
                {
                    if ( normal(0) < 0 && normal(1) < 0 && normal(2) < 0 )
                        label[DGEO_8B] |= 1 << 0;

                    if ( normal(0) < 0 && normal(1) < 0 && normal(2) >= 0 )
                        label[DGEO_8B] |= 1 << 1;

                    if ( normal(0) < 0 && normal(1) >= 0 && normal(2) < 0 )
                        label[DGEO_8B] |= 1 << 2;

                    if ( normal(0) < 0 && normal(1) >= 0 && normal(2) >= 0 )
                        label[DGEO_8B] |= 1 << 3;

                    if ( normal(0) >= 0 && normal(1) < 0 && normal(2) < 0 )
                        label[DGEO_8B] |= 1 << 4;

                    if ( normal(0) >= 0 && normal(1) < 0 && normal(2) >= 0 )
                        label[DGEO_8B] |= 1 << 5;

                    if ( normal(0) >= 0 && normal(1) >= 0 && normal(2) < 0 )
                        label[DGEO_8B] |= 1 << 6;

                    if ( normal(0) >= 0 && normal(1) >= 0 && normal(2) >= 0 )
                        label[DGEO_8B] |= 1 << 7;
                }


            }


            for (int i = 0; i < MAX_NR_METRICS; i++)
            {
                if (metric_enabled[i] != 0)
                {
                    histogram[i][foo][label[i]]++;
                }
            }

        }
    }

    // normalize
    for (int i = 0; i < MAX_NR_METRICS; i++)
    {
        if (metric_enabled[i] != 0)
        {
            for (int foo = 0; foo < neiborhood_list_size; foo++)
            {
                for (int j = 0; j < end_of_scale[i][foo]; j++)
                {
                    histogram[i][foo][j] = histogram[i][foo][j] / pc->points_.size();
                }
            }
        }
    }


    // Results output
    FILE *hist_fp;

    //  write
    if (create_histogram == true)
    {
        if (split_files == true)
        {
            for (int i = 0; i < MAX_NR_METRICS; i++)
            {
                if (metric_enabled[i] != 0)
                {
                    for (int foo = 0; foo < neiborhood_list_size; foo++)
                    {
                        char filename[4096];
                        // this is our output file format for split files!
                        sprintf(filename, "%s_M%02d_N%02d.csv", histogram_filename, i, neiborhood_size[foo]);

                        hist_fp = fopen(filename, "a");
                        if (hist_fp == NULL)
                        {
                            fprintf(stderr, "Could not write histogram output path: %s.\n", filename);
                            exit (EXIT_FAILURE);
                        }

                        fprintf(hist_fp, "%s,", input_filename);
                        for (int j = 0; j < end_of_scale[i][foo]; j++)
                        {

                            if ( fpclassify(histogram[i][foo][j]) == FP_ZERO ) // if (histogram[i][foo][j] == 0.0) ...
                                fprintf(hist_fp, "0.0%s", (j != (end_of_scale[i][foo] - 1))? ",":"");
                            else
                                fprintf(hist_fp, "%0.16f%s", histogram[i][foo][j],(j != (end_of_scale[i][foo] - 1))? ",":"");
                        }
                        fprintf(hist_fp, "\n");

                        fclose(hist_fp);
                    }
                }
            }
        }

        if (split_files == false)
        {
            hist_fp = fopen(histogram_filename, "a");

            // Write the output //
            if (hist_fp == NULL)
            {
                fprintf(stderr, "Could not write histogram output path: %s.\n", histogram_filename);
                exit (EXIT_FAILURE);
            }
            // fwrite(normalize ? (void *)histogram_normalized : (void *)histogram, 8, end_of_scale, hist_fp); // ps: sizeof(double) == sizeof(uint64_t) == 8 // binary write

            for (int i = 0; i < MAX_NR_METRICS; i++)
            {
                if (metric_enabled[i] != 0)
                {
                    for (int foo = 0; foo < neiborhood_list_size; foo++)
                    {
                        fprintf(hist_fp, "%s,metric_%d,n_%d,", input_filename, i, neiborhood_size[foo]);
                        for (int j = 0; j < end_of_scale[i][foo]; j++)
                        {

                            if ( fpclassify(histogram[i][foo][j]) == FP_ZERO ) // if (histogram[i][foo][j] == 0.0) ...
                                fprintf(hist_fp, "0.0%s", (j != (end_of_scale[i][foo] - 1))? ",":"");
                            else
                                fprintf(hist_fp, "%0.16f%s", histogram[i][foo][j],(j != (end_of_scale[i][foo] - 1))? ",":"");
                        }
                        fprintf(hist_fp, "\n");
                    }
                }
            }


            fclose(hist_fp);
        }
    }


    return EXIT_SUCCESS;
}
