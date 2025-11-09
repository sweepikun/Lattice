package net.minecraft.world.level.pathfinder;

import io.lattice.world.NativePathfinder;
import io.lattice.world.MobPathfindingParams;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.pathfinder.Path;

public class Pathfinder {
    // Lattice - 寻路优化
    protected Path findPathOptimized(BlockPos startPos, BlockPos targetPos, Mob mob) {
        if (NativePathfinder.isNativeOptimizationAvailable()) {
            try {
                // 获取生物类型
                int mobType = getMobTypeForEntity(mob);
                
                // 使用原生寻路优化
                io.lattice.world.PathNode[] pathNodes = NativePathfinder.optimizePathfinding(
                    startPos.getX(), startPos.getY(), startPos.getZ(),
                    targetPos.getX(), targetPos.getY(), targetPos.getZ(), mobType);
                
                // 如果找到了路径，将其转换为Minecraft Path对象
                if (pathNodes != null && pathNodes.length > 0) {
                    // 创建Node数组
                    Node[] nodes = new Node[pathNodes.length];
                    for (int i = 0; i < pathNodes.length; i++) {
                        io.lattice.world.PathNode pathNode = pathNodes[i];
                        nodes[i] = new Node(pathNode.x, pathNode.y, pathNode.z);
                    }
                    
                    // 创建并返回Path对象
                    return new Path(nodes, targetPos, true);
                }
            } catch (Throwable t) {
                // 出现异常时回退到原始实现
                // 可以添加日志记录
            }
        }
        
        // 回退到原始实现
        return null;
    }
    
    // Lattice - 获取生物类型
    private int getMobTypeForEntity(Mob mob) {
        // 根据生物的类名确定生物类型
        String entityClassName = mob.getClass().getName();
        return NativePathfinder.getMobTypeForEntity(entityClassName);
    }
}