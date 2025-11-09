package io.lattice;

import io.lattice.nativeutil.LatticeNativeInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class LatticeServer {
    private static final Logger LOGGER = LoggerFactory.getLogger(LatticeServer.class);
    
    public static void main(String[] args) {
        // Initialize native libraries
        LOGGER.info("正在初始化Lattice本地库...");
        if (!LatticeNativeInitializer.initialize()) {
            LOGGER.error("无法加载Lattice本地库，某些功能可能无法正常工作");
        } else {
            LOGGER.info("Lattice本地库初始化成功");
        }
        
        // Start server initialization
        System.out.println("Initializing Lattice server...");
    }
}