package io.lattice.redstone;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.atomic.AtomicLong;

/**
 * 红石性能监控器
 * 监控红石系统的性能指标，包括计算次数、缓存命中率、平均计算时间等
 */
public class RedstonePerformanceMonitor {
    private static final Logger LOGGER = LoggerFactory.getLogger(RedstonePerformanceMonitor.class);
    
    // 性能统计计数器
    private final AtomicLong totalCalculations = new AtomicLong(0);
    private final AtomicLong cacheHits = new AtomicLong(0);
    private final AtomicLong nativeCalculations = new AtomicLong(0);
    private final AtomicLong fallbackCalculations = new AtomicLong(0);
    private final AtomicLong totalCalculationTime = new AtomicLong(0);
    private final AtomicLong maxCalculationTime = new AtomicLong(0);
    
    // 缓存统计
    private final AtomicLong cacheSize = new AtomicLong(0);
    private final AtomicLong cacheEvictions = new AtomicLong(0);
    
    // 单例实例
    private static final RedstonePerformanceMonitor instance = new RedstonePerformanceMonitor();
    
    // 私有构造函数
    private RedstonePerformanceMonitor() {
        // 启动性能报告线程
        Thread reportThread = new Thread(() -> {
            while (!Thread.currentThread().isInterrupted()) {
                try {
                    Thread.sleep(5 * 60 * 1000); // 每5分钟
                    printPerformanceReport();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                } catch (Exception e) {
                    LOGGER.warn("红石性能监控线程异常", e);
                }
            }
        }, "RedstonePerformanceMonitor");
        reportThread.setDaemon(true);
        reportThread.start();
    }
    
    /**
     * 获取监控器实例
     * @return 实例
     */
    public static RedstonePerformanceMonitor getInstance() {
        return instance;
    }
    
    /**
     * 记录红石计算开始
     * @return 开始时间戳
     */
    public long startCalculation() {
        return System.nanoTime();
    }
    
    /**
     * 记录红石计算结束
     * @param startTime 开始时间戳
     * @param isCacheHit 是否缓存命中
     * @param isNative 是否使用原生计算
     */
    public void endCalculation(long startTime, boolean isCacheHit, boolean isNative) {
        long endTime = System.nanoTime();
        long calculationTime = endTime - startTime;
        
        // 更新统计计数器
        totalCalculations.incrementAndGet();
        if (isCacheHit) {
            cacheHits.incrementAndGet();
        }
        
        if (isNative) {
            nativeCalculations.incrementAndGet();
        } else {
            fallbackCalculations.incrementAndGet();
        }
        
        // 更新计算时间统计
        totalCalculationTime.addAndGet(calculationTime);
        long currentMax = maxCalculationTime.get();
        while (calculationTime > currentMax) {
            if (maxCalculationTime.compareAndSet(currentMax, calculationTime)) {
                break;
            }
            currentMax = maxCalculationTime.get();
        }
    }
    
    /**
     * 记录缓存大小变化
     * @param size 缓存大小
     */
    public void updateCacheSize(long size) {
        cacheSize.set(size);
    }
    
    /**
     * 记录缓存驱逐
     */
    public void recordCacheEviction() {
        cacheEvictions.incrementAndGet();
    }
    
    /**
     * 获取总计算次数
     * @return 总计算次数
     */
    public long getTotalCalculations() {
        return totalCalculations.get();
    }
    
    /**
     * 获取缓存命中次数
     * @return 缓存命中次数
     */
    public long getCacheHits() {
        return cacheHits.get();
    }
    
    /**
     * 获取缓存命中率
     * @return 缓存命中率(0-1)
     */
    public double getCacheHitRate() {
        long total = totalCalculations.get();
        if (total == 0) {
            return 0.0;
        }
        return (double) cacheHits.get() / total;
    }
    
    /**
     * 获取原生计算次数
     * @return 原生计算次数
     */
    public long getNativeCalculations() {
        return nativeCalculations.get();
    }
    
    /**
     * 获取回退计算次数
     * @return 回退计算次数
     */
    public long getFallbackCalculations() {
        return fallbackCalculations.get();
    }
    
    /**
     * 获取平均计算时间(纳秒)
     * @return 平均计算时间
     */
    public double getAverageCalculationTime() {
        long total = totalCalculations.get();
        if (total == 0) {
            return 0.0;
        }
        return (double) totalCalculationTime.get() / total;
    }
    
    /**
     * 获取最大计算时间(纳秒)
     * @return 最大计算时间
     */
    public long getMaxCalculationTime() {
        return maxCalculationTime.get();
    }
    
    /**
     * 获取缓存大小
     * @return 缓存大小
     */
    public long getCacheSize() {
        return cacheSize.get();
    }
    
    /**
     * 获取缓存驱逐次数
     * @return 缓存驱逐次数
     */
    public long getCacheEvictions() {
        return cacheEvictions.get();
    }
    
    /**
     * 打印性能统计报告
     */
    public void printPerformanceReport() {
        LOGGER.info("=== 红石系统性能报告 ===");
        LOGGER.info("总计算次数: {}", getTotalCalculations());
        LOGGER.info("缓存命中率: {:.2%}", getCacheHitRate());
        LOGGER.info("原生计算次数: {}", getNativeCalculations());
        LOGGER.info("回退计算次数: {}", getFallbackCalculations());
        LOGGER.info("平均计算时间: {:.2f} 纳秒", getAverageCalculationTime());
        LOGGER.info("最大计算时间: {} 纳秒", getMaxCalculationTime());
        LOGGER.info("当前缓存大小: {}", getCacheSize());
        LOGGER.info("缓存驱逐次数: {}", getCacheEvictions());
        LOGGER.info("========================");
    }
    
    /**
     * 重置性能统计
     */
    public void resetStatistics() {
        totalCalculations.set(0);
        cacheHits.set(0);
        nativeCalculations.set(0);
        fallbackCalculations.set(0);
        totalCalculationTime.set(0);
        maxCalculationTime.set(0);
        cacheEvictions.set(0);
    }
}