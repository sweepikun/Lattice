package io.lattice.nativeutil;

import io.lattice.config.LatticeConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Lattice本地库初始化器
 * 统一管理所有本地库的加载，确保只加载一次
 */
public class LatticeNativeInitializer {
    private static final Logger LOGGER = LoggerFactory.getLogger(LatticeNativeInitializer.class);
    
    // 根据不同平台选择对应的本地库名称
    private static final String LIB_NAME;
    
    static {
        String osName = System.getProperty("os.name").toLowerCase();
        if (osName.contains("linux")) {
            LIB_NAME = "lattice_native_linux";
        } else if (osName.contains("mac")) {
            LIB_NAME = "lattice_native_mac";
        } else if (osName.contains("windows")) {
            LIB_NAME = "lattice_native_windows";
        } else {
            LIB_NAME = "lattice_native";
        }
    }
    private static boolean initialized = false;
    private static boolean nativeLibraryLoaded = false;
    private static boolean nativeOptimizationAvailable = false;
    
    /**
     * 初始化所有本地库功能
     * @return 是否初始化成功
     */
    public static synchronized boolean initialize() {
        if (initialized) {
            return nativeLibraryLoaded;
        }
        
        try {
            // 尝试加载统一的本地库
            nativeLibraryLoaded = NativeLibraryManager.loadLibrary(LIB_NAME);
            
            if (nativeLibraryLoaded) {
                nativeOptimizationAvailable = true;
                LOGGER.info("成功加载Lattice本地库: {}", LIB_NAME);
            } else {
                LOGGER.warn("无法加载Lattice本地库: {}", LIB_NAME);
            }
        } catch (Throwable t) {
            LOGGER.error("加载Lattice本地库时发生错误", t);
            nativeLibraryLoaded = false;
            nativeOptimizationAvailable = false;
        }
        
        initialized = true;
        return nativeLibraryLoaded;
    }
    
    /**
     * 检查本地库是否已加载
     * @return 本地库是否已加载
     */
    public static boolean isNativeLibraryLoaded() {
        return nativeLibraryLoaded;
    }
    
    /**
     * 检查本地优化是否可用
     * @return 本地优化是否可用
     */
    public static boolean isNativeOptimizationAvailable() {
        return nativeOptimizationAvailable;
    }
    
    /**
     * 检查特定功能的本地优化是否可用
     * @param featureEnabled 配置中该功能是否启用
     * @return 该功能的本地优化是否可用
     */
    public static boolean isFeatureNativeOptimizationAvailable(boolean featureEnabled) {
        return nativeOptimizationAvailable && featureEnabled;
    }
}