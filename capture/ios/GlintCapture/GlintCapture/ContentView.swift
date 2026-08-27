import SwiftUI

struct ContentView: View {
    @StateObject private var arSessionManager = ARSessionManager()

    private enum CaptureMode: String, CaseIterable {
        case file = "Save to File"
        case live = "Live to Mac"
    }

    @State private var mode: CaptureMode = .file
    @State private var liveHost = ""
    @State private var showShareSheet = false
    @State private var pulse = false

    var body: some View {
        VStack(spacing: 24) {
            // Mode picker + host field only shown before capture starts — nothing to
            // reconfigure mid-recording.
            if !arSessionManager.isCapturing && arSessionManager.lastSavedFileURL == nil {
                Picker("Mode", selection: $mode) {
                    ForEach(CaptureMode.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal)

                if mode == .live {
                    TextField("Mac's IP address", text: $liveHost)
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.decimalPad)
                        .padding(.horizontal)
                }
            }

            if arSessionManager.isCapturing {
                HStack {
                    Circle()
                        .fill(Color.red)
                        .frame(width: 14, height: 14)
                        .opacity(pulse ? 1.0 : 0.3)
                        .onAppear {
                            withAnimation(.easeInOut(duration: 0.6).repeatForever(autoreverses: true)) {
                                pulse = true
                            }
                        }
                    Text("Recording").font(.headline)
                }

                Text("\(arSessionManager.frameCount) frames · "
                    + "\(String(format: "%.0f", arSessionManager.elapsedSeconds))s")
                    .font(.subheadline)
                    .monospacedDigit()

                // Depth/tracking quality is genuinely motion-sensitive — this needs to stay
                // visible the whole time it's recording, not just shown once beforehand.
                Text("Walk slowly. Point your phone at the walls and furniture.")
                    .multilineTextAlignment(.center)
                    .foregroundStyle(.secondary)
                    .padding(.horizontal)
            }

            if let url = arSessionManager.lastSavedFileURL {
                Text("Saved: \(arSessionManager.frameCount) frames, "
                    + "\(String(format: "%.0f", arSessionManager.elapsedSeconds))s")
                    .font(.headline)

                Button("Send") {
                    showShareSheet = true
                }
                .sheet(isPresented: $showShareSheet) {
                    ShareSheet(items: [url])
                }
            }

            Button(action: toggleCapture) {
                Text(arSessionManager.isCapturing ? "Stop" : "Start Capture")
                    .font(.title2)
                    .padding()
                    .frame(maxWidth: .infinity)
                    .background(arSessionManager.isCapturing ? Color.red : Color.blue)
                    .foregroundStyle(.white)
                    .clipShape(RoundedRectangle(cornerRadius: 12))
            }
            .padding(.horizontal)
        }
        .padding()
        .onAppear {
            arSessionManager.start()
        }
    }

    private func toggleCapture() {
        if arSessionManager.isCapturing {
            arSessionManager.stopCapture()
        } else if mode == .file {
            arSessionManager.startCaptureToFile()
        } else {
            arSessionManager.startCaptureLive(host: liveHost, port: 5555)
        }
    }
}

#Preview {
    ContentView()
}
