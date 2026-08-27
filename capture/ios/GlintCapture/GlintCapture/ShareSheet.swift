import SwiftUI
import UIKit

// SwiftUI has no native Share Sheet — this wraps UIKit's UIActivityViewController, which is
// what actually gives access to AirDrop/Messages/Files.
struct ShareSheet: UIViewControllerRepresentable {
    let items: [Any]

    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: items, applicationActivities: nil)
    }

    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}
