#ifndef BAKTSIU_TEXTURE_POOL_H_
#define BAKTSIU_TEXTURE_POOL_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include "texture.h"

namespace baktsiu
{

/**
 * Texture pool to manage file loading and pixel uploading tasks.
 *
 * Each image would be first loaded to memory by a worker thread, then 
 * main (GL) thread will invoke upload() to transfer textures to GPU.
 */
class TexturePool
{
public:
    TexturePool() = default;

    // Setup worker threads to handle file loading tasks.
    void    initialize(int workerNum);

    void    release();
    
    // Clear textures that no others reference.
    void    cleanUnusedTextures();

    /** 
     * Upload binary blob of textures to GPU.
     *
     * @return A list of uploaded textures.
     */
    TextureList upload();

    /**
     * Acquire texture from image filepath.
     *
     * @param filepath The filepath of image file.
     * @return A shared pointer of reference texture.
     */
    TextureSPtr acquireTexture(const std::string& filepath);

    // Whether there are textures waiting for uploading.
    bool    hasNoPendingTasks() const;

private:
    // The handling function for each worker thread.
    void    processImportTasks();

private:
    using LoadRequest = std::tuple<std::string, TextureSPtr>;

    TextureList                 mTextureList;

    std::deque<LoadRequest>     mLoadRequestQueue;
    std::deque<TextureSPtr>     mUploadTaskQueue;
    std::mutex                  mLoadMutex;
    std::mutex                  mUploadMutex;
    std::condition_variable     mConditionVar;
    std::atomic<int>            mImportRequestNum = { 0 };
    std::vector<std::thread>    mWorkers;

    bool    mAboutToTerminate = false;
};

}  // namespace baktsiu
#endif