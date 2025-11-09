package io.lattice.world;

/**
 * 寻路节点类
 * 表示寻路路径中的一个点
 */
public class PathNode {
    public final int x;
    public final int y;
    public final int z;
    
    public PathNode(int x, int y, int z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }
    
    @Override
    public String toString() {
        return "PathNode{" +
                "x=" + x +
                ", y=" + y +
                ", z=" + z +
                '}';
    }
}