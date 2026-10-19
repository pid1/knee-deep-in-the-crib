// nudge: post a tiny mouse-move so apps that pause on idle (Photo Booth) keep running.
import CoreGraphics
let p = CGEvent(source: nil)!.location
for dx in [1.0, -1.0] {
    CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: CGPoint(x: p.x + dx, y: p.y), mouseButton: .left)?.post(tap: .cghidEventTap)
}
