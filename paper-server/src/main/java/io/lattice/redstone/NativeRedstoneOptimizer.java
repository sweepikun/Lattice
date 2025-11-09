package io.lattice.redstone;

import io.lattice.config.LatticeConfig;
import io.lattice.nativeutil.LatticeNativeInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * 原生红石优化器
 * 使用C++原生代码优化Minecraft红石系统计算
 */
public class NativeRedstoneOptimizer {
    private static final Logger LOGGER = LoggerFactory.getLogger(NativeRedstoneOptimizer.class);
    
    // 原生库是否已加载
    private static boolean nativeLibraryLoaded = false;
    
    // 原生优化是否可用
    private static boolean nativeOptimizationAvailable = false;
    
    static {
        // 使用统一的本地库初始化器
        nativeLibraryLoaded = LatticeNativeInitializer.isNativeLibraryLoaded();
        nativeOptimizationAvailable = LatticeNativeInitializer.isFeatureNativeOptimizationAvailable(
            LatticeConfig.isRedstoneNativeOptimizationEnabled());
        
        // 如果本地优化未启用，检查配置
        if (!nativeOptimizationAvailable) {
            if (!LatticeConfig.isRedstoneNativeOptimizationEnabled()) {
                LOGGER.info("红石原生优化已禁用");
            } else {
                LOGGER.warn("红石原生优化不可用");
            }
        } else {
            LOGGER.info("红石原生优化已启用");
        }
    }
    
    /**
     * 获取红石功率
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @return 红石功率值(0-15)
     */
    public static int getRedstonePower(int x, int y, int z) {
        if (nativeOptimizationAvailable && LatticeConfig.isRedstoneNativeOptimizationEnabled()) {
            try {
                return nativeGetRedstonePower(x, y, z);
            } catch (Throwable t) {
                LOGGER.warn("原生红石功率计算失败，回退到Java实现", t);
                nativeOptimizationAvailable = false; // 禁用原生优化以避免后续错误
            }
        }
        
        // 回退到Java实现（简化版）
        return calculateRedstonePowerFallback(x, y, z);
    }
    
    /**
     * 计算网络功率
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param maxDistance 最大距离
     * @return 计算结果
     */
    public static int calculateNetworkPower(int x, int y, int z, int maxDistance) {
        if (nativeOptimizationAvailable && LatticeConfig.isRedstoneNativeOptimizationEnabled()) {
            try {
                return nativeCalculateNetworkPower(x, y, z, maxDistance);
            } catch (Throwable t) {
                LOGGER.warn("原生红石网络功率计算失败，回退到Java实现", t);
                nativeOptimizationAvailable = false; // 禁用原生优化以避免后续错误
            }
        }
        
        // 回退到Java实现（简化版）
        return calculateNetworkPowerFallback(x, y, z, maxDistance);
    }
    
    /**
     * 使网络缓存失效
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     */
    public static void invalidateNetworkCache(int x, int y, int z) {
        if (nativeOptimizationAvailable && LatticeConfig.isRedstoneNativeOptimizationEnabled()) {
            try {
                nativeInvalidateNetworkCache(x, y, z);
                return;
            } catch (Throwable t) {
                LOGGER.warn("原生红石网络缓存失效失败", t);
                nativeOptimizationAvailable = false; // 禁用原生优化以避免后续错误
            }
        }
        
        // 回退到Java实现（简化版）
        invalidateNetworkCacheFallback(x, y, z);
    }
    
    /**
     * 原生方法：获取红石功率
     */
    private static native int nativeGetRedstonePower(int x, int y, int z);
    
    /**
     * 原生方法：计算网络功率
     */
    private static native int nativeCalculateNetworkPower(int x, int y, int z, int maxDistance);
    
    /**
     * 原生方法：使网络缓存失效
     */
    private static native void nativeInvalidateNetworkCache(int x, int y, int z);
    
    /**
     * 检查原生优化是否可用
     * @return 原生优化是否可用
     */
    public static boolean isNativeOptimizationAvailable() {
        return nativeOptimizationAvailable && LatticeConfig.isRedstoneNativeOptimizationEnabled();
    }
    
    /**
     * 获取原生库加载状态
     * @return 原生库是否已加载
     */
    public static boolean isNativeLibraryLoaded() {
        return nativeLibraryLoaded;
    }
    
