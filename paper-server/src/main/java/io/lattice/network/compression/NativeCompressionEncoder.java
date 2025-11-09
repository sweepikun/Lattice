package io.lattice.network.compression;

import io.lattice.network.NativeCompression;
import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;

@ChannelHandler.Sharable
public class NativeCompressionEncoder extends MessageToByteEncoder<ByteBuf> {
    private final int threshold;
    private final int compressionLevel;

    public NativeCompressionEncoder(int threshold, int compressionLevel) {
        this.threshold = threshold;
        this.compressionLevel = compressionLevel;
    }

    @Override
    protected void encode(ChannelHandlerContext ctx, ByteBuf in, ByteBuf out) throws Exception {
        int readable = in.readableBytes();
        if (readable < threshold) {
            // No need to compress
            writeVarInt(out, 0);
            out.writeBytes(in);
            return;
        }

        // Prefer direct buffers to enable zero-copy through JNI
        if (NativeCompression.isNativeAvailable() && in.isDirect()) {
            ByteBuf dst = ctx.alloc().directBuffer(NativeCompression.deflateCompressBound(readable));
            try {
                int written = NativeCompression.compressDeflateDirect(in.nioBuffer(), readable, dst.nioBuffer(0, dst.capacity()), dst.capacity(), compressionLevel);
                if (written > 0 && written < readable) {
                    writeVarInt(out, readable);
                    // write compressed bytes from dst
                    out.writeBytes(dst, 0, written);
                    return;
                }
            } catch (Throwable ignored) {
                // fall back
            } finally {
                dst.release();
            }
        }

        // Fallback path using heap arrays and byte[] JNI APIs
        byte[] uncompressed = new byte[readable];
        in.readBytes(uncompressed);

        byte[] compressed = null;
        if (NativeCompression.isNativeAvailable()) {
            compressed = NativeCompression.compressDeflate(uncompressed, compressionLevel);
        }

        // If compression is effective, write compressed data
        if (compressed != null && compressed.length < readable) {
            writeVarInt(out, readable);
            out.writeBytes(compressed);
        } else {
            // Otherwise write uncompressed
            writeVarInt(out, 0);
            out.writeBytes(uncompressed);
        }
    }

    private static void writeVarInt(ByteBuf buf, int value) {
        while (true) {
            if ((value & ~0x7F) == 0) {
                buf.writeByte(value);
                return;
            }
            buf.writeByte((value & 0x7F) | 0x80);
            value >>>= 7;
        }
    }
}