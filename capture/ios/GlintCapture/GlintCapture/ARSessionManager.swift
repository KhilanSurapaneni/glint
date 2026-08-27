import ARKit
import Combine

// Owns the connection to ARKit's tracking system — same role as glint::gpu::Device plays for
// the GPU on the C++ side. Also an ObservableObject now: SwiftUI's UI (2.6) reads these
// @Published properties directly to show live capture state.
class ARSessionManager: NSObject, ObservableObject, ARSessionDelegate {
    private let session = ARSession()

    @Published private(set) var isCapturing = false
    @Published private(set) var frameCount = 0
    @Published private(set) var elapsedSeconds: TimeInterval = 0
    @Published private(set) var lastSavedFileURL: URL?

    private var sink: ByteSink?
    private var wroteSessionHeader = false
    private var captureStartTime: Date?
    private var elapsedTimer: Timer?
    private var pendingFileURL: URL?

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

    func startCaptureToFile() {
        let url = FileManager.default.temporaryDirectory
            .appendingPathComponent("capture-\(Int(Date().timeIntervalSince1970)).glcb")
        guard let fileSink = FileByteSink(url: url) else {
            print("failed to create capture file at \(url)")
            return
        }
        pendingFileURL = url
        beginWriting(to: fileSink)
    }

    func startCaptureLive(host: String, port: UInt16) {
        pendingFileURL = nil
        beginWriting(to: TCPByteSink(host: host, port: port))
    }

    func stopCapture() {
        sink?.close()
        sink = nil
        isCapturing = false
        elapsedTimer?.invalidate()
        elapsedTimer = nil
        lastSavedFileURL = pendingFileURL
    }

    private func beginWriting(to newSink: ByteSink) {
        lastSavedFileURL = nil
        sink = newSink
        wroteSessionHeader = false
        frameCount = 0
        captureStartTime = Date()
        isCapturing = true

        elapsedTimer?.invalidate()
        elapsedTimer = Timer.scheduledTimer(withTimeInterval: 0.2, repeats: true) { [weak self] _ in
            guard let self, let start = self.captureStartTime else { return }
            self.elapsedSeconds = Date().timeIntervalSince(start)
        }
    }

    // Called automatically by ARKit once per tracked frame — not guaranteed to run on the main
    // thread, unlike SwiftUI's @Published requirements, hence the explicit dispatch below.
    func session(_ session: ARSession, didUpdate frame: ARFrame) {
        guard let captured = FrameExtraction.extract(from: frame) else { return }
        guard isCapturing, let sink else { return }

        let width = CVPixelBufferGetWidth(captured.rgbPixelBuffer)
        let height = CVPixelBufferGetHeight(captured.rgbPixelBuffer)

        // The session header needs this frame's intrinsics/dimensions, so it's written once,
        // lazily, on the first frame that actually arrives — not eagerly when capture starts,
        // before any real data exists yet.
        if !wroteSessionHeader {
            let header = CaptureBundleEncoder.encodeSessionHeader(
                width: width, height: height,
                fx: captured.fx, fy: captured.fy, cx: captured.cx, cy: captured.cy)
            sink.write(header)
            wroteSessionHeader = true
        }

        guard let frameData = CaptureBundleEncoder.encodeFrame(captured, width: width, height: height) else {
            return
        }
        sink.write(frameData)

        DispatchQueue.main.async {
            self.frameCount += 1
        }
    }
}
