import ARKit

// Owns the connection to ARKit's tracking system — same role as glint::gpu::Device plays for
// the GPU on the C++ side. One instance, created once, running for as long as we're capturing.
class ARSessionManager: NSObject, ARSessionDelegate {
    private let session = ARSession()
    private var frameCount = 0

    func start() {
        let configuration = ARWorldTrackingConfiguration()

        // Opt in to LiDAR depth data — without this, ARFrame.sceneDepth stays nil even on a
        // LiDAR-capable device. Checked first since running this on a non-LiDAR device (like
        // a base iPhone) would otherwise fail silently.
        if ARWorldTrackingConfiguration.supportsFrameSemantics(.sceneDepth) {
            configuration.frameSemantics.insert(.sceneDepth)
        } else {
            print("WARNING: this device has no LiDAR — capturing without depth data")
        }

        session.delegate = self
        session.run(configuration)
    }

    func stop() {
        session.pause()
    }

    // Called automatically by ARKit once per tracked frame — same "callback, not something we
    // call ourselves" pattern as the scroll callback on the C++ side.
    func session(_ session: ARSession, didUpdate frame: ARFrame) {
        frameCount += 1

        guard let captured = FrameExtraction.extract(from: frame) else {
            if frameCount % 30 == 0 {
                print("frame #\(frameCount): no depth yet, skipping")
            }
            return
        }

        if frameCount % 30 == 0 {  // print occasionally, not every single frame
            print("frame #\(frameCount): fx=\(captured.fx) fy=\(captured.fy) "
                + "cx=\(captured.cx) cy=\(captured.cy)")
        }
    }
}
