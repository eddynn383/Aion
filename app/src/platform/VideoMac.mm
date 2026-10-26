#include "Video.h"

#include <QImage>

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <cstring>

namespace platform {

bool encodeVideoNative(const QStringList& frames, int fps, const QString& output, QString* error)
{
    @autoreleasepool {
        if (frames.isEmpty()) {
            return false;
        }
        const QImage first(frames.first());
        // H.264 cere dimensiuni pare.
        const int width = first.width() & ~1;
        const int height = first.height() & ~1;
        if (width <= 0 || height <= 0) {
            if (error) {
                *error = QStringLiteral("Cadrele nu s-au putut citi.");
            }
            return false;
        }

        NSURL* url = [NSURL fileURLWithPath:output.toNSString()];
        [[NSFileManager defaultManager] removeItemAtURL:url error:nil];
        NSError* nsError = nil;
        AVAssetWriter* writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&nsError];
        if (writer == nil) {
            if (error) {
                *error = QString::fromNSString(nsError.localizedDescription);
            }
            return false;
        }
        NSDictionary* settings = @{
            AVVideoCodecKey : AVVideoCodecTypeH264,
            AVVideoWidthKey : @(width),
            AVVideoHeightKey : @(height),
            AVVideoCompressionPropertiesKey : @ { AVVideoAverageBitRateKey : @(width * height * 6) },
        };
        AVAssetWriterInput* input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                                       outputSettings:settings];
        input.expectsMediaDataInRealTime = NO;
        // Cheile CoreVideo sunt CFStringRef: în Objective-C se folosesc prin cast „bridge” (doar sintaxă C).
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
        NSDictionary* attributes = @{
            (__bridge NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA),
            (__bridge NSString*)kCVPixelBufferWidthKey : @(width),
            (__bridge NSString*)kCVPixelBufferHeightKey : @(height),
        };
#pragma clang diagnostic pop
        AVAssetWriterInputPixelBufferAdaptor* adaptor =
            [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input
                                                                             sourcePixelBufferAttributes:attributes];
        [writer addInput:input];
        if (![writer startWriting]) {
            if (error) {
                *error = QString::fromNSString(writer.error.localizedDescription);
            }
            return false;
        }
        [writer startSessionAtSourceTime:kCMTimeZero];

        for (qsizetype i = 0; i < frames.size(); ++i) {
            @autoreleasepool {
                // ARGB32 în memorie, pe little-endian, e exact BGRA.
                QImage image = QImage(frames[i]).convertToFormat(QImage::Format_ARGB32);
                if (image.width() != width || image.height() != height) {
                    image = image.copy(0, 0, width, height);
                }
                while (!input.readyForMoreMediaData) {
                    [NSThread sleepForTimeInterval:0.002];
                }
                CVPixelBufferRef buffer = nullptr;
                if (CVPixelBufferPoolCreatePixelBuffer(nullptr, adaptor.pixelBufferPool, &buffer) != kCVReturnSuccess) {
                    if (error) {
                        *error = QStringLiteral("Encoder-ul video nu a putut aloca un cadru.");
                    }
                    [input markAsFinished];
                    [writer cancelWriting];
                    return false;
                }
                CVPixelBufferLockBaseAddress(buffer, 0);
                auto* dst = static_cast<unsigned char*>(CVPixelBufferGetBaseAddress(buffer));
                const size_t dstStride = CVPixelBufferGetBytesPerRow(buffer);
                for (int y = 0; y < height; ++y) {
                    std::memcpy(dst + static_cast<size_t>(y) * dstStride, image.constScanLine(y),
                        static_cast<size_t>(width) * 4);
                }
                CVPixelBufferUnlockBaseAddress(buffer, 0);
                [adaptor appendPixelBuffer:buffer withPresentationTime:CMTimeMake(static_cast<int64_t>(i), fps)];
                CVPixelBufferRelease(buffer);
            }
        }

        [input markAsFinished];
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        [writer finishWritingWithCompletionHandler:^{
            dispatch_semaphore_signal(done);
        }];
        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        if (writer.status != AVAssetWriterStatusCompleted) {
            if (error) {
                *error = QString::fromNSString(writer.error.localizedDescription);
            }
            return false;
        }
        return true;
    }
}

} // namespace platform
