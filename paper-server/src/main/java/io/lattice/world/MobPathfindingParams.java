package io.lattice.world;

/**
 * 生物寻路参数类
 * 包含不同类型生物的寻路参数
 */
public class MobPathfindingParams {
    public float stepHeight;
    public float maxDropHeight;
    public boolean canSwim;
    public boolean canFly;
    public boolean avoidsWater;
    public boolean avoidsSun;
    public float speedFactor;
    
    public MobPathfindingParams() {
        // 默认构造函数
    }
    
    public MobPathfindingParams(float stepHeight, float maxDropHeight, boolean canSwim, 
                               boolean canFly, boolean avoidsWater, boolean avoidsSun, 
                               float speedFactor) {
        this.stepHeight = stepHeight;
        this.maxDropHeight = maxDropHeight;
        this.canSwim = canSwim;
        this.canFly = canFly;
        this.avoidsWater = avoidsWater;
        this.avoidsSun = avoidsSun;
        this.speedFactor = speedFactor;
    }
    
    @Override
    public String toString() {
        return "MobPathfindingParams{" +
                "stepHeight=" + stepHeight +
                ", maxDropHeight=" + maxDropHeight +
                ", canSwim=" + canSwim +
                ", canFly=" + canFly +
                ", avoidsWater=" + avoidsWater +
                ", avoidsSun=" + avoidsSun +
                ", speedFactor=" + speedFactor +
                '}';
    }
}