package io.lattice.network.compression;

import io.lattice.network.NativeCompression;
import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;

import java.nio.ByteBuffer;
import java.util.List;

public class NativeCompressionDecoder extends ByteToMessageDecoder {
    private final int maxSize;

    public NativeCompressionDecoder(int maxSize) {
        this.maxSize = Math.min(maxSize, NativeCompression.MAX_DECOMPRESSED_SIZE);
    }

    @Override
    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) throws Exception {
        if (in.readableBytes() == 0) {
            return;
        }

        // We must read a VarInt but it may be incomplete - parse without advancing readerIndex until complete
        final int startIndex = in.readerIndex();
        int[] varintParse = tryReadVarInt(in);
        if (varintParse == null) {
            // Not enough bytes yet to read the VarInt
            return;
        }

        int uncompressedSize = varintParse[0];
        int varintLength = varintParse[1];

        // Advance readerIndex past the VarInt
        in.readerIndex(startIndex + varintLength);

        if (uncompressedSize == 0) {
            // Data is uncompressed: everything remaining in this frame is plaintext
            ByteBuf copy = ctx.alloc().buffer(in.readableBytes());
            copy.writeBytes(in);
            out.add(copy);
            return;
        }

        if (uncompressedSize > maxSize) {
            throw new RuntimeException("Compressed packet is larger than allowed " + uncompressedSize + " > " + maxSize);
        }

        int compressedLen = in.readableBytes();
        if (compressedLen == 0) {
            throw new RuntimeException("No compressed data after size field");
        }

        // Try direct-buffer zero-copy path first
        if (NativeCompression.isNativeAvailable() && in.isDirect()) {
            ByteBuffer srcNio = in.nioBuffer(in.readerIndex(), compressedLen);
            ByteBuf dstBuf = ctx.alloc().directBuffer(uncompressedSize);
            boolean addedToOut = false;
            try {
                ByteBuffer dstNio = dstBuf.nioBuffer(0, uncompressedSize);
                // 只使用deflate解压缩，移除对zstd的依赖
                int written = NativeCompression.decompressDeflateDirect(srcNio, compressedLen, dstNio, uncompressedSize);
                if (written == uncompressedSize) {
                    // consume compressed bytes
                    in.skipBytes(compressedLen);
                    dstBuf.writerIndex(written);
                    out.add(dstBuf);
                    addedToOut = true;
                    return;
                }
            } catch (Throwable t) {
                // fall through to fallback
            } finally {
                // only release if we did not hand ownership to the out list
                if (!addedToOut) {
                    dstBuf.release();
                }
            }
        }

        // Fallback: copy compressed bytes to heap and use byte[] JNI APIs
        byte[] compressed = new byte[compressedLen];
        in.readBytes(compressed);

        byte[] uncompressed = null;
        if (NativeCompression.isNativeAvailable()) {
            // 只使用deflate解压缩，移除对zstd的依赖
            uncompressed = NativeCompression.decompressDeflate(compressed, uncompressedSize);
        }

        if (uncompressed == null || uncompressed.length != uncompressedSize) {
            throw new RuntimeException("Failed to decompress packet");
        }

        ByteBuf decompressed = ctx.alloc().buffer(uncompressedSize);
        decompressed.writeBytes(uncompressed);
        out.add(decompressed);
    }

    // Returns int[2] = { value, lengthInBytes } or null if not enough bytes available to parse VarInt
    private static int[] tryReadVarInt(ByteBuf buf) {
        int value = 0;
        int position = 0;
        int index = buf.readerIndex();

        while (true) {
            if (index >= buf.writerIndex()) return null; // not enough bytes
            byte currentByte = buf.getByte(index++);
            value |= (currentByte & 0x7F) << position;

            if ((currentByte & 0x80) == 0) {
                int length = index - buf.readerIndex();
                return new int[] { value, length };
            }

            position += 7;
            if (position >= 32) throw new RuntimeException("VarInt is too big");
        }
    }
}