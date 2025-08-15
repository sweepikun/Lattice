package io.lattice.performance;

public class NativeInterface {
    static {
        System.loadLibrary("lattice_native");
    }

    /**
     * 初始化本地线程池和任务调度器
     * @param threadCount 线程池中的线程数量
     * @return 是否成功初始化
     */
    public static native boolean initializeThreadPool(int threadCount);

    /**
     * 提交任务到本地线程池
     * @param taskId 任务ID
     * @param priority 优先级 (0-100)
     * @param data 任务数据
     * @return 任务句柄
     */
    public static native long submitTask(int taskId, int priority, byte[] data);

    /**
     * 检查任务是否完成
     * @param handle 任务句柄
     * @return 任务是否完成
     */
    public static native boolean isTaskComplete(long handle);

    /**
     * 获取任务结果
     * @param handle 任务句柄
     * @return 任务结果数据
     */
    public static native byte[] getTaskResult(long handle);
}
