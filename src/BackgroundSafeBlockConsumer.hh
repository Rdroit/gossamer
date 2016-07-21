#ifndef BACKGROUNDSAFEBLOCKCONSUMER_HH
#define BACKGROUNDSAFEBLOCKCONSUMER_HH

#ifndef BOUNDEDQUEUE_HH
#include "BoundedQueue.hh"
#endif

template <typename Consumer>
class BackgroundSafeBlockConsumer
{
public:
    typedef typename Consumer::value_type value_type;
    typedef std::vector<value_type> block_type;
    typedef boost::shared_ptr<block_type> block_ptr_type;

    class ConsBlockWorker
    {
    public:
        void operator()()
        {
            block_ptr_type blkPtr;
            while (mQueue.get(blkPtr))
            {
                const block_type& blk(*blkPtr);
                for (uint64_t i = 0; i < blk.size(); ++i)
                {
                    mCons.push_back(blk[i]);
                }
            }
        }

        ConsBlockWorker(BoundedQueue<block_ptr_type>& pQueue, Consumer& pCons)
            : mQueue(pQueue), mCons(pCons)
        {
        }

    private:
        BoundedQueue<block_ptr_type>& mQueue;
        Consumer& mCons;
    };

    void push_back(const value_type& pVal)
    {
        boost::unique_lock<boost::mutex> lock(mMutex);
        mCurrBlock->push_back(pVal);
        if (mCurrBlock->size() == mBlkSize)
        {
            mQueue.put(mCurrBlock);
            mCurrBlock = block_ptr_type(new block_type);
            mCurrBlock->reserve(mBlkSize);
        }
    }

    void end()
    {
        if (mFinished)
        {
            return;
        }
        mFinished = true;
        if (mCurrBlock->size())
        {
            mQueue.put(mCurrBlock);
            mCurrBlock = block_ptr_type();
        }
        mQueue.finish();
    }

    void wait()
    {
        end();
        mThread.join();
        mJoined = true;
    }

    PropertyTree stat() const
    {
        PropertyTree t;
        t.putSub("queue", mQueue.stat());
        return t;
    }

    BackgroundSafeBlockConsumer(Consumer& pCons, uint64_t pNumBufItems, uint64_t pBlkSize)
        : mBlkSize(pBlkSize), mQueue(pNumBufItems), mCons(mQueue, pCons),
          mThread(mCons), mFinished(false), mJoined(false)
    {
        mCurrBlock = block_ptr_type(new block_type);
        mCurrBlock->reserve(mBlkSize);
    }

    ~BackgroundSafeBlockConsumer()
    {
        if (!mFinished)
        {
            end();
        }
        if (!mJoined)
        {
            mThread.join();
        }
    }

private:
    const uint64_t mBlkSize;
    boost::mutex mMutex;
    BoundedQueue<block_ptr_type> mQueue;
    block_ptr_type mCurrBlock;
    ConsBlockWorker mCons;
    boost::thread mThread;
    bool mFinished;
    bool mJoined;
};

#endif // BACKGROUNDSAFEBLOCKCONSUMER_HH