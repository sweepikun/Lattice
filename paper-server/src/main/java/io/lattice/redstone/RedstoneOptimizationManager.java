package io.lattice.redstone;

import io.lattice.config.LatticeConfig;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.RedstoneWireBlock;
import net.minecraft.world.level.block.state.BlockState;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.*;

/**
 * 红石优化管理器
 * 管理红石系统的优化逻辑，包括原生优化和缓存机制
 */
public class RedstoneOptimizationManager {
    private static final Logger LOGGER = LoggerFactory.getLogger(RedstoneOptimizationManager.class);
    
    // 单例实例
    private static RedstoneOptimizationManager instance;
    
    // 性能监控器
    private final RedstonePerformanceMonitor performanceMonitor;
    
    // 红石功率缓存
    private final Map<BlockPos, Integer> powerCache = new HashMap<>();
    private final Map<BlockPos, Long> cacheTimestamps = new HashMap<>();
    
    // 缓存过期时间（毫秒）
    private static final long CACHE_EXPIRY_TIME = 5000; // 5秒
    
    /**
     * 私有构造函数，防止外部实例化
     */
    private RedstoneOptimizationManager() {
        this.performanceMonitor = new RedstonePerformanceMonitor();
    }
    
    /**
     * 获取单例实例
     * @return RedstoneOptimizationManager实例
     */
    public static synchronized RedstoneOptimizationManager getInstance() {
        if (instance == null) {
            instance = new RedstoneOptimizationManager();
        }
        return instance;
    }
    
    /**
     * 获取指定位置的红石功率
     * @param level 世界对象
     * @param pos 位置
     * @return 红石功率值(0-15)
     */
    public int getRedstonePower(Level level, BlockPos pos) {
        long startTime = performanceMonitor.startCalculation();
        boolean isCacheHit = false;
        boolean isNative = false;
        
        try {
            // 检查是否启用原生红石优化
            if (NativeRedstoneOptimizer.isNativeOptimizationAvailable()) {
                try {
                    // 使用原生优化器计算红石功率
                    isNative = true;
                    int power = NativeRedstoneOptimizer.getRedstonePower(pos.getX(), pos.getY(), pos.getZ());
                    performanceMonitor.endCalculation(startTime, isCacheHit, isNative);
                    return power;
                } catch (Throwable t) {
                    LOGGER.warn("原生红石功率计算失败，回退到标准实现", t);
                }
            }
            
            // 回退到标准实现
            int power = getRedstonePowerFallback(level, pos);
            performanceMonitor.endCalculation(startTime, isCacheHit, isNative);
            return power;
        } catch (Exception e) {
            LOGGER.error("红石功率计算过程中发生错误", e);
            performanceMonitor.endCalculation(startTime, isCacheHit, isNative);
            return 0;
        }
    }
    
    /**
     * 计算红石网络功率
     * @param level 世界对象
     * @param pos 位置
     * @param maxDistance 最大计算距离
     * @return 网络功率值
     */
    public int calculateNetworkPower(Level level, BlockPos pos, int maxDistance) {
        long startTime = performanceMonitor.startCalculation();
        boolean isCacheHit = false;
        boolean isNative = false;
        
        try {
            // 检查是否启用原生红石优化
            if (NativeRedstoneOptimizer.isNativeOptimizationAvailable()) {
                try {
                    // 使用原生优化器计算网络功率
                    isNative = true;
                    int power = NativeRedstoneOptimizer.calculateNetworkPower(pos.getX(), pos.getY(), pos.getZ(), maxDistance);
                    performanceMonitor.endCalculation(startTime, isCacheHit, isNative);
                    return power;
                } catch (Throwable t) {
                    LOGGER.warn("原生红石网络功率计算失败，回退到标准实现", t);
                }
            }
            
            // 回退到标准实现
            int power = calculateNetworkPowerFallback(level, pos, maxDistance);
            performanceMonitor.endCalculation(startTime, isCacheHit, isNative);
            return power;
        } catch (Exception e) {
            LOGGER.error("红石网络功率计算过程中发生错误", e);
            performanceMonitor.endCalculation(startTime, isCacheHit, isNative);
            return 0;
        }
    }
    
    /**
     * 使指定位置的网络缓存失效
     * @param pos 位置
     */
    public void invalidateNetworkCache(BlockPos pos) {
        try {
            // 检查是否启用原生红石优化
            if (NativeRedstoneOptimizer.isNativeOptimizationAvailable()) {
                try {
                    // 使原生优化器的网络缓存失效
                    NativeRedstoneOptimizer.invalidateNetworkCache(pos.getX(), pos.getY(), pos.getZ());
                    return;
                } catch (Throwable t) {
                    LOGGER.warn("原生红石网络缓存失效失败", t);
                }
            }
            
            // 回退到标准实现
            invalidateNetworkCacheFallback(pos);
        } catch (Exception e) {
            LOGGER.error("红石网络缓存失效过程中发生错误", e);
        }
    }
    
