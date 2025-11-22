#include "simple_redstone_engine.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace lattice::redstone;

// 启用调试输出
#define DEBUG_REDSTONE

void testBasicSignalPropagation() {
    std::cout << "\n=== 测试基本信号传播 ===" << std::endl;
    
    auto& engine = RedstoneEngine::getInstance();
    
    // 创建一个拉杆
    auto lever = std::make_unique<Lever>(Position(0, 0, 0), false);
    engine.registerComponent(std::move(lever));
    
    // 创建红石线
    auto wire1 = std::make_unique<RedstoneWire>(Position(1, 0, 0));
    auto wire2 = std::make_unique<RedstoneWire>(Position(2, 0, 0));
    engine.registerComponent(std::move(wire1));
    engine.registerComponent(std::move(wire2));
    
    std::cout << "初始状态: 拉杆关闭 (0,0,0) -> 信号 0" << std::endl;
    engine.tick();
    
    // 模拟拉杆激活
    std::cout << "\n拉杆激活，测试applySignal..." << std::endl;
    engine.applySignal(Position(0, 0, 0), 15); // 拉杆应该输出15
    
    // 运行几个tick来观察信号传播
    for (int i = 0; i < 3; i++) {
        std::cout << "Tick " << i << ":" << std::endl;
        engine.tick();
    }
}

void testComponentTypes() {
    std::cout << "\n=== 测试不同组件类型 ===" << std::endl;
    
    auto& engine = RedstoneEngine::getInstance();
    
    // 创建按钮
    auto button = std::make_unique<Button>(Position(5, 0, 0), false);
    auto* buttonPtr = button.get();
    engine.registerComponent(std::move(button));
    
    // 创建红石火把
    auto torch = std::make_unique<RedstoneTorch>(Position(6, 0, 0), false);
    engine.registerComponent(std::move(torch));
    
    std::cout << "测试按钮按压..." << std::endl;
    buttonPtr->press();
    engine.applySignal(Position(5, 0, 0), 15);
    
    std::cout << "\n测试红石火把..." << std::endl;
    engine.applySignal(Position(6, 0, 0), 0); // 无输入时火把应该输出15
    
    engine.tick();
}

void testPerformanceMonitoring() {
    std::cout << "\n=== 测试性能监控 ===" << std::endl;
    
    auto& engine = RedstoneEngine::getInstance();
    
    // 重置统计
    engine.resetPerformanceStats();
    
    // 创建组件进行测试
    for (int x = 0; x < 5; x++) {
        for (int z = 0; z < 5; z++) {
            if (x == 0 && z == 0) continue; // 跳过原点
            
            auto wire = std::make_unique<RedstoneWire>(Position(x, 0, z));
            engine.registerComponent(std::move(wire));
        }
    }
    
    // 运行多次tick
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        engine.tick();
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // 获取性能统计
    auto stats = engine.getPerformanceStats();
    
    std::cout << "性能统计:" << std::endl;
    std::cout << "  总组件数: " << stats.totalComponents << std::endl;
    std::cout << "  处理信号数: " << stats.signalsProcessed << std::endl;
    std::cout << "  电路tick数: " << stats.circuitTicks << std::endl;
    std::cout << "  平均处理时间: " << stats.avgProcessingTimeMs << " ms" << std::endl;
    std::cout << "  内存使用: " << stats.memoryUsageBytes << " bytes" << std::endl;
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    std::cout << "  总执行时间: " << duration.count() << " ms" << std::endl;
    std::cout << "  当前tick: " << engine.getCurrentTick() << std::endl;
}

int main() {
    std::cout << "简化红石引擎功能测试" << std::endl;
    std::cout << "===================" << std::endl;
    
    try {
        testBasicSignalPropagation();
        testComponentTypes();
        testPerformanceMonitoring();
        
        std::cout << "\n✅ 所有测试完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}