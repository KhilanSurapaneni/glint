import Foundation
import Network

// Where encoded capture bytes go — a file, or a live socket. The encoder in
// CaptureBundleFormat.swift doesn't know or care which one it's writing to.
protocol ByteSink {
    func write(_ data: Data)
    func close()
}

final class FileByteSink: ByteSink {
    private var fileHandle: FileHandle?
    let url: URL

    init?(url: URL) {
        self.url = url
        guard FileManager.default.createFile(atPath: url.path, contents: nil) else {
            return nil
        }
        fileHandle = try? FileHandle(forWritingTo: url)
    }

    func write(_ data: Data) {
        fileHandle?.write(data)
    }

    func close() {
        try? fileHandle?.close()
        fileHandle = nil
    }
}

final class TCPByteSink: ByteSink {
    private let connection: NWConnection
    private var isReady = false
    private var pendingData: [Data] = []

    init(host: String, port: UInt16) {
        connection = NWConnection(
            host: NWEndpoint.Host(host),
            port: NWEndpoint.Port(rawValue: port)!,
            using: .tcp
        )
        // Sending before the connection reaches .ready would silently drop or misqueue data —
        // buffer anything written early and flush it once the connection actually confirms it's up.
        connection.stateUpdateHandler = { [weak self] state in
            if case .ready = state {
                self?.flushPending()
            }
        }
        connection.start(queue: .global())
    }

    private func flushPending() {
        isReady = true
        let queued = pendingData
        pendingData.removeAll()
        for data in queued {
            send(data)
        }
    }

    private func send(_ data: Data) {
        connection.send(content: data, completion: .contentProcessed { _ in })
    }

    func write(_ data: Data) {
        if isReady {
            send(data)
        } else {
            pendingData.append(data)
        }
    }

    func close() {
        connection.cancel()
    }
}
