#include <gtest/gtest.h>

#include <Eigen/Core>

#include "core/types.hpp"

TEST(CoreTypes, CameraHoldsValues) {
  glint::core::Camera camera;
  camera.width = 1200;
  camera.height = 680;
  camera.fx = 600.0f;
  camera.fy = 600.0f;
  camera.cx = 599.5f;
  camera.cy = 339.5f;

  EXPECT_EQ(camera.width, 1200);
  EXPECT_EQ(camera.height, 680);
  EXPECT_FLOAT_EQ(camera.fx, 600.0f);
  EXPECT_FLOAT_EQ(camera.cy, 339.5f);
}

TEST(CoreTypes, PoseDefaultsToIdentity) {
  glint::core::Pose pose;
  EXPECT_TRUE(pose.camera_to_world.isApprox(Eigen::Matrix4f::Identity()));
}

TEST(CoreTypes, FrameHoldsPixelsAndPose) {
  glint::core::Frame frame;
  frame.rgb.resize(3 * 2 * 3, 128);   // tiny fake 3x2 RGB image
  frame.depth.resize(3 * 2, 1000);
  frame.pose.camera_to_world(0, 3) = 5.0f;  // pretend the camera moved along X

  EXPECT_EQ(frame.rgb.size(), 18u);
  EXPECT_EQ(frame.depth.size(), 6u);
  EXPECT_FLOAT_EQ(frame.pose.camera_to_world(0, 3), 5.0f);
}
