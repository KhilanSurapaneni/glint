#!/usr/bin/env bash
# Downloads the Replica RGB-D sequences (8 scenes, ~11.6 GB zipped) into assets/replica/.
#
# This is the same archive used by NICE-SLAM and SplaTAM — camera trajectories rendered by the
# iMAP authors over Meta's original Replica scene meshes, hosted by ETH Zurich's Computer
# Vision Group. Verified structure (2026-08-21): each scene is a results/ folder of
# frame*.jpg (1200x680 RGB) + depth*.png (1200x680, 16-bit) pairs, plus a traj.txt with one
# line per frame — 16 space-separated floats, a row-major 4x4 camera-to-world matrix.
# Intrinsics (fixed across all scenes): fx=fy=600.0, cx=599.5, cy=339.5, png_depth_scale=6553.5.
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p assets/replica
cd assets/replica

curl -L -o Replica.zip https://cvg-data.inf.ethz.ch/nice-slam/data/Replica.zip
unzip -q Replica.zip
rm Replica.zip
