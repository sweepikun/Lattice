#include "redstone_engine.hpp"
#include "redstone_components.hpp"
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
    auto lever = factory::createLever(Position(0, 0, 0), false);
    engine.registerComponent(std::move(lever));
    
    // 创建一个红石线
    auto wire1 = factory::createWire(Position(1, 0, 0));
    auto wire2 = factory::createWire(Position(2, 0, 0));
    engine.registerComponent(std::move(wire1));
    engine.registerComponent(std::move(wire2));
    
    std::cout << "初始状态: 拉杆关闭 (0,0,0) -> 信号 0" << std::endl;
    engine.tick();
    
    // 模拟拉杆激活
    std::cout << "\n拉杆激活，测试applySignal..." << std::endl;
    engine.applySignal(Position(0, 0, 0), 15); // 拉杆应该输出15
    
    // 运行几个tick来观察信号传播
    for (int i = 0; i < 5; i++) {
        std::cout << "Tick " << i << ":" << std::endl;
        engine.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void testComponentFactory() {
    std::cout << "\n=== 测试组件工厂 ===" << std::endl;
    
    auto& engine = RedstoneEngine::getInstance();
    
    // 创建不同类型的组件
    auto repeater = factory::createRepeater(Position(5, 0, 0), 2); // 2 tick延迟
    auto comparator = factory::createComparator(Position(6, 0, 0), 
                                              RedstoneComparator::Mode::COMPARE);
    auto torch = factory::createTorch(Position(7, 0, 0), false);
    auto button = factory::createButton(Position(8, 0, 0), Button::Type::STONE);
    
    engine.registerComponent(std::move(repeater));
    engine.registerComponent(std::move(comparator));
    engine.registerComponent(std::move(torch));
    engine.registerComponent(std::move(button));
    
    std::cout << "创建了中继器、比较器、火把和按钮组件" << std::endl;
    
    // 测试按钮按压
    std::cout << "\n测试按钮按压..." << std::endl;
    auto buttonComp = static_cast<Button*>(engine.queryComponentsInRange(Position(8, 0, 0), 1)[0]);
    if (buttonComp) {
        buttonComp->press();
        engine.applySignal(Position(8, 0, 0), 15);
    }
    
    // 测试火把
    std::cout << "\n测试红石火把..." << std::endl;
    engine.applySignal(Position(7, 0, 0), 0); // 无输入时火把应该输出15
    
    engine.tick();
}

void testPerformanceMonitoring() {
    std::cout << "\n=== 测试性能监控 ===" << std::endl;
    
    auto& engine = RedstoneEngine::getInstance();
    
    // 重置统计
    engine.resetPerformanceStats();
    
    // 创建大量组件进行压力测试
    for (int x = 0; x < 10; x++) {
        for (int z = 0; z < 10; z++) {
            if (x == 0 && z == 0) continue; // 跳过原点
            
            auto wire = factory::createWire(Position(x, 0, z));
            engine.registerComponent(std::move(wire));
        }
    }
    
    // 运行多次tick
    auto startTime = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
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
}

void testRangeQueries() {
    std::cout << "\n=== 测试范围查询 ===" << std::endl;
    
    auto& engine = RedstoneEngine::getInstance();
    
    // 在原点周围创建组件
    engine.registerComponent(factory::createWire(Position(1, 0, 0)));
    engine.registerComponent(factory::createWire(Position(-1, 0, 0)));
    engine.registerComponent(factory::createWire(Position(0, 0, 1)));
    engine.registerComponent(factory::createWire(Position(0, 0, -1)));
    engine.registerComponent(factory::createRepeater(Position(2, 0, 0)));
    
    // 范围查询测试
    auto nearby = engine.queryComponentsInRange(Position(0, 0, 0), 2);
    std::cout << "在原点(0,0,0)周围2格内找到 " << nearby.size() << " 个组件" << std::endl;
    
    for (const auto* component : nearby) {
        auto pos = component->getPosition();
        std::cout << "  组件在位置 (" << pos.x << "," << pos.y << "," << pos.z 
                  << "), 类型: " << static_cast<int>(component->type) << std::endl;
    }
}

int main() {
    std::cout << "红石引擎完整功能测试" << std::endl;
    std::cout << "===================" << std::endl;
    
    try {
        testBasicSignalPropagation();
        testComponentFactory();
        testPerformanceMonitoring();
        testRangeQueries();
        
        std::cout << "\n✅ 所有测试完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}