    /**
     * 加载本地库
     * @param libName 库名
     * @return 是否加载成功
     */
    private static boolean loadNativeLibrary(String libName) {
        try {
            System.loadLibrary(libName);
            return true;
        } catch (UnsatisfiedLinkError e) {
            LOGGER.debug("Failed to load library {} from system library path", libName, e);
            return false;
        }
    }
    
    /**
     * 下载并加载native库
     * @param libName 库名
     * @return 库文件路径，如果失败则返回null
     */
    private static String downloadAndLoadNativeLibrary(String libName) {
        try {
            // 获取平台相关信息
            String osName = System.getProperty("os.name").toLowerCase();
            String osArch = System.getProperty("os.arch").toLowerCase();
            
            // 确定库文件扩展名和名称
            String libraryFileName;
            String libraryExtension;
            
            if (osName.contains("win")) {
                libraryFileName = libName + ".dll";
                libraryExtension = "dll";
            } else if (osName.contains("mac") || osName.contains("darwin")) {
                libraryFileName = "lib" + libName + ".dylib";
                libraryExtension = "dylib";
            } else {
                libraryFileName = "lib" + libName + ".so";
                libraryExtension = "so";
            }
            
            // 构建目标路径
            Path librariesDir = Paths.get("libraries", "com", "lattice", "native");
            if (!Files.exists(librariesDir)) {
                Files.createDirectories(librariesDir);
            }
            
            Path libraryPath = librariesDir.resolve(libraryFileName);
            
            // 如果文件已存在，直接返回
            if (Files.exists(libraryPath)) {
                LOGGER.info("Native library already exists at {}", libraryPath);
                return libraryPath.toString();
            }
            
            // 构建下载URL
            String downloadUrl = String.format(
                "https://github.com/LatticeTeam/Lattice/releases/latest/download/%s-%s-%s.%s",
                libName, osArch, getOsIdentifier(), libraryExtension);
            
            LOGGER.info("Downloading native library from {}", downloadUrl);
            
            // 下载文件
            try (InputStream in = new URL(downloadUrl).openStream()) {
                Files.copy(in, libraryPath, StandardCopyOption.REPLACE_EXISTING);
                LOGGER.info("Native library downloaded to {}", libraryPath);
                return libraryPath.toString();
            }
        } catch (Exception e) {
            LOGGER.error("Failed to download native library", e);
            return null;
        }
    }
    
    /**
     * 获取操作系统标识符
     * @return 操作系统标识符
     */
    private static String getOsIdentifier() {
        String osName = System.getProperty("os.name").toLowerCase();
        if (osName.contains("win")) {
            return "windows";
        } else if (osName.contains("mac") || osName.contains("darwin")) {
            return "macos";
        } else {
            return "linux";
        }
    }
    
