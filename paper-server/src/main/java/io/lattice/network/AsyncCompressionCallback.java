package io.lattice.network;

/**
 * 异步压缩回调接口
 * 用于处理异步压缩操作的结果
 */
public interface AsyncCompressionCallback {
    
    /**
     * 压缩成功时调用
     * @param compressedSize 压缩后的数据大小
     */
    void onSuccess(long compressedSize);
    
    /**
     * 压缩失败时调用
     * @param errorCode 错误代码
     */
    void onError(int errorCode);
}