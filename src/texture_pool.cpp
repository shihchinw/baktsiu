#include "texture_pool.h"

namespace baktsiu
{

void    TexturePool::initialize(int workerNum)
{
    mWorkers.clear();

    for (int i = 0; i < workerNum; ++i) {
        mWorkers.push_back(std::thread(&TexturePool::processImportTasks, this));
    }
}

void    TexturePool::release()
{
    {
        const std::lock_guard<std::mutex> lock(mLoadMutex);
        mAboutToTerminate = true;
        mConditionVar.notify_all();
    }
    
    for (auto& worker : mWorkers) {
        worker.join();
    }
}

void    TexturePool::cleanUnusedTextures()
{
    for (int idx = static_cast<int>(mTextureList.size()) - 1; idx >= 0; --idx) {
        if (mTextureList[idx].use_count() == 1) {
            mTextureList.erase(mTextureList.begin() + idx);
        }
    }
}

// Upload texture content to GPU. This function should be executed in main thread (with GL context).
TextureList    TexturePool::upload()
{
    TextureList newTextureList;
    {
        std::unique_lock<std::mutex> lock(mUploadMutex);
        std::move(mUploadTaskQueue.begin(), mUploadTaskQueue.end(), std::back_inserter(newTextureList));
        mUploadTaskQueue.clear();
    }

    for (auto& newTexture : newTextureList) {
        newTexture->upload();
        --mImportRequestNum;
    }

    return newTextureList;
}

bool    TexturePool::hasNoPendingTasks() const
{
    return mImportRequestNum == 0;
}

TextureSPtr TexturePool::acquireTexture(const std::string& filepath)
{
    // The size of mTextureList is usually less than 100, thus we
    // use linear search instead of using std::map.
    for (auto& texture : mTextureList) {
        if (texture->filepath() == filepath) {
            return texture;
        }
    }

    TextureSPtr newTexture = std::make_shared<Texture>();

    {
        // Create a load request and append to queue.
        const std::lock_guard<std::mutex> lock(mLoadMutex);
        LoadRequest loadRequest = std::make_tuple(filepath, newTexture);
        mLoadRequestQueue.push_back(loadRequest);

        mTextureList.push_back(newTexture);
    }

    mConditionVar.notify_one(); // Wake up one worker to load image.

    return newTexture;
}

// This function is executed in each worker thread. It mainly decodes image 
// to internal buffer and push entity to mUploadTaskQueue, then the main GL 
// render thread would upload texture to GPU at its next tick.
void    TexturePool::processImportTasks()
{
    while (true) {

        bool keepProcessing = false;

        LoadRequest loadRequest;

        {
            std::unique_lock<std::mutex> lock(mLoadMutex);

            mConditionVar.wait(lock, [this]() {
                return !mLoadRequestQueue.empty() || mAboutToTerminate;
            });

            if (mAboutToTerminate) {
                break;
            }

            loadRequest = mLoadRequestQueue.front();
            mLoadRequestQueue.pop_front();

            keepProcessing = mLoadRequestQueue.empty();
        }

        const std::string& imagePath = std::get<0>(loadRequest);

        ScopeMarker((std::string("Load texture") + imagePath).c_str());
        auto& newTexture = std::get<1>(loadRequest);
        if (!newTexture->loadFromFile(imagePath)) {
            LOGE("Failed to load texture {}", imagePath);
            continue;
        }

        ++mImportRequestNum;
        std::unique_lock<std::mutex> lock(mUploadMutex);
        mUploadTaskQueue.push_back(std::move(newTexture));

        if (keepProcessing) {
            mConditionVar.notify_one();
        }
    }
}

} // namespace baktsiu