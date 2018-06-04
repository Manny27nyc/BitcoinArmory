////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <atomic>
#include <future>
#include <string>
#include <thread>

#include "libwebsockets.h"
#include "ThreadSafeClasses.h"
#include "BinaryData.h"
#include "DataObject.h"
#include "SocketObject.h"
#include "WebSocketMessage.h"
#include "BlockDataManagerConfig.h"
#include "ClientClasses.h"
#include "AsyncClient.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
struct WriteAndReadPacket
{
   const unsigned id_;
   WebSocketMessage response_;
   shared_ptr<Socket_ReadPayload> payload_;

   WriteAndReadPacket(unsigned id, shared_ptr<Socket_ReadPayload> payload) :
      id_(id), payload_(payload)
   {}

   ~WriteAndReadPacket(void)
   {}
};

////////////////////////////////////////////////////////////////////////////////
enum client_protocols {
   PROTOCOL_ARMORY_CLIENT,

   /* always last */
   CLIENT_PROTOCOL_COUNT
};

struct per_session_data__client {
   static const unsigned rcv_size = 8000;
};

namespace SwigClient
{
   class PythonCallback;
}

////////////////////////////////////////////////////////////////////////////////
class WebSocketClient : public SocketPrototype
{
private:
   atomic<void*> wsiPtr_;
   atomic<void*> contextPtr_;
   unique_ptr<promise<bool>> ctorProm_ = nullptr;

   Stack<BinaryData> writeQueue_;
   BlockingStack<BinaryData> readQueue_;
   atomic<unsigned> run_;
   thread serviceThr_, readThr_;
   TransactionalMap<uint64_t, shared_ptr<WriteAndReadPacket>> readPackets_;
   RemoteCallback* callbackPtr_ = nullptr;
   
   static TransactionalMap<struct lws*, shared_ptr<WebSocketClient>> objectMap_; 

private:
   WebSocketClient(const string& addr, const string& port) :
      SocketPrototype(addr, port, false)
   {
      wsiPtr_.store(nullptr, memory_order_relaxed);
      contextPtr_.store(nullptr, memory_order_relaxed);
      init();
   }

   void init();
   void setIsReady(bool);
   void readService(void);
   void service(void);

public:
   ~WebSocketClient()
   {
      shutdown();
   }

   //locals
   void shutdown(void);   
   void setCallback(RemoteCallback*);

   //virtuals
   SocketType type(void) const { return SocketWS; }
   void pushPayload(
      Socket_WritePayload&, shared_ptr<Socket_ReadPayload>);
   bool connectToRemote(void);

   //statics
   static shared_ptr<WebSocketClient> getNew(
      const string& addr, const string& port);

   static int callback(
      struct lws *wsi, enum lws_callback_reasons reason, 
      void *user, void *in, size_t len);

   static shared_ptr<WebSocketClient> getInstance(struct lws* ptr);
   static void destroyInstance(struct lws* ptr);
};

#endif