#ifndef LATTICE_REDSTONE_COMPONENTS_HPP
#define LATTICE_REDSTONE_COMPONENTS_HPP

#include "redstone_engine.hpp"
#include <memory>
#include <vector>
#include <algorithm>
#include <tuple>

namespace lattice::redstone {

    // ========== 红石线组件 ==========
    
    class RedstoneWire : public RedstoneComponentBase {
    public:
        explicit RedstoneWire(Position pos) 
            : RedstoneComponentBase(pos, ComponentType::WIRE), 
              poweredNeighbors_(0), totalNeighbors_(0) {}
        
        int calculateOutput(int inputSignal) const override {
            // 红石线输出等于输入（无放大或延迟）
            return std::max(0, std::min(15, inputSignal));
        }
        
        bool canReceiveSignal(int signal) const override {
            return signal >= 0 && signal <= 15;
        }
        
        // 邻居管理
        void addNeighbor(const Position& neighborPos) {
            neighbors_.push_back(neighborPos);
            totalNeighbors_++;
        }
        
        void setPoweredNeighbors(int count) { poweredNeighbors_ = count; }
        int getPoweredNeighbors() const { return poweredNeighbors_; }
        int getTotalNeighbors() const { return totalNeighbors_; }
        
    private:
        std::vector<Position> neighbors_;
        int poweredNeighbors_;
        int totalNeighbors_;
    };

    // ========== 中继器组件 ==========
    
    class RedstoneRepeater : public RedstoneComponentBase {
    public:
        explicit RedstoneRepeater(Position pos, int delay = 1) 
            : RedstoneComponentBase(pos, ComponentType::REPEATER), 
              delay_(delay), locked_(false), frontSignal_(0) {}
        
        int calculateOutput(int inputSignal) const override {
            if (locked_) {
                return frontSignal_; // 被锁定时输出前面信号
            }
            
            // 中继器延迟输出，信号强度不变
            return std::max(0, std::min(15, inputSignal));
        }
        
        bool canReceiveSignal(int signal) const override {
            return signal > 0; // 只接收激活信号
        }
        
        void setDelay(int delay) { 
            delay_ = std::max(1, std::min(4, delay)); // 限制在1-4之间
        }
        
        int getDelay() const override { return delay_; }
        
        void setLocked(bool locked) { locked_ = locked; }
        void setFrontSignal(int signal) { frontSignal_ = signal; }
        
    private:
        int delay_;
        bool locked_;
        int frontSignal_;
    };

    // ========== 比较器组件 ==========
    
    class RedstoneComparator : public RedstoneComponentBase {
    public:
        enum class Mode : uint8_t {
            SUBTRACT,    // 减法模式
            COMPARE      // 比较模式
        };
        
        explicit RedstoneComparator(Position pos, Mode mode = Mode::SUBTRACT) 
            : RedstoneComponentBase(pos, ComponentType::COMPARATOR),
              mode_(mode), 
              frontSignal_(0),
              sideSignal_(0) {}
        
        int calculateOutput(int inputSignal) const override {
            switch (mode_) {
                case Mode::SUBTRACT:
                    // 减法模式：输出 = 前面信号 - 侧面信号
                    return std::max(0, std::min(15, frontSignal_ - sideSignal_));
                    
                case Mode::COMPARE:
                    // 比较模式：当前端信号 >= 侧面信号时输出前端信号，否则输出0
                    return (frontSignal_ >= sideSignal_) ? frontSignal_ : 0;
                    
                default:
                    return 0;
            }
        }
        
        bool canReceiveSignal(int signal) const override {
            return signal >= 0 && signal <= 15;
        }
        
        void setMode(Mode mode) { mode_ = mode; }
        void setFrontSignal(int signal) { frontSignal_ = signal; }
        void setSideSignal(int signal) { sideSignal_ = signal; }
        
    private:
        Mode mode_;
        int frontSignal_;
        int sideSignal_;
    };

    // ========== 红石火把组件 ==========
    
    class RedstoneTorch : public RedstoneComponentBase {
    public:
        explicit RedstoneTorch(Position pos, bool powered = false) 
            : RedstoneComponentBase(pos, ComponentType::TORCH), 
              powered_(powered) {}
        
        int calculateOutput(int inputSignal) const override {
            // 红石火把是反向器：无输入时输出15，有输入时输出0
            return powered_ ? 0 : 15;
        }
        
        bool canReceiveSignal(int signal) const override {
            return true; // 火把可以接收任何信号
        }
        
        void setPowered(bool powered) { powered_ = powered; }
        bool isPowered() const { return powered_; }
        
    private:
        bool powered_;
    };

    // ========== 拉杆组件 ==========
    
