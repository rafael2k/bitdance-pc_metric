#
# Copyright (C) 2021 Pedro Garcia Freitas <pdr.grc.frts@gmail.com>
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this software; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street,
# Boston, MA 02110-1301, USA.
#


import os
import sys
import argparse

import pandas as pd
import numpy as np
from absl import app
from absl.flags import argparse_flags
import scipy.spatial.distance as d
from scipy.stats import wasserstein_distance as EMD
from scipy.stats import energy_distance as energy


distances = [
    d.braycurtis, d.canberra, d.chebyshev, d.cityblock,
    d.cosine, d.euclidean, d.jensenshannon,  EMD, energy
]

# Ian Porto's distances (Chamfer, Hausdorff, L1, L2) — implemented on
# histogram feature vectors for direct comparison with the SLBP thesis.
# Note: these compute distances between aligned 1D feature vectors. Ian's
# thesis uses Chamfer/Hausdorff on 3D point sets (Section 4.2.7-4.2.8),
# which is semantically different. L1 and L2 are aliases for cityblock and
# euclidean, already above.

def chamfer(u, v):
    """Chamfer distance between two feature vectors.
    For aligned 1D vectors of same length, this reduces to mean absolute
    difference: Σ|u[i]-v[i]| / N."""
    return np.mean(np.abs(u - v))

def hausdorff(u, v):
    """Hausdorff distance between two feature vectors.
    For aligned 1D vectors, this is the maximum absolute element-wise
    difference: max_i |u[i]-v[i]| (same as Chebyshev/L∞)."""
    return np.max(np.abs(u - v))

# L1 and L2 are already cityblock and euclidean above.
# Ian also uses them directly on feature maps (Section 4.2.9-4.2.10).

ian_distances = [
    chamfer, hausdorff,
]



def run(args):
    df_baseline = pd.read_csv(args.score_file)
    df_features = pd.read_csv(args.feature_file)
    stimuli = len(df_baseline.index)
    output = []
    for i in range(stimuli):
        row = df_baseline.iloc[[i]]
        PC_name = row["SIGNAL_LOCATION"].tolist()[0].strip()
        PC_referencia = row["REF_LOCATION"].tolist()[0].strip()
        PC_row = df_features[df_features["file"].str.match(PC_name)]
        ref_row = df_features[df_features["file"].str.match(PC_referencia)]
        PC_columns = [col for col in PC_row if col.startswith('fv')]
        PC_features = PC_row[PC_columns].to_numpy().flatten()
        ref_columns = [c for c in ref_row if c.startswith('fv')]
        reference_features = ref_row[ref_columns].to_numpy().flatten()
        line = {
            'nome_do_pc': PC_name,
            'nome_referencia': PC_referencia,
            'subjective_MOS': row["SCORE"].tolist()[0]
        }
        for d in distances:
            d_name = d.__name__
            line["d_" + d_name] = d(reference_features, PC_features)
        output.append(line)
    df_out = pd.DataFrame.from_dict(output)
    df_out.to_csv(args.output_file, index=False)


def parse_args(argv):
    """Parses command line arguments."""
    parser = argparse_flags.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument(
        "--score_file", "-s",
        type=str, dest="score_file",
        help="File containing the database files with subjective scores.")
    parser.add_argument(
        "--feature_file", "-f",
        type=str, dest="feature_file",
        help="File containing the features.")
    parser.add_argument(
        "--output_file", "-o",
        type=str, dest="output_file",
        help="File containing the processed distances.")
    args = parser.parse_args(argv[1:])
    return args


def main(args):
    run(args)


if __name__ == "__main__":
    app.run(main, flags_parser=parse_args)
