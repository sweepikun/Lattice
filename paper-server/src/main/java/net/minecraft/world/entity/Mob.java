
    /**
     * Lattice - 优化的寻路方法
     */
    protected Path createOptimizedPath(Level level, BlockPos targetPos) {
        // 使用原生寻路优化
        Pathfinder pathfinder = this.getNavigation().getPathFinder();
        if (pathfinder != null) {
            // 调用优化的寻路方法
            return pathfinder.findPathOptimized(this.blockPosition(), targetPos, this);
        }
        
        // 回退到标准实现
        return null;
    }
    
    /**
     * Lattice - 重写寻路方法以使用优化版本
     */
    @Override
    public boolean pathfindToTarget(BlockPos targetPos) {
        Path path = this.createOptimizedPath(this.level, targetPos);
        if (path != null) {
            return this.getNavigation().moveTo(path, 1.0D);
        }
        
        // 回退到标准实现
        return super.pathfindToTarget(targetPos);
    }