    class Lever : public RedstoneComponentBase {
    public:
        explicit Lever(Position pos, bool powered = false) 
            : RedstoneComponentBase(pos, ComponentType::LEVER), 
              powered_(powered) {}
        
        int calculateOutput(int inputSignal) const override {
            // 拉杆输出等于自身状态
            return powered_ ? 15 : 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            // 拉杆本身不接收外部信号（只由玩家操作）
            return false;
        }
        
        void toggle() { powered_ = !powered_; }
        void setPowered(bool powered) { powered_ = powered; }
        bool isPowered() const { return powered_; }
        
    private:
        bool powered_;
    };

    // ========== 按钮组件 ==========
    
    class Button : public RedstoneComponentBase {
    public:
        enum class Type : uint8_t {
            WOODEN,   // 木质按钮（15 tick）
            STONE     // 石头按钮（10 tick)
        };
        
        explicit Button(Position pos, Type type = Type::STONE) 
            : RedstoneComponentBase(pos, ComponentType::BUTTON),
              type_(type), 
              pressed_(false),
              pressTime_(0) {}
        
        int calculateOutput(int inputSignal) const override {
            return pressed_ ? 15 : 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            // 按钮本身不接收外部信号
            return false;
        }
        
        int getDelay() const override {
            return (type_ == Type::WOODEN) ? 15 : 10;
        }
        
        void press() { 
            pressed_ = true; 
            pressTime_ = RedstoneEngine::getInstance().getCurrentTick();
        }
        
        void release() { pressed_ = false; }
        bool isPressed() const { return pressed_; }
        Type getType() const { return type_; }
        
        // 检查是否应该自动释放
        bool shouldAutoRelease(uint64_t currentTick) const {
            if (!pressed_) return false;
            return (currentTick - pressTime_) >= getDelay();
        }
        
    private:
        Type type_;
        bool pressed_;
        uint64_t pressTime_;
    };

    // ========== 压力板组件 ==========
    
    class PressurePlate : public RedstoneComponentBase {
    public:
        enum class Type : uint8_t {
            WOODEN,     // 木质压力板
            STONE,      // 石头压力板
            GOLD,       // 黄金压力板
            IRON        // 铁压力板
        };
        
        explicit PressurePlate(Position pos, Type type = Type::STONE) 
            : RedstoneComponentBase(pos, ComponentType::PRESSURE_PLATE),
              type_(type), 
              entitiesOnPlate_(0) {}
        
        int calculateOutput(int inputSignal) const override {
            return entitiesOnPlate_ > 0 ? 15 : 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            return false; // 压力板不接收外部信号
        }
        
        void setEntityCount(int count) { entitiesOnPlate_ = std::max(0, count); }
        int getEntityCount() const { return entitiesOnPlate_; }
        Type getType() const { return type_; }
        
        // 获取阈值（不同压力板需要不同数量的实体激活）
        int getThreshold() const {
            switch (type_) {
                case Type::WOODEN:   return 1;
                case Type::STONE:    return 1;
                case Type::GOLD:     return 1;
                case Type::IRON:     return 1;
                default:            return 1;
            }
        }
        
        bool isActivated() const {
            return entitiesOnPlate_ >= getThreshold();
        }
        
    private:
        Type type_;
        int entitiesOnPlate_;
    };

    // ========== 观察者组件 ==========
    
    class Observer : public RedstoneComponentBase {
    public:
        enum class Facing : uint8_t {
            NORTH, SOUTH, EAST, WEST, UP, DOWN
        };
        
        explicit Observer(Position pos, Facing facing = Facing::SOUTH) 
            : RedstoneComponentBase(pos, ComponentType::OBSERVER),
              facing_(facing),
              lastState_(false),
              currentState_(false),
              pulseCount_(0) {}
        
        int calculateOutput(int inputSignal) const override {
            // 观察者输出脉冲（只输出1 tick）
            return currentState_ && !lastState_ ? 15 : 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            return true; // 观察者可以接收信号来检测方块更新
        }
        
        void updateState(bool newState) {
            lastState_ = currentState_;
            currentState_ = newState;
            
            // 检测到上升沿时增加脉冲计数
            if (currentState_ && !lastState_) {
                pulseCount_++;
            }
        }
        
        Facing getFacing() const { return facing_; }
        bool getCurrentState() const { return currentState_; }
        bool getLastState() const { return lastState_; }
        uint64_t getPulseCount() const { return pulseCount_; }
        bool isPulseActive() const { 
            return currentState_ && !lastState_; 
        }
        
    private:
        Facing facing_;
        bool lastState_;
        bool currentState_;
        uint64_t pulseCount_;
    };

    // ========== 活塞组件 ==========
    
    // 活塞方向枚举（与Observer的Facing区分）
    enum class PistonFacing : uint8_t {
        NORTH = 0,
        SOUTH = 1,
        EAST = 2,
        WEST = 3
    };
    
