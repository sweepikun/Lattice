package io.lattice.network.compression;

import io.netty.channel.ChannelPipeline;
import io.netty.handler.codec.compression.ZlibCodecFactory;
import io.netty.handler.codec.compression.ZlibWrapper;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.Connection;
import net.minecraft.network.ConnectionProtocol;

import java.util.concurrent.atomic.AtomicInteger;

public class NativeCompressionManager {
    private static final String COMPRESSOR_NAME = "native-compress";
    private static final String DECOMPRESSOR_NAME = "native-decompress";

    // Initial compression level - can be adjusted via config
    private static final AtomicInteger compressionLevel = new AtomicInteger(3);
    
    // Default max size (8MB)
    private static final int DEFAULT_MAX_SIZE = 8388608;

    public static void setCompressionLevel(int level) {
        compressionLevel.set(level);
    }

    public static void setCompressionThreshold(Connection connection, int threshold) {
        ChannelPipeline pipeline = connection.channel.pipeline();

        if (threshold >= 0) {
            if (pipeline.get(COMPRESSOR_NAME) instanceof NativeCompressionEncoder) {
                // Already using native compression
                pipeline.remove(COMPRESSOR_NAME);
            }
            if (pipeline.get(DECOMPRESSOR_NAME) instanceof NativeCompressionDecoder) {
                pipeline.remove(DECOMPRESSOR_NAME);
            }

            pipeline.addBefore("packet_handler", COMPRESSOR_NAME,
                    new NativeCompressionEncoder(threshold, compressionLevel.get()));
            pipeline.addBefore("packet_handler", DECOMPRESSOR_NAME,
                    new NativeCompressionDecoder(DEFAULT_MAX_SIZE));
        } else {
            if (pipeline.get(COMPRESSOR_NAME) != null) {
                pipeline.remove(COMPRESSOR_NAME);
            }
            if (pipeline.get(DECOMPRESSOR_NAME) != null) {
                pipeline.remove(DECOMPRESSOR_NAME);
            }
        }
    }

    // Fallback to vanilla compression if native compression fails
    public static void enableVanillaCompression(Connection connection, int threshold) {
        ChannelPipeline pipeline = connection.channel.pipeline();

        if (pipeline.get(COMPRESSOR_NAME) != null) {
            pipeline.remove(COMPRESSOR_NAME);
        }
        if (pipeline.get(DECOMPRESSOR_NAME) != null) {
            pipeline.remove(DECOMPRESSOR_NAME);
        }

        int compressionThreshold = Math.max(threshold, 0);
        pipeline.addBefore("packet_handler", "decompress",
                ZlibCodecFactory.newZlibDecoder(ZlibWrapper.NONE));
        pipeline.addBefore("packet_handler", "compress",
                ZlibCodecFactory.newZlibEncoder(ZlibWrapper.NONE, compressionLevel.get()));
    }
}
