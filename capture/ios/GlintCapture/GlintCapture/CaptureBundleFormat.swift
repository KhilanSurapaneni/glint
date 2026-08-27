import CoreImage
import Foundation
import simd

// Little-endian byte encoding helpers — matches docs/CAPTURE_FORMAT.md's explicit choice of
// little-endian, and both Apple Silicon Macs and iPhones are little-endian ARM anyway, so this
// is also just "the natural byte order," not a conversion.
private extension Data {
    mutating func appendLittleEndian(_ value: UInt32) {
        var v = value.littleEndian
        // Qualified as Swift.withUnsafeBytes explicitly — inside a Data extension, the bare
        // name is ambiguous with Data's own (differently-shaped) instance method of the same name.
        Swift.withUnsafeBytes(of: &v) { append(contentsOf: $0) }
    }

    mutating func appendLittleEndian(_ value: Float) {
        appendLittleEndian(value.bitPattern)
    }
}

// Encodes CapturedFrame data into the .glcb format from docs/CAPTURE_FORMAT.md. Used
// identically by both the file-writing and TCP-sending paths — this code has no idea which
// one it's feeding.
enum CaptureBundleEncoder {
    private static let magic: [UInt8] = Array("GLCB".utf8)
    private static let formatVersion: UInt32 = 1
    private static let ciContext = CIContext()

    static func encodeSessionHeader(width: Int, height: Int, fx: Float, fy: Float, cx: Float,
                                     cy: Float) -> Data {
        var data = Data()
        data.append(contentsOf: magic)
        data.appendLittleEndian(formatVersion)
        data.appendLittleEndian(UInt32(width))
        data.appendLittleEndian(UInt32(height))
        data.appendLittleEndian(fx)
        data.appendLittleEndian(fy)
        data.appendLittleEndian(cx)
        data.appendLittleEndian(cy)
        return data
    }

    static func encodeFrame(_ frame: CapturedFrame, width: Int, height: Int) -> Data? {
        // JPEG encoding doesn't care that the source is BGRA rather than plain RGB — it
        // produces a standard JPEG either way, which stb_image already decodes correctly on
        // the C++ side (same as it already does for Replica's frame*.jpg files).
        guard let jpegData = ciContext.jpegRepresentation(
            of: CIImage(cvPixelBuffer: frame.rgbPixelBuffer),
            colorSpace: CGColorSpaceCreateDeviceRGB()
        ) else {
            return nil
        }

        let depthData = tightlyPackedFloatBytes(frame.depthPixelBuffer, width: width, height: height)

        var data = Data()

        // Row-major 4x4 pose — same convention io/dataset.cpp already reads Replica's
        // traj.txt with.
        for row in 0..<4 {
            for col in 0..<4 {
                data.appendLittleEndian(frame.cameraToWorld[col][row])
            }
        }

        data.appendLittleEndian(UInt32(jpegData.count))
        data.append(jpegData)

        data.appendLittleEndian(UInt32(depthData.count))
        data.append(depthData)

        return data
    }

    // Copies a CVPixelBuffer's float32 depth data row-by-row, using its actual byte stride —
    // NOT one contiguous block copy, since bytesPerRow can include padding beyond the real
    // width*4 bytes of data. Getting this wrong wouldn't crash, just silently misalign every
    // row after the first.
    private static func tightlyPackedFloatBytes(_ pixelBuffer: CVPixelBuffer, width: Int,
                                                  height: Int) -> Data {
        CVPixelBufferLockBaseAddress(pixelBuffer, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(pixelBuffer, .readOnly) }

        let bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer)
        let tightRowBytes = width * MemoryLayout<Float>.size
        guard let baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer) else {
            return Data(count: tightRowBytes * height)
        }

        var result = Data(capacity: tightRowBytes * height)
        for row in 0..<height {
            let rowStart = baseAddress.advanced(by: row * bytesPerRow)
            result.append(Data(bytes: rowStart, count: tightRowBytes))
        }
        return result
    }
}
