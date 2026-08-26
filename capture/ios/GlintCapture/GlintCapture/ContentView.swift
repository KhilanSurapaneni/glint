//
//  ContentView.swift
//  GlintCapture
//
//  Created by Khilan Surapaneni on 8/26/26.
//

import SwiftUI

struct ContentView: View {
    // @StateObject-style lifetime isn't needed here since ARSessionManager doesn't publish
    // any SwiftUI-observable state yet — that arrives with the real UI in step 2.6.
    private let arSessionManager = ARSessionManager()

    var body: some View {
        VStack {
            Image(systemName: "globe")
                .imageScale(.large)
                .foregroundStyle(.tint)
            Text("Hello, world!")
        }
        .padding()
        .onAppear {
            arSessionManager.start()
        }
    }
}

#Preview {
    ContentView()
}