    /**
     * 回退实现：获取指定位置的红石功率
     * @param level 世界对象
     * @param pos 位置
     * @return 红石功率值(0-15)
     */
    private int getRedstonePowerFallback(Level level, BlockPos pos) {
        // 检查缓存
        Integer cachedPower = getCachedPower(pos);
        if (cachedPower != null) {
            return cachedPower;
        }
        
        // 获取方块状态
        BlockState state = level.getBlockState(pos);
        Block block = state.getBlock();
        
        // 获取方块的红石功率
        int power = block.getSignal(level, pos, state, null);
        
        // 缓存结果
        cachePower(pos, power);
        
        return power;
    }
    
    /**
     * 回退实现：计算红石网络功率
     * @param level 世界对象
     * @param pos 位置
     * @param maxDistance 最大计算距离
     * @return 网络功率值
     */
    private int calculateNetworkPowerFallback(Level level, BlockPos pos, int maxDistance) {
        // 检查缓存
        Integer cachedPower = getCachedPower(pos);
        if (cachedPower != null) {
            return cachedPower;
        }
        
        // 使用广度优先搜索(BFS)算法计算红石网络功率
        Queue<BlockPos> queue = new LinkedList<>();
        Set<BlockPos> visited = new HashSet<>();
        Map<BlockPos, Integer> distances = new HashMap<>();
        
        queue.offer(pos);
        visited.add(pos);
        distances.put(pos, 0);
        
        int maxPower = 0;
        
        while (!queue.isEmpty()) {
            BlockPos currentPos = queue.poll();
            int currentDistance = distances.get(currentPos);
            
            // 如果超出最大距离，停止传播
            if (currentDistance > maxDistance) {
                continue;
            }
            
            // 获取当前位置的方块状态
            BlockState state = level.getBlockState(currentPos);
            
            // 如果是红石线，检查其功率
            if (state.getBlock() instanceof RedstoneWireBlock) {
                int power = state.getValue(RedstoneWireBlock.POWER);
                if (power > maxPower) {
                    maxPower = power;
                }
            }
            
            // 检查直接邻近方块的红石信号强度
            for (Direction direction : Direction.values()) {
                BlockPos neighborPos = currentPos.relative(direction);
                BlockState neighborState = level.getBlockState(neighborPos);
                Block neighborBlock = neighborState.getBlock();
                
                int signal = neighborBlock.getDirectSignal(level, neighborPos, state, direction);
                if (signal > maxPower) {
                    maxPower = signal;
                }
            }
            
            // 如果还有传播距离，将邻居加入队列
            if (currentDistance < maxDistance) {
                for (Direction direction : Direction.values()) {
                    BlockPos neighborPos = currentPos.relative(direction);
                    
                    // 检查是否已访问
                    if (!visited.contains(neighborPos)) {
                        visited.add(neighborPos);
                        queue.offer(neighborPos);
                        distances.put(neighborPos, currentDistance + 1);
                    }
                }
            }
        }
        
        // 缓存结果
        cachePower(pos, maxPower);
        
        return maxPower;
    }
    
    /**
     * 回退实现：使指定位置的网络缓存失效
     * @param pos 位置
     */
    private void invalidateNetworkCacheFallback(BlockPos pos) {
        // 清除指定位置的缓存
        powerCache.remove(pos);
        cacheTimestamps.remove(pos);
        
        // 清除邻居位置的缓存
        for (Direction direction : Direction.values()) {
            BlockPos neighborPos = pos.relative(direction);
            powerCache.remove(neighborPos);
            cacheTimestamps.remove(neighborPos);
        }
    }
    
    /**
     * 从缓存中获取红石功率
     * @param pos 位置
     * @return 红石功率值，如果缓存不存在或已过期则返回null
     */
    private Integer getCachedPower(BlockPos pos) {
        // 检查是否存在缓存
        if (!powerCache.containsKey(pos)) {
            return null;
        }
        
        // 检查缓存是否过期
        Long timestamp = cacheTimestamps.get(pos);
        if (timestamp == null || System.currentTimeMillis() - timestamp > CACHE_EXPIRY_TIME) {
            // 缓存已过期，移除缓存条目
            powerCache.remove(pos);
            cacheTimestamps.remove(pos);
            return null;
        }
        
        return powerCache.get(pos);
    }
    
    /**
     * 缓存红石功率
     * @param pos 位置
     * @param power 功率值
     */
    private void cachePower(BlockPos pos, int power) {
        powerCache.put(pos, power);
        cacheTimestamps.put(pos, System.currentTimeMillis());
        performanceMonitor.updateCacheSize(powerCache.size());
    }
    
    /**
     * 获取性能监控器
     * @return RedstonePerformanceMonitor实例
     */
    public RedstonePerformanceMonitor getPerformanceMonitor() {
        return performanceMonitor;
    }
    
    /**
     * 清理过期缓存
     */
    public void cleanupExpiredCache() {
        long currentTime = System.currentTimeMillis();
        Iterator<Map.Entry<BlockPos, Long>> iterator = cacheTimestamps.entrySet().iterator();
        
        int evictedCount = 0;
        while (iterator.hasNext()) {
            Map.Entry<BlockPos, Long> entry = iterator.next();
            if (currentTime - entry.getValue() > CACHE_EXPIRY_TIME) {
                iterator.remove();
                powerCache.remove(entry.getKey());
                evictedCount++;
            }
        }
        
        if (evictedCount > 0) {
            performanceMonitor.recordCacheEviction();
            performanceMonitor.updateCacheSize(powerCache.size());
        }
    }
}