#ifndef _BUFFER_POOL_H_
#define _BUFFER_POOL_H_

#include <iostream>
#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>

#include "srsran/srsran.h"

#ifdef __cplusplus
extern "C" {
#endif



class BufferPool
{
public:
    BufferPool( );
    BufferPool(uint32_t nof_buffer, uint32_t nof_prb);
    ~BufferPool();

    cf_t** getBuffer();

    void returnBuffer(cf_t** buffer);
private:
    std::queue<cf_t**> sfBuffers;

    cf_t** createBuffer();

    const uint32_t nof_buffer;
    const uint32_t nof_prb;
};


#ifdef __cplusplus
}
#endif

#endif