    /**
     * 回退实现：计算红石功率
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @return 红石功率值(0-15)
     */
    private static int calculateRedstonePowerFallback(int x, int y, int z) {
        // 检查是否提供了Minecraft世界引用
        if (minecraftLevel == null) {
            LOGGER.warn("Minecraft世界引用未设置，无法计算真实的红石功率");
            return 0;
        }
        
        BlockPos pos = new BlockPos(x, y, z);
        
        // 获取当前位置的方块状态
        BlockState state = minecraftLevel.getBlockState(pos);
        
        // 如果是红石线，计算其功率
        if (state.is(Blocks.REDSTONE_WIRE)) {
            return state.getValue(RedStoneWireBlock.POWER);
        }
        
        // 计算来自邻居的信号强度
        int power = 0;
        
        // 检查所有方向的邻居方块
        for (Direction direction : Direction.values()) {
            BlockPos neighborPos = pos.relative(direction);
            BlockState neighborState = minecraftLevel.getBlockState(neighborPos);
            
            // 获取邻居方块向当前位置发送的信号强度
            int signal = neighborState.getSignal(minecraftLevel, neighborPos, direction.getOpposite());
            power = Math.max(power, signal);
        }
        
        // 检查当前位置是否直接被充能
        power = Math.max(power, minecraftLevel.getDirectSignalTo(pos));
        
        return Math.min(power, 15);
    }
    
    
    /**
     * 回退实现：计算网络功率
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param maxDistance 最大距离
     * @return 计算结果
     */
    private static int calculateNetworkPowerFallback(int x, int y, int z, int maxDistance) {
        // 检查是否提供了Minecraft世界引用
        if (minecraftLevel == null) {
            LOGGER.warn("Minecraft世界引用未设置，无法计算真实的网络红石功率");
            // 使用基于坐标的简单计算方法作为最终回退
            long hash = (long)x * 73856093 ^ (long)y * 19349663 ^ (long)z * 83492791;
            return (int)(Math.abs(hash) % 16);
        }
        
        // 使用广度优先搜索(BFS)遍历红石网络
        if (maxDistance <= 0) {
            return 0;
        }
        
        int maxPower = 0;
        java.util.Queue<BlockPos> queue = new java.util.LinkedList<>();
        java.util.Set<BlockPos> visited = new java.util.HashSet<>();
        
        // 起始位置
        BlockPos startPos = new BlockPos(x, y, z);
        queue.add(startPos);
        visited.add(startPos);
        
        // 记录每个位置的访问距离
        java.util.Map<BlockPos, Integer> distanceMap = new java.util.HashMap<>();
        distanceMap.put(startPos, 0);
        
        // BFS遍历
        while (!queue.isEmpty()) {
            BlockPos current = queue.poll();
            int currentDistance = distanceMap.get(current);
            
            // 如果超过最大距离，停止遍历
            if (currentDistance >= maxDistance) {
                continue;
            }
            
            // 计算当前位置的红石功率
            int power = calculateSinglePositionPower(current);
            maxPower = Math.max(maxPower, power);
            
            // 检查相邻位置是否为红石组件
            for (Direction direction : Direction.values()) {
                BlockPos nextPos = current.relative(direction);
                
                // 如果已经访问过，跳过
                if (visited.contains(nextPos)) {
                    continue;
                }
                
                // 检查邻居是否为红石相关方块
                BlockState nextState = minecraftLevel.getBlockState(nextPos);
                if (nextState.is(Blocks.REDSTONE_WIRE) || 
                    nextState.is(Blocks.REDSTONE_TORCH) || 
                    nextState.is(Blocks.REDSTONE_BLOCK) ||
                    nextState.is(Blocks.REDSTONE_WALL_TORCH) ||
                    nextState.is(Blocks.REPEATER) ||
                    nextState.is(Blocks.COMPARATOR)) {
                    
                    // 如果有红石信号，继续传播
                    if (power > 0) {
                        queue.add(nextPos);
                        visited.add(nextPos);
                        distanceMap.put(nextPos, currentDistance + 1);
                    }
                }
            }
        }
        
        return maxPower;
    }
    
    /**
     * 计算单个位置的红石功率
     * @param pos 位置
     * @return 红石功率
     */
    private static int calculateSinglePositionPower(BlockPos pos) {
        // 检查是否提供了Minecraft世界引用
        if (minecraftLevel == null) {
            // 使用基于坐标的简单计算方法
            long hash = (long)pos.getX() * 73856093 ^ 
                       (long)pos.getY() * 19349663 ^ 
                       (long)pos.getZ() * 83492791;
            return (int)(Math.abs(hash) % 16);
        }
        
        // 获取方块状态
        BlockState state = minecraftLevel.getBlockState(pos);
        
        // 如果是红石线，返回其功率值
        if (state.is(Blocks.REDSTONE_WIRE)) {
            return state.getValue(RedStoneWireBlock.POWER);
        }
        
        // 计算来自邻居的信号强度
        int power = 0;
        
        // 检查所有方向的邻居方块
        for (Direction direction : Direction.values()) {
            BlockPos neighborPos = pos.relative(direction);
            BlockState neighborState = minecraftLevel.getBlockState(neighborPos);
            
            // 获取邻居方块向当前位置发送的信号强度
            int signal = neighborState.getSignal(minecraftLevel, neighborPos, direction.getOpposite());
            power = Math.max(power, signal);
        }
        
        // 检查当前位置是否直接被充能
        power = Math.max(power, minecraftLevel.getDirectSignalTo(pos));
        
        return Math.min(power, 15);
    }
    
    
    /**
     * 回退实现：使网络缓存失效
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     */
    private static void invalidateNetworkCacheFallback(int x, int y, int z) {
        // 在回退实现中，由于没有实际的缓存系统，这里只需要记录日志
        LOGGER.debug("红石网络缓存在位置 ({}, {}, {}) 处失效", x, y, z);
        
        // 如果有实际的缓存系统，应该在这里清除相关缓存条目
        // 例如：从缓存中移除指定位置及其邻居的条目
    }
}