import ARKit
import CoreImage

// One captured frame's raw data, pulled out of an ARFrame — the Swift-side mirror of
// core::Frame/core::Camera on the C++ side. Encoding this to bytes for the file bundle is the
// next step (2.4/2.5), not this one — this just grabs and converts the raw pieces.
struct CapturedFrame {
    let rgbPixelBuffer: CVPixelBuffer  // BGRA8 — converted from ARKit's native YCbCr
    let depthPixelBuffer: CVPixelBuffer  // already float32 meters, ARKit's native format
    let cameraToWorld: simd_float4x4
    let fx: Float
    let fy: Float
    let cx: Float
    let cy: Float
}

enum FrameExtraction {
    // Reused across frames rather than created fresh each time — CIContext setup is
    // relatively expensive.
    private static let ciContext = CIContext()

    // ARKit's capturedImage arrives as YCbCr (camera-native), not RGB — CoreImage handles the
    // conversion. Rendering into BGRA8 here (a well-supported CoreImage output format);
    // reordering to plain RGB happens later, when actually writing bytes to the file (2.5).
    private static func convertToBGRA(_ pixelBuffer: CVPixelBuffer) -> CVPixelBuffer? {
        let ciImage = CIImage(cvPixelBuffer: pixelBuffer)
        let width = CVPixelBufferGetWidth(pixelBuffer)
        let height = CVPixelBufferGetHeight(pixelBuffer)

        var output: CVPixelBuffer?
        CVPixelBufferCreate(kCFAllocatorDefault, width, height,
                             kCVPixelFormatType_32BGRA, nil, &output)
        guard let bgraBuffer = output else { return nil }

        ciContext.render(ciImage, to: bgraBuffer)
        return bgraBuffer
    }

    static func extract(from frame: ARFrame) -> CapturedFrame? {
        guard let rgb = convertToBGRA(frame.capturedImage) else { return nil }
        guard let depth = frame.sceneDepth?.depthMap else { return nil }

        // ARCamera.intrinsics is a 3x3 matrix: [[fx,0,0],[0,fy,0],[cx,cy,1]] in simd's
        // column-major storage — columns.0/.1 hold the focal lengths on their own diagonal,
        // columns.2 holds the principal point.
        let intrinsics = frame.camera.intrinsics
        let fx = intrinsics.columns.0.x
        let fy = intrinsics.columns.1.y
        let cx = intrinsics.columns.2.x
        let cy = intrinsics.columns.2.y

        return CapturedFrame(
            rgbPixelBuffer: rgb,
            depthPixelBuffer: depth,
            cameraToWorld: frame.camera.transform,
            fx: fx,
            fy: fy,
            cx: cx,
            cy: cy
        )
    }
}
