package io.lattice.network;

import io.lattice.config.LatticeConfig;
import io.lattice.network.compression.NativeCompressionManager;
import io.netty.channel.ChannelPipeline;
import io.papermc.paper.network.ConnectionEvent;
import net.minecraft.network.Connection;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Connection {
    private static final Logger LOGGER = LoggerFactory.getLogger(Connection.class);
    
    private final Connection connection;
    private final io.netty.channel.Channel channel;

    public Connection(Connection connection) {
        this.connection = connection;
        this.channel = connection.channel;
    }

    public void setupCompression(int threshold, boolean validateDecompressed) {
        // Try to use Lattice native compression if enabled
        if (LatticeConfig.isPacketCompressionOptimizationEnabled()) {
            try {
                if (threshold >= 0) {
                    NativeCompressionManager.setCompressionThreshold(this.connection, threshold);
                } else {
                    ChannelPipeline pipeline = this.channel.pipeline();
                    if (pipeline.get("native-compress") != null) {
                        pipeline.remove("native-compress");
                    }
                    if (pipeline.get("native-decompress") != null) {
                        pipeline.remove("native-decompress");
                    }
                }
                this.channel.pipeline().fireUserEventTriggered(ConnectionEvent.COMPRESSION_THRESHOLD_SET);
                return;
            } catch (Throwable t) {
                LOGGER.warn("Failed to initialize Lattice native compression, falling back to vanilla", t);
            }
        }
        
        // Fallback to vanilla compression
        if (threshold >= 0) {
            // Paper - Use Velocity cipher
            com.velocitypowered.natives.compression.VelocityCompressor compressor = com.velocitypowered.natives.util.Natives.compress.get().create(io.papermc.paper.configuration.GlobalConfiguration.get().misc.compressionLevel.or(-1));
            if (this.channel.pipeline().get("decompress") instanceof io.netty.handler.codec.compression.Decoder compressionDecoder) {
                compressionDecoder.setThreshold(compressor, threshold, validateDecompressed); // Paper - Use Velocity cipher
            } else {
                this.channel.pipeline().addAfter("splitter", "decompress", new io.netty.handler.codec.compression.Decoder(compressor, threshold, validateDecompressed)); // Paper - Use Velocity cipher
            }

            if (this.channel.pipeline().get("compress") instanceof io.netty.handler.codec.compression.Encoder compressionEncoder) {
                compressionEncoder.setThreshold(threshold);
            } else {
                this.channel.pipeline().addAfter("prepender", "compress", new io.netty.handler.codec.compression.Encoder(compressor, threshold)); // Paper - Use Velocity cipher
            }
            this.channel.pipeline().fireUserEventTriggered(ConnectionEvent.COMPRESSION_THRESHOLD_SET); // Paper - Add Channel initialization listeners
        } else {
            if (this.channel.pipeline().get("decompress") instanceof io.netty.handler.codec.compression.Decoder) {
                this.channel.pipeline().remove("decompress");
            }

            if (this.channel.pipeline().get("compress") instanceof io.netty.handler.codec.compression.Encoder) {
                this.channel.pipeline().remove("compress");
            }
            this.channel.pipeline().fireUserEventTriggered(ConnectionEvent.COMPRESSION_DISABLED); // Paper - Add Channel initialization listeners
        }
    }
}