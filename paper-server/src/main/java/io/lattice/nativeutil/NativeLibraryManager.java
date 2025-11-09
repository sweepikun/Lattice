package io.lattice.nativeutil;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.net.URL;
import java.io.InputStream;
import java.nio.file.Path;
import java.util.concurrent.locks.ReentrantLock;

/**
 * 统一管理native库的下载和加载
 */
public class NativeLibraryManager {
    private static final Logger LOGGER = LoggerFactory.getLogger(NativeLibraryManager.class);
    
    private static final String LIB_NAME = "lattice_native";
    private static boolean libraryLoaded = false;
    private static final ReentrantLock lock = new ReentrantLock();
    
    /**
     * 加载指定的native库
     * @param libraryName 库名
     * @return 是否加载成功
     */
    public static boolean loadLibrary(String libraryName) {
        lock.lock();
        try {
            if (libraryLoaded) {
                return true;
            }
            
            // 获取平台特定的库名
            String platformLibraryName = getPlatformLibraryName(libraryName);
            
            // 尝试加载本地库
            if (loadNativeLibrary(platformLibraryName)) {
                libraryLoaded = true;
                return true;
            }
            
            // 尝试下载并加载库
            try {
                String libPath = downloadAndLoadNativeLibrary(libraryName);
                if (libPath != null) {
                    System.load(libPath);
                    libraryLoaded = true;
                    return true;
                }
            } catch (Throwable ex) {
                LOGGER.error("无法下载并加载native库: {}", libraryName, ex);
            }
            
            return false;
        } finally {
            lock.unlock();
        }
    }
    
    /**
     * 获取平台特定的库名
     * @param baseName 基础库名
     * @return 平台特定的库名
     */
    private static String getPlatformLibraryName(String baseName) {
        String osName = System.getProperty("os.name").toLowerCase();
        String osArch = System.getProperty("os.arch").toLowerCase();
        
        // 规范化架构名称
        String arch;
        if (osArch.contains("amd64") || osArch.contains("x86_64")) {
            arch = "amd64";
        } else if (osArch.contains("aarch64") || osArch.contains("arm64")) {
            arch = "aarch64";
        } else {
            arch = osArch;
        }
        
        // 返回平台特定的库名
        if (osName.contains("win")) {
            return baseName + "-" + arch + "-windows";
        } else if (osName.contains("mac") || osName.contains("darwin")) {
            return "lib" + baseName + "-" + arch + "-macos";
        } else {
            return "lib" + baseName + "-" + arch + "-linux";
        }
    }
    
    /**
     * 加载本地库
     * @param libName 库名
     * @return 是否加载成功
     */
    private static boolean loadNativeLibrary(String libName) {
        try {
            System.loadLibrary(libName);
            LOGGER.info("成功从系统路径加载库: {}", libName);
            return true;
        } catch (UnsatisfiedLinkError e) {
            LOGGER.debug("无法从系统路径加载库: {}", libName, e);
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
            
            // 规范化架构名称
            String arch;
            if (osArch.contains("amd64") || osArch.contains("x86_64")) {
                arch = "amd64";
            } else if (osArch.contains("aarch64") || osArch.contains("arm64")) {
                arch = "aarch64";
            } else {
                arch = osArch;
            }
            
            if (osName.contains("win")) {
                libraryFileName = libName + "-" + arch + "-windows.dll";
                libraryExtension = "dll";
            } else if (osName.contains("mac") || osName.contains("darwin")) {
                libraryFileName = "lib" + libName + "-" + arch + "-macos.dylib";
                libraryExtension = "dylib";
            } else {
                libraryFileName = "lib" + libName + "-" + arch + "-linux.so";
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
                LOGGER.info("Native库已存在于: {}", libraryPath);
                return libraryPath.toString();
            }
            
            // 构建下载URL
            String downloadUrl = String.format(
                "https://github.com/LatticeTeam/Lattice/releases/latest/download/%s",
                libraryFileName);
            
            LOGGER.info("正在从 {} 下载native库", downloadUrl);
            
            // 下载文件
            try (InputStream in = new URL(downloadUrl).openStream()) {
                Files.copy(in, libraryPath, StandardCopyOption.REPLACE_EXISTING);
                LOGGER.info("Native库已下载到: {}", libraryPath);
                return libraryPath.toString();
            }
        } catch (Exception e) {
            LOGGER.error("下载native库失败", e);
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
     * 检查库是否已加载
     * @return 库是否已加载
     */
    public static boolean isLibraryLoaded() {
        return libraryLoaded;
    }
}