    class Piston : public RedstoneComponentBase {
    public:
        enum class Type : uint8_t {
            NORMAL,     // 普通活塞
            STICKY      // 黏性活塞
        };
        
        enum class State : uint8_t {
            RETRACTED,  // 收回状态
            EXTENDING,  // 伸出中
            EXTENDED,   // 伸出状态
            RETRACTING  // 收回中
        };
        
        explicit Piston(Position pos, Type type = Type::NORMAL, PistonFacing facing = PistonFacing::SOUTH) 
            : RedstoneComponentBase(pos, ComponentType::PISTON),
              type_(type),
              facing_(facing),
              state_(State::RETRACTED),
              extended_(false),
              targetBlockPos_(calculateTargetPosition()) {}
        
        int calculateOutput(int inputSignal) const override {
            // 活塞不输出红石信号
            return 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            return signal > 0;
        }
        
        void activate() {
            if (state_ == State::RETRACTED) {
                state_ = State::EXTENDING;
                extended_ = true;
            }
        }
        
        void deactivate() {
            if (state_ == State::EXTENDED) {
                state_ = State::RETRACTING;
                extended_ = false;
            }
        }
        
        Type getType() const { return type_; }
        PistonFacing getFacing() const { return facing_; }
        State getState() const { return state_; }
        bool isExtended() const { return extended_; }
        
        Position getTargetBlockPosition() const { return targetBlockPos_; }
        
        // 更新活塞状态（基于tick）
        void update(uint64_t currentTick) {
            switch (state_) {
                case State::EXTENDING:
                    // 伸出需要2 tick
                    if ((currentTick % 2) == 0) {
                        state_ = State::EXTENDED;
                    }
                    break;
                    
                case State::RETRACTING:
                    // 收回需要2 tick
                    if ((currentTick % 2) == 0) {
                        state_ = State::RETRACTED;
                    }
                    break;
                    
                default:
                    break;
            }
        }

    private:
        Type type_;
        Facing facing_;
        State state_;
        bool extended_;
        Position targetBlockPos_;
        
        Position calculateTargetPosition() const {
            // 根据朝向计算目标位置
            auto pos = getPosition();
            switch (facing_) {
                case PistonFacing::NORTH: return Position(pos.x, pos.y, pos.z - 1);
                case PistonFacing::SOUTH: return Position(pos.x, pos.y, pos.z + 1);
                case PistonFacing::EAST:  return Position(pos.x + 1, pos.y, pos.z);
                case PistonFacing::WEST:  return Position(pos.x - 1, pos.y, pos.z);
                default:           return pos;
            }
        }
    };

    // ========== 工厂函数 ==========
    
    namespace factory {
        
        std::unique_ptr<RedstoneComponentBase> createWire(Position pos) {
            return std::make_unique<RedstoneWire>(pos);
        }
        
        std::unique_ptr<RedstoneComponentBase> createRepeater(Position pos, int delay = 1) {
            return std::make_unique<RedstoneRepeater>(pos, delay);
        }
        
        std::unique_ptr<RedstoneComponentBase> createComparator(Position pos, 
            RedstoneComparator::Mode mode = RedstoneComparator::Mode::SUBTRACT) {
            return std::make_unique<RedstoneComparator>(pos, mode);
        }
        
        std::unique_ptr<RedstoneComponentBase> createTorch(Position pos, bool powered = false) {
            return std::make_unique<RedstoneTorch>(pos, powered);
        }
        
        std::unique_ptr<RedstoneComponentBase> createLever(Position pos, bool powered = false) {
            return std::make_unique<Lever>(pos, powered);
        }
        
        std::unique_ptr<RedstoneComponentBase> createButton(Position pos, 
            Button::Type type = Button::Type::STONE) {
            return std::make_unique<Button>(pos, type);
        }
        
        std::unique_ptr<RedstoneComponentBase> createPressurePlate(Position pos,
            PressurePlate::Type type = PressurePlate::Type::STONE) {
            return std::make_unique<PressurePlate>(pos, type);
        }
        
        std::unique_ptr<RedstoneComponentBase> createObserver(Position pos,
            Observer::Facing facing = Observer::Facing::SOUTH) {
            return std::make_unique<Observer>(pos, facing);
        }
        
        std::unique_ptr<RedstoneComponentBase> createPiston(Position pos,
            Piston::Type type = Piston::Type::NORMAL,
            PistonFacing facing = PistonFacing::SOUTH) {
            return std::make_unique<Piston>(pos, type, facing);
        }
        
    } // namespace factory

} // namespace lattice::redstone

#endif // LATTICE_REDSTONE_COMPONENTS_HPP