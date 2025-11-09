package io.lattice.config;

import org.bukkit.configuration.file.YamlConfiguration;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;

/**
 * Lightweight lattice configuration loader.
 * - Loads config/lattice.yml
 * - Provides compression/decompression level accessors
 * - Auto-creates a default config if missing
 */
public final class LatticeConfig {
    private static final String FILE_NAME = "lattice.yml";
    private static volatile YamlConfiguration cfg;

    // Defaults
    private static volatile int compressionLevel = 6; // zlib default
    private static volatile int decompressionLevel = 6; // informational; zlib decompression does not use level but admin can set for policies
    
    // Optimization flags
    private static volatile boolean packetCompressionOptimization = true;
    private static volatile boolean lightUpdateOptimization = true;
    private static volatile boolean chunkSerializationOptimization = true;
    
    // Redstone system optimization configuration
    private static volatile boolean redstoneOptimizationEnabled = true;
    private static volatile boolean redstoneCachingEnabled = true;
    private static volatile int redstoneCacheSize = 10000;
    private static volatile boolean redstoneNetworkOptimization = true;
    private static volatile boolean redstoneNativeOptimization = true; // New field for redstone native optimization

    // Pathfinding system optimization configuration
    private static volatile boolean pathfindingOptimizationEnabled = true;
    private static volatile boolean pathfindingCachingEnabled = true;
    private static volatile int pathfindingCacheSize = 1000;
    private static volatile boolean pathfindingNativeOptimization = true; // New field for pathfinding native optimization

    // Network compression optimization configuration
    private static volatile boolean asyncCompressionEnabled = true;
    private static volatile int asyncCompressionThreads = -1; // -1 means auto-detect
    private static volatile boolean dynamicCompressionEnabled = true;
    private static volatile int minCompressionLevel = 1;
    private static volatile int maxCompressionLevel = 9;
    private static volatile double compressionSensitivity = 0.5;

    private LatticeConfig() {}

    // Auto-load from default config directory on class load (config/lattice.yml)
    static {
        try {
            load(Path.of("config"));
        } catch (Exception ignored) {
            // fallthrough: defaults will be used
        }
    }

    /**
     * Load lattice.yml from the given configuration directory.
     * Creates a default file if one does not exist.
     */
    public static synchronized void load(Path configDir) {
        try {
            if (!Files.exists(configDir)) {
                Files.createDirectories(configDir);
            }
            Path cfgPath = configDir.resolve(FILE_NAME);
            if (!Files.exists(cfgPath)) {
                // create default
                String def = "# Lattice configuration for compression/decompression levels\n" +
                        "# compression-level: zlib compression level used when compressing (1-9, default 6)\n" +
                        "# decompression-level: informational, for server policies (default 6)\n" +
                        "compression-level: 6\n" +
                        "decompression-level: 6\n" +
                        "\n" +
                        "# Lattice performance optimizations\n" +
                        "# packet-compression-optimization: Use native libdeflate for packet compression (default true)\n" +
                        "# light-update-optimization: Use native implementation for light updates (default true)\n" +
                        "# chunk-serialization-optimization: Use native libdeflate for chunk serialization (default true)\n" +
                        "packet-compression-optimization: true\n" +
                        "light-update-optimization: true\n" +
                        "chunk-serialization-optimization: true\n" +
                        "\n" +
                        "# Network compression optimization configuration\n" +
                        "network:\n" +
                        "  # Asynchronous compression settings\n" +
                        "  async-compression:\n" +
                        "    enabled: true # Enable asynchronous compression\n" +
                        "    threads: -1   # Number of compression threads (-1 for auto-detect based on CPU cores)\n" +
                        "  \n" +
                        "  # Dynamic compression settings\n" +
                        "  compression:\n" +
                        "    dynamic: true     # Enable dynamic compression level adjustment\n" +
                        "    level: 6          # Base compression level (1-9)\n" +
                        "    min_level: 3      # Minimum compression level for dynamic adjustment\n" +
                        "    max_level: 8      # Maximum compression level for dynamic adjustment\n" +
                        "    sensitivity: 0.7  # Sensitivity of dynamic adjustment (0.0 - 1.0)\n" +
                        "\n" +
                        "# Redstone system optimization configuration\n" +
                        "redstone-optimization:\n" +
                        "  enabled: true # Enable redstone system optimization\n" +
                        "  caching-enabled: true # Enable redstone cache mechanism\n" +
                        "  cache-size: 10000 # Size of redstone cache\n" +
                        "  network-optimization: true # Enable redstone network optimization\n" +
                        "  native-optimization: true # Enable redstone native optimization\n" +
                        "\n" +
                        "# Pathfinding system optimization configuration\n" +
                        "pathfinding-optimization:\n" +
                        "  enabled: true # Enable pathfinding system optimization\n" +
                        "  caching-enabled: true # Enable pathfinding cache mechanism\n" +
                        "  cache-size: 1000 # Size of pathfinding cache\n" +
                        "  native-optimization: true # Enable pathfinding native optimization\n";
                Files.writeString(cfgPath, def, StandardCharsets.UTF_8);
            }

            File file = cfgPath.toFile();
            cfg = YamlConfiguration.loadConfiguration(file);

            compressionLevel = cfg.getInt("compression-level", compressionLevel);
            decompressionLevel = cfg.getInt("decompression-level", decompressionLevel);
            // clamp
            if (compressionLevel < 1) compressionLevel = 1;
            if (compressionLevel > 9) compressionLevel = 9;
            
            // Load optimization flags
            packetCompressionOptimization = cfg.getBoolean("packet-compression-optimization", packetCompressionOptimization);
            lightUpdateOptimization = cfg.getBoolean("light-update-optimization", lightUpdateOptimization);
            chunkSerializationOptimization = cfg.getBoolean("chunk-serialization-optimization", chunkSerializationOptimization);
            
            // Load network compression optimization configuration
            asyncCompressionEnabled = cfg.getBoolean("network.async-compression.enabled", asyncCompressionEnabled);
            asyncCompressionThreads = cfg.getInt("network.async-compression.threads", asyncCompressionThreads);
            dynamicCompressionEnabled = cfg.getBoolean("network.compression.dynamic", dynamicCompressionEnabled);
            minCompressionLevel = cfg.getInt("network.compression.min_level", minCompressionLevel);
            maxCompressionLevel = cfg.getInt("network.compression.max_level", maxCompressionLevel);
            compressionSensitivity = cfg.getDouble("network.compression.sensitivity", compressionSensitivity);
            
            // Load redstone optimization configuration
            redstoneOptimizationEnabled = cfg.getBoolean("redstone-optimization.enabled", redstoneOptimizationEnabled);
            redstoneCachingEnabled = cfg.getBoolean("redstone-optimization.caching-enabled", redstoneCachingEnabled);
            redstoneCacheSize = cfg.getInt("redstone-optimization.cache-size", redstoneCacheSize);
            redstoneNetworkOptimization = cfg.getBoolean("redstone-optimization.network-optimization", redstoneNetworkOptimization);
            redstoneNativeOptimization = cfg.getBoolean("redstone-optimization.native-optimization", redstoneNativeOptimization); // Load new native optimization setting

            // Load pathfinding optimization configuration
            pathfindingOptimizationEnabled = cfg.getBoolean("pathfinding-optimization.enabled", pathfindingOptimizationEnabled);
            pathfindingCachingEnabled = cfg.getBoolean("pathfinding-optimization.caching-enabled", pathfindingCachingEnabled);
            pathfindingCacheSize = cfg.getInt("pathfinding-optimization.cache-size", pathfindingCacheSize);
            pathfindingNativeOptimization = cfg.getBoolean("pathfinding-optimization.native-optimization", pathfindingNativeOptimization); // Load new native optimization setting
        } catch (IOException e) {
            // leave defaults
        }
    }

