// camsnap: grab one JPEG from a named camera. Usage: camsnap <out.jpg> [device-name-substring]
import AVFoundation
import AppKit

let args = CommandLine.arguments
let out = args.count > 1 ? args[1] : "snap.jpg"
let want = args.count > 2 ? args[2] : "FaceTime"

let status = AVCaptureDevice.authorizationStatus(for: .video)
FileHandle.standardError.write("auth status: \(status.rawValue) (0=notDetermined 1=restricted 2=denied 3=authorized)\n".data(using: .utf8)!)
if status == .notDetermined {
    let sem = DispatchSemaphore(value: 0)
    AVCaptureDevice.requestAccess(for: .video) { ok in
        FileHandle.standardError.write("requestAccess -> \(ok)\n".data(using: .utf8)!); sem.signal()
    }
    sem.wait()
}

let devs = AVCaptureDevice.DiscoverySession(deviceTypes: [.builtInWideAngleCamera, .external, .continuityCamera],
                                            mediaType: .video, position: .unspecified).devices
guard let dev = devs.first(where: { $0.localizedName.contains(want) }) else {
    FileHandle.standardError.write("no camera matching \(want): \(devs.map { $0.localizedName })\n".data(using: .utf8)!); exit(2)
}

final class Grabber: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    var frames = 0
    let done = DispatchSemaphore(value: 0)
    func captureOutput(_ o: AVCaptureOutput, didOutput sb: CMSampleBuffer, from c: AVCaptureConnection) {
        frames += 1
        guard frames == 15, let px = CMSampleBufferGetImageBuffer(sb) else { return }  // let exposure settle
        let ci = CIImage(cvPixelBuffer: px)
        let rep = NSBitmapImageRep(ciImage: ci)
        try? rep.representation(using: .jpeg, properties: [.compressionFactor: 0.85])?.write(to: URL(fileURLWithPath: out))
        done.signal()
    }
}

let s = AVCaptureSession()
s.sessionPreset = .hd1280x720
s.addInput(try! AVCaptureDeviceInput(device: dev))
let o = AVCaptureVideoDataOutput()
let g = Grabber()
o.setSampleBufferDelegate(g, queue: DispatchQueue(label: "cam"))
s.addOutput(o)
s.startRunning()
if g.done.wait(timeout: .now() + 8) == .timedOut {
    FileHandle.standardError.write("timeout, frames=\(g.frames)\n".data(using: .utf8)!); exit(1)
}
s.stopRunning()
print(out)
