# Failures

Append-only catalogue of observed failure modes. For each: symptom, hypothesized cause,
whether it was mitigated, and what more time would buy. Entries are never deleted or softened
to make the project look cleaner.

Still empty after M1, for a genuine reason: this file is for observed failure modes of
reconstruction/tracking (motion blur, textureless surfaces, sensor dropout, and the like), and
M1 doesn't do reconstruction or tracking — it unprojects Replica's clean, synthetic ground-truth
depth directly. There's no camera tracking to lose, no real sensor noise to hit. This starts
filling in for real once M2 (Gaussian Splatting) and M3 (SLAM) exist to actually produce these
failure modes — and once real ARKit capture happens, since Replica's depth has none of a real
LiDAR sensor's noise/dropout characteristics.

One current gap worth watching rather than reporting yet: `src/shaders/unproject.metal`
doesn't filter invalid/zero depth pixels, so a frame with depth dropout would show stray points
at the camera's own position instead of a gap — not yet actually observed (Replica's rendered
depth appears to be complete indoors), but likely to surface for real once live ARKit capture
happens, where LiDAR dropout on dark/transparent surfaces is expected.