    public static synchronized void reload(Path configDir) {
        load(configDir);
    }

    public static int getCompressionLevel() {
        return compressionLevel;
    }
    
    public static int getDecompressionLevel() {
        return decompressionLevel;
    }
    
    // 新增异步压缩和动态压缩配置
    public static int getMinCompressionLevel() {
        return minCompressionLevel;
    }
    
    public static int getMaxCompressionLevel() {
        return maxCompressionLevel;
    }
    
    public static double getCompressionSensitivity() {
        return compressionSensitivity;
    }
    
    public static boolean isDynamicCompressionEnabled() {
        return dynamicCompressionEnabled;
    }
    
    public static boolean isNetworkCompressionOptimizationEnabled() {
        return packetCompressionOptimization;
    }

    public static boolean isPacketCompressionOptimizationEnabled() {
        return packetCompressionOptimization;
    }

    public static boolean isLightUpdateOptimizationEnabled() {
        return lightUpdateOptimization;
    }

    public static boolean isChunkSerializationOptimizationEnabled() {
        return chunkSerializationOptimization;
    }
    
    public static boolean isRedstoneOptimizationEnabled() {
        return redstoneOptimizationEnabled;
    }
    
    public static boolean isRedstoneCachingEnabled() {
        return redstoneCachingEnabled && redstoneOptimizationEnabled;
    }
    
    public static int getRedstoneCacheSize() {
        return redstoneCacheSize;
    }
    
    public static boolean isRedstoneNetworkOptimizationEnabled() {
        return redstoneNetworkOptimization && redstoneOptimizationEnabled;
    }

    /**
     * Check if redstone native optimization is enabled.
     * Returns false if redstone optimization is not enabled.
     */
    public static boolean isRedstoneNativeOptimizationEnabled() {
        return redstoneNativeOptimization && redstoneOptimizationEnabled;
    }

    /**
     * Check if pathfinding optimization is enabled.
     */
    public static boolean isPathfindingOptimizationEnabled() {
        return pathfindingOptimizationEnabled;
    }

    /**
     * Check if pathfinding caching is enabled.
     * Returns false if pathfinding optimization is not enabled.
     */
    public static boolean isPathfindingCachingEnabled() {
        return pathfindingCachingEnabled && pathfindingOptimizationEnabled;
    }

    /**
     * Get the size of the pathfinding cache.
     */
    public static int getPathfindingCacheSize() {
        return pathfindingCacheSize;
    }

    /**
     * Check if pathfinding native optimization is enabled.
     * Returns false if pathfinding optimization is not enabled.
     */
    public static boolean isPathfindingNativeOptimizationEnabled() {
        return pathfindingNativeOptimization && pathfindingOptimizationEnabled;
    }

    /**
     * Check if asynchronous compression is enabled.
     */
    public static boolean isAsyncCompressionEnabled() {
        return asyncCompressionEnabled;
    }

    /**
     * Get the number of asynchronous compression threads.
     * Returns -1 for auto-detection based on CPU cores.
     */
    public static int getAsyncCompressionThreads() {
        return asyncCompressionThreads;
    }

    public static YamlConfiguration getRaw() {
        return cfg;
    